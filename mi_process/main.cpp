#include "json_parser.h"
#include "mesh_io.h"
#include "volume.h"

#include <cmath>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

namespace fs = std::filesystem;
using JsonValue = mi::json::Value;
using VolumePtr = std::shared_ptr<mi::Volume>;

const JsonValue *member(const JsonValue &object, const char *key) {
	if(object.type != JsonValue::Type::Object)
		return nullptr;
	const auto found = object.object.find(key);
	return found == object.object.end() ? nullptr : &found->second;
}

std::string requiredString(const JsonValue &object, const char *key) {
	const JsonValue *value = member(object, key);
	if(!value || value->type != JsonValue::Type::String || value->string.empty())
		throw std::runtime_error(std::string("Missing or invalid string field: ") + key);
	return value->string;
}

fs::path resolvePath(const fs::path &base, const std::string &path) {
	fs::path result(path);
	if(result.is_relative())
		result = base / result;
	return result.lexically_normal();
}

std::string pathKey(const fs::path &path) {
	return fs::absolute(path).lexically_normal().generic_string();
}

struct Transform {
	double matrix[4][4] = {
		{1.0, 0.0, 0.0, 0.0},
		{0.0, 1.0, 0.0, 0.0},
		{0.0, 0.0, 1.0, 0.0},
		{0.0, 0.0, 0.0, 1.0}
	};
};

Transform worldTransform(const JsonValue &entity) {
	Transform result;
	const JsonValue *matrix = member(entity, "matrix_world");
	if(!matrix)
		return result;
	if(matrix->type != JsonValue::Type::Array || matrix->array.size() != 4)
		throw std::runtime_error("matrix_world must be a 4x4 array");

	for(size_t row = 0; row < 4; ++row) {
		if(matrix->array[row].type != JsonValue::Type::Array ||
		   matrix->array[row].array.size() != 4)
			throw std::runtime_error("matrix_world must be a 4x4 array");
		for(size_t column = 0; column < 4; ++column) {
			const JsonValue &entry = matrix->array[row].array[column];
			if(entry.type != JsonValue::Type::Number || !std::isfinite(entry.number))
				throw std::runtime_error("matrix_world contains an invalid number");
			result.matrix[row][column] = entry.number;
		}
	}
	return result;
}

void applyTransform(std::vector<Vec3f> &vertices, const Transform &transform) {
	for(Vec3f &vertex : vertices) {
		const double input[4] = {vertex[0], vertex[1], vertex[2], 1.0};
		double output[4] = {};
		for(size_t row = 0; row < 4; ++row)
			for(size_t column = 0; column < 4; ++column)
				output[row] += transform.matrix[row][column] * input[column];
		if(!std::isfinite(output[3]) || output[3] == 0.0)
			throw std::runtime_error("matrix_world maps a vertex to an invalid position");
		for(int axis = 0; axis < 3; ++axis) {
			const double coordinate = output[axis] / output[3];
			const float converted = static_cast<float>(coordinate);
			if(!std::isfinite(converted))
				throw std::runtime_error("matrix_world produces a non-finite vertex");
			vertex[axis] = converted;
		}
	}
}

std::string sourceReference(const JsonValue &entity, bool allowSnapshotFallback) {
	const JsonValue *source = member(entity, "source");
	if(source) {
		if(source->type != JsonValue::Type::String || source->string.empty())
			throw std::runtime_error("source must be a non-empty string");
		return source->string;
	}
	if(allowSnapshotFallback)
		return requiredString(entity, "obj_snapshot");
	throw std::runtime_error("Missing string field: source");
}

VolumePtr loadOperand(const JsonValue &entity, bool allowSnapshotFallback,
					  const fs::path &base, float step,
					  const std::set<std::string> &snapshotKeys,
					  const std::map<std::string, VolumePtr> &results,
					  const std::string &outputKey) {
	const fs::path sourcePath = resolvePath(base, sourceReference(entity, allowSnapshotFallback));
	const std::string sourceKey = pathKey(sourcePath);
	const auto result = results.find(sourceKey);
	if(result != results.end())
		return result->second;
	if(snapshotKeys.count(sourceKey))
		throw std::runtime_error("Referenced operation snapshot has not been produced: " +
								 sourcePath.string());
	if(sourceKey == outputKey)
		throw std::runtime_error("Output path would overwrite an input OBJ: " +
								 sourcePath.string());

	std::vector<Vec3f> vertices;
	std::vector<int> faces;
	loadObj(sourcePath.string().c_str(), vertices, faces);
	if(vertices.empty() || faces.empty() || faces.size() % 3 != 0)
		throw std::runtime_error("OBJ must contain vertices and triangularized faces: " +
								 sourcePath.string());
	for(const Vec3f &vertex : vertices)
		for(int axis = 0; axis < 3; ++axis)
			if(!std::isfinite(vertex[axis]))
				throw std::runtime_error("OBJ contains a non-finite vertex: " +
										 sourcePath.string());
	for(int index : faces)
		if(index < 0 || static_cast<size_t>(index) >= vertices.size())
			throw std::runtime_error("OBJ contains a face index outside the vertex array: " +
									 sourcePath.string());

	applyTransform(vertices, worldTransform(entity));
	VolumePtr volume = std::make_shared<mi::Volume>();
	volume->fromMesh(vertices, faces, step);
	return volume;
}

JsonValue readOperations(const fs::path &path) {
	std::ifstream input(path);
	if(!input)
		throw std::runtime_error("Could not open operations JSON: " + path.string());
	std::ostringstream contents;
	contents << input.rdbuf();
	if(input.bad())
		throw std::runtime_error("Could not read operations JSON: " + path.string());

	JsonValue document;
	if(!mi::json::Parser(contents.str()).parse(document))
		throw std::runtime_error("Invalid operations JSON: " + path.string());
	if(document.type != JsonValue::Type::Array || document.array.empty())
		throw std::runtime_error("Operations JSON must be a non-empty array");
	return document;
}

bool hasExtension(const fs::path &path, const std::string &extension) {
	std::string actual = path.extension().string();
	for(char &character : actual)
		character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
	return actual == extension;
}

void writeObj(const fs::path &path, const std::vector<Vec3f> &vertices,
			  const std::vector<int> &faces, bool dual) {
	const size_t faceSize = dual ? 4 : 3;
	if(faces.size() % faceSize != 0)
		throw std::runtime_error("Mesher returned an invalid face array");
	for(int index : faces)
		if(index < 0 || static_cast<size_t>(index) >= vertices.size())
			throw std::runtime_error("Mesher returned an invalid face index");

	std::ofstream output(path);
	if(!output)
		throw std::runtime_error("Could not open output OBJ: " + path.string());
	output << std::setprecision(std::numeric_limits<float>::max_digits10);
	for(const Vec3f &vertex : vertices)
		output << "v " << vertex[0] << ' ' << vertex[1] << ' ' << vertex[2] << '\n';
	for(size_t i = 0; i < faces.size(); i += faceSize) {
		output << 'f';
		for(size_t j = 0; j < faceSize; ++j)
			output << ' ' << faces[i + j] + 1;
		output << '\n';
	}
	if(!output)
		throw std::runtime_error("Could not write output OBJ: " + path.string());
}

void printUsage(const char *program) {
	std::cerr << "Usage: " << program
			  << " -s step [-d | -m] <operations.json> <output.obj>\n"
			  << "  -s step  Voxel spacing used for all input meshes.\n"
			  << "  -d       Export a dual mesh (default).\n"
			  << "  -m       Export a primal mesh.\n";
}

int runOperations(const fs::path &jsonPath, const fs::path &outputPath,
				  float step, bool dual) {
	const fs::path absoluteJsonPath = fs::absolute(jsonPath).lexically_normal();
	const fs::path base = absoluteJsonPath.parent_path();
	const fs::path absoluteOutputPath = fs::absolute(outputPath).lexically_normal();
	const std::string outputKey = pathKey(absoluteOutputPath);
	if(pathKey(absoluteJsonPath) == outputKey)
		throw std::runtime_error("Input JSON and output OBJ must be different files");

	const JsonValue document = readOperations(absoluteJsonPath);
	std::set<std::string> snapshotKeys;
	for(const JsonValue &operation : document.array) {
		const JsonValue *target = member(operation, "target");
		if(!target)
			throw std::runtime_error("Operation is missing its target");
		const std::string snapshot = requiredString(*target, "obj_snapshot");
		if(!snapshotKeys.insert(pathKey(resolvePath(base, snapshot))).second)
			throw std::runtime_error("Operation snapshots must have unique names: " + snapshot);
	}
	if(snapshotKeys.count(outputKey))
		throw std::runtime_error("Output path conflicts with an operation snapshot");

	std::map<std::string, VolumePtr> results;
	VolumePtr finalResult;
	for(size_t i = 0; i < document.array.size(); ++i) {
		const JsonValue &operation = document.array[i];
		const std::string type = requiredString(operation, "operation");
		const JsonValue *targetRecord = member(operation, "target");
		const JsonValue *toolRecord = member(operation, "tool");
		if(!targetRecord || !toolRecord)
			throw std::runtime_error("Operation " + std::to_string(i + 1) +
									 " must contain target and tool objects");

		const VolumePtr target = loadOperand(*targetRecord, false, base, step,
											 snapshotKeys, results, outputKey);
		const VolumePtr tool = loadOperand(*toolRecord, true, base, step,
										   snapshotKeys, results, outputKey);
		VolumePtr result = std::make_shared<mi::Volume>();
		if(type == "DIFFERENCE")
			result->subtraction(*target, *tool);
		else if(type == "UNION")
			result->unification(*target, *tool);
		else if(type == "INTERSECTION" || type == "INTERSECT")
			result->intersection(*target, *tool);
		else
			throw std::runtime_error("Unsupported boolean operation: " + type);
		if(!result->checkConsistency())
			throw std::runtime_error("Boolean operation " + std::to_string(i + 1) +
									 " produced an inconsistent volume");

		const std::string snapshot = requiredString(*targetRecord, "obj_snapshot");
		results.emplace(pathKey(resolvePath(base, snapshot)), result);
		finalResult = result;
	}

	std::vector<Vec3f> vertices;
	std::vector<int> faces;
	if(dual)
		finalResult->toMeshDual(vertices, faces);
	else
		finalResult->toMesh(vertices, faces);
	writeObj(absoluteOutputPath, vertices, faces, dual);
	return 0;
}

} // namespace

int main(int argc, char *argv[]) {
	bool hasStep = false;
	bool hasMeshOption = false;
	bool dual = true;
	float step = 0.0f;
	std::vector<std::string> files;

	for(int i = 1; i < argc; ++i) {
		const std::string argument(argv[i]);
		if(argument == "-d" || argument == "-m") {
			if(hasMeshOption) {
				std::cerr << "Specify only one of -d or -m.\n";
				return 2;
			}
			hasMeshOption = true;
			dual = argument == "-d";
		} else if(argument == "-s") {
			if(hasStep || i + 1 >= argc) {
				printUsage(argv[0]);
				return 2;
			}
			char *end = nullptr;
			step = std::strtof(argv[++i], &end);
			if(end == argv[i] || *end != '\0' || !std::isfinite(step) || step <= 0.0f) {
				std::cerr << "Step must be a positive finite number.\n";
				return 2;
			}
			hasStep = true;
		} else if(!argument.empty() && argument[0] == '-') {
			printUsage(argv[0]);
			return 2;
		} else {
			files.push_back(argument);
		}
	}

	if(!hasStep || files.size() != 2) {
		printUsage(argv[0]);
		return 2;
	}
	if(!hasExtension(files[0], ".json") || !hasExtension(files[1], ".obj")) {
		std::cerr << "Input must be an operations JSON file and output must be an OBJ file.\n";
		return 2;
	}

	try {
		return runOperations(files[0], files[1], step, dual);
	} catch(const std::string &error) {
		std::cerr << error << '\n';
	} catch(const std::exception &error) {
		std::cerr << error.what() << '\n';
	}
	return 1;
}
