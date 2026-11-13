#include "volume_io.h"
#include "json_parser.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace mi {

namespace {

using JsonParser = json::Parser;
using JsonValue = json::Value;

struct ParsedVolume {
	float step = 0.0f;
	int box[6] = {};
	int dimensions[3] = {};
	bool hasStep = false;
	bool hasBox = false;
	bool hasDimensions = false;
	std::vector<std::vector<float>> lines[3];
	std::vector<bool> linePresent[3];
};

const int planeUAxis[3] = {1, 2, 0};
const int planeVAxis[3] = {2, 0, 1};

void initializeParsedLines(ParsedVolume &parsed) {
	if(!parsed.hasDimensions)
		throw std::runtime_error("volume dimensions are missing");
	for(int axis = 0; axis < 3; ++axis) {
		if(parsed.dimensions[axis] <= 0)
			throw std::runtime_error("box has a non-positive dimension");
		const size_t width = static_cast<size_t>(parsed.dimensions[planeUAxis[axis]]);
		const size_t height = static_cast<size_t>(parsed.dimensions[planeVAxis[axis]]);
		if(width > std::numeric_limits<size_t>::max() / height)
			throw std::runtime_error("plane dimensions overflow addressable memory");
		const size_t count = width * height;
		if(count > static_cast<size_t>(std::numeric_limits<int>::max()))
			throw std::runtime_error("plane contains too many lines");
		parsed.lines[axis].resize(count);
		parsed.linePresent[axis].assign(count, false);
	}
}

void putLine(ParsedVolume &parsed, int axis, int row, int column,
			 const std::vector<float> &values, bool coordinatesAreUV) {
	if(axis < 0 || axis > 2 || values.size() % 2 != 0)
		throw std::runtime_error("line has an invalid plane or odd intersection count");
	const int width = parsed.dimensions[planeUAxis[axis]];
	const int height = parsed.dimensions[planeVAxis[axis]];
	const int u = coordinatesAreUV ? row : column;
	const int v = coordinatesAreUV ? column : row;
	if(u < 0 || u >= width || v < 0 || v >= height)
		throw std::runtime_error("line coordinates are outside the volume dimensions");
	const size_t index = static_cast<size_t>(v) * width + u;
	if(parsed.linePresent[axis][index])
		throw std::runtime_error("volume contains a duplicate line");
	float previous = -std::numeric_limits<float>::infinity();
	for(float value : values) {
		if(!std::isfinite(value) || value < previous)
			throw std::runtime_error("intersection positions must be finite and sorted");
		previous = value;
	}
	parsed.lines[axis][index] = values;
	parsed.linePresent[axis][index] = true;
}

void makeGridBox(const int boxValues[6], Box3i &box, int dimensions[3]) {
	Pos3i gridMin;
	Pos3i gridMax;
	for(int axis = 0; axis < 3; ++axis) {
		gridMin[axis] = boxValues[axis];
		gridMax[axis] = boxValues[axis + 3];
		const long long dimension = static_cast<long long>(gridMax[axis]) - gridMin[axis];
		if(dimension <= 0 || dimension > std::numeric_limits<int>::max())
			throw std::runtime_error("box endpoints must define positive representable dimensions");
		dimensions[axis] = static_cast<int>(dimension);
	}
	if(dimensions[0] > std::numeric_limits<int>::max() / dimensions[1])
		throw std::runtime_error("volume plane area exceeds supported size");
	const long long planeArea = static_cast<long long>(dimensions[0]) * dimensions[1];
	if(planeArea > std::numeric_limits<int>::max() / dimensions[2])
		throw std::runtime_error("volume size exceeds supported range");
	box = Box3i(gridMin, gridMax);
}

void installParsedVolume(const ParsedVolume &parsed, Volume &destination) {
	if(!parsed.hasStep || !parsed.hasBox || !parsed.hasDimensions ||
	   !std::isfinite(parsed.step) || parsed.step <= 0.0f)
		throw std::runtime_error("volume metadata is incomplete or has an invalid step");
	Box3i gridBox;
	int dimensions[3];
	makeGridBox(parsed.box, gridBox, dimensions);

	Volume loaded(gridBox);
	loaded.step = parsed.step;
	for(int axis = 0; axis < 3; ++axis) {
		const std::vector<std::vector<float>> &lines = parsed.lines[axis];
		std::vector<int> indices(lines.size() + 1, 0);
		std::vector<Intersection> intersections;
		size_t total = 0;
		for(size_t i = 0; i < lines.size(); ++i) {
			if(lines[i].size() > static_cast<size_t>(std::numeric_limits<int>::max()) - total)
				throw std::runtime_error("plane intersection count exceeds supported range");
			total += lines[i].size();
			indices[i + 1] = static_cast<int>(total);
		}
		intersections.reserve(total);
		for(const std::vector<float> &line : lines) {
			for(float value : line) {
				if(!std::isfinite(value))
					throw std::runtime_error("volume contains a non-finite intersection position");
#if STORE_NORMALS
				intersections.push_back(Intersection(value, Vec3f(0, 0, 0)));
#else
				intersections.push_back(Intersection(value));
#endif
			}
		}
		loaded.setPlaneIndices(axis, indices);
		loaded.setPlaneIntersections(axis, intersections);
	}
	destination = loaded;
}

const JsonValue *member(const JsonValue &value, const char *key) {
	if(value.type != JsonValue::Type::Object) return nullptr;
	const auto found = value.object.find(key);
	return found == value.object.end() ? nullptr : &found->second;
}

bool jsonNumber(const JsonValue *value, double &number) {
	if(!value || value->type != JsonValue::Type::Number) return false;
	number = value->number;
	return std::isfinite(number);
}

bool jsonInteger(const JsonValue *value, int &integer) {
	double number;
	if(!jsonNumber(value, number) || std::floor(number) != number ||
	   number < std::numeric_limits<int>::min() || number > std::numeric_limits<int>::max())
		return false;
	integer = static_cast<int>(number);
	return true;
}

} // namespace

bool VolumeIo::saveJSON(const std::string &path) const {
	if(volume.box.IsNull() || !std::isfinite(volume.step) || volume.step <= 0.0f)
		return false;

	const int dimensions[3] = {volume.box.DimX(), volume.box.DimY(), volume.box.DimZ()};

	std::ostringstream json;
	json << std::setprecision(std::numeric_limits<float>::max_digits10);
	json << "{\n  \"version\": 1.0,\n  \"step\": " << volume.step
		<< ",\n  \"box\": [";
	for(int axis = 0; axis < 3; ++axis) {
		if(axis) json << ", ";
		json << volume.box.min[axis];
	}
	for(int axis = 0; axis < 3; ++axis) {
		json << ", " << volume.box.max[axis];
	}

	json << "],\n  \"planes\": [\n";

	// Each plane's local axes are cyclically ordered around the axis normal:
	// axis 0 -> (y,z,x), axis 1 -> (z,x,y), axis 2 -> (x,y,z).
	for(int axis = 0; axis < 3; ++axis) {
		const int width = dimensions[planeUAxis[axis]];
		const int height = dimensions[planeVAxis[axis]];
		const std::vector<int> indices = volume.getPlaneIndices(axis);
		const std::vector<Intersection> intersections = volume.getPlaneIntersections(axis);
		if(indices.size() != static_cast<size_t>(width) * height + 1 ||
		   indices.empty() || indices.back() != static_cast<int>(intersections.size()))
			return false;

		if(axis) json << ",\n";
		json << "    {\n      \"axis\": " << axis << ",\n      \"lines\": [";
		bool firstLine = true;
		for(int v = 0; v < height; ++v) {
			for(int u = 0; u < width; ++u) {
				const size_t lineIndex = static_cast<size_t>(u) + static_cast<size_t>(v) * width;
				const int begin = indices[lineIndex];
				const int end = indices[lineIndex + 1];
				if(begin < 0 || end < begin || end > static_cast<int>(intersections.size()))
					return false;
				if(begin == end)
					continue;

				if(firstLine) json << '\n';
				else json << ",\n";
				firstLine = false;
				json << "        [" << u << ", " << v << ", [";
				for(int i = begin; i < end; ++i) {
					if(i > begin) json << ", ";
					const float position = intersections[i].p;
					if(!std::isfinite(position))
						return false;
					json << position;
				}
				json << "]]";
			}
		}
		if(!firstLine) json << '\n';
		json << "      ]\n    }";
	}
	json << "\n  ]\n}\n";
	if(!json.good())
		return false;

	std::ofstream output(path.c_str(), std::ios::out | std::ios::trunc);
	if(!output)
		return false;
	output << json.str();
	return output.good();
}

bool VolumeIo::saveText(const std::string &path) const {
	if(volume.box.IsNull() || !std::isfinite(volume.step) || volume.step <= 0.0f)
		return false;

	const int dimensions[3] = {volume.box.DimX(), volume.box.DimY(), volume.box.DimZ()};

	std::ostringstream text;
	text << std::setprecision(std::numeric_limits<float>::max_digits10);
	text << "# MI_FORMAT_VERSION 1.0\n"
		 << "STEP " << volume.step << '\n'
		 << "# Integer grid box: min_x min_y min_z max_x max_y max_z\n"
		 << "BOX";
	for(int axis = 0; axis < 3; ++axis) {
		text << ' ' << volume.box.min[axis];
	}
	for(int axis = 0; axis < 3; ++axis) {
		text << ' ' << volume.box.max[axis];
	}

	text << "\n\n# Plane Data (e.g., Axis 0 = X Plane, Axis 1 = Y, Axis 2 = Z)\n"
		 << "# Format per line: row column [intersection positions in grid coordinates...]\n";

	for(int axis = 0; axis < 3; ++axis) {
		const int width = dimensions[planeUAxis[axis]];
		const int height = dimensions[planeVAxis[axis]];
		const std::vector<int> indices = volume.getPlaneIndices(axis);
		const std::vector<Intersection> intersections = volume.getPlaneIntersections(axis);
		if(indices.size() != static_cast<size_t>(width) * height + 1 ||
		   indices.empty() || indices.back() != static_cast<int>(intersections.size()))
			return false;

		text << "PLANE " << axis << '\n';
		for(int v = 0; v < height; ++v) {
			for(int u = 0; u < width; ++u) {
				const size_t lineIndex = static_cast<size_t>(u) + static_cast<size_t>(v) * width;
				const int begin = indices[lineIndex];
				const int end = indices[lineIndex + 1];
				if(begin < 0 || end < begin || end > static_cast<int>(intersections.size()))
					return false;
				if(begin == end)
					continue;

				text << "LINE " << v << ' ' << u;
				for(int i = begin; i < end; ++i) {
					const float position = intersections[i].p;
					if(!std::isfinite(position))
						return false;
					text << ' ' << position;
				}
				text << '\n';
			}
		}
		text << "END_PLANE\n\n";
	}
	if(!text.good())
		return false;

	std::ofstream output(path.c_str(), std::ios::out | std::ios::trunc);
	if(!output)
		return false;
	output << text.str();
	return output.good();
}

bool VolumeIo::saveBin(const std::string &path) const {
	if(volume.box.IsNull() || !std::isfinite(volume.step) || volume.step <= 0.0f)
		return false;

	const int dimensions[3] = {
		volume.box.DimX(),
		volume.box.DimY(),
		volume.box.DimZ()
	};
	for(int axis = 0; axis < 3; ++axis)
		if(dimensions[axis] <= 0)
			return false;

	// Validate all records before creating/truncating the destination file.
	for(int axis = 0; axis < 3; ++axis) {
		const int width = dimensions[planeUAxis[axis]];
		const int height = dimensions[planeVAxis[axis]];
		const std::vector<int> indices = volume.getPlaneIndices(axis);
		const std::vector<Intersection> intersections = volume.getPlaneIntersections(axis);
		if(indices.size() != static_cast<size_t>(width) * height + 1 ||
		   indices.empty() || indices.front() != 0 ||
		   indices.back() != static_cast<int>(intersections.size()))
			return false;

		for(int row = 0; row < height; ++row) {
			for(int column = 0; column < width; ++column) {
				const size_t lineIndex = static_cast<size_t>(column) + static_cast<size_t>(row) * width;
				const int begin = indices[lineIndex];
				const int end = indices[lineIndex + 1];
				if(begin < 0 || end < begin || end > static_cast<int>(intersections.size()) ||
				   (end - begin) % 2 != 0)
					return false;
				for(int i = begin; i < end; ++i) {
					const float position = intersections[i].p;
					if(!std::isfinite(position))
						return false;
				}
			}
		}
	}

	const float step = volume.step;
	int32_t box[6];
	if(!std::isfinite(step) || step <= 0.0f)
		return false;
	for(int axis = 0; axis < 3; ++axis) {
		box[axis] = static_cast<int32_t>(volume.box.min[axis]);
		box[axis + 3] = static_cast<int32_t>(volume.box.max[axis]);
	}

	// Raw values in host byte order: one IEEE-754 step float, six int32 box
	// coordinates (min XYZ, max XYZ), then
	// 5-float segment records until EOF (axis, row, column, start, end), with
	// start/end stored as absolute grid-coordinate positions.
	std::ofstream output(path.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);
	if(!output)
		return false;
	output.write(reinterpret_cast<const char *>(&step), sizeof(step));
	output.write(reinterpret_cast<const char *>(box), sizeof(box));
	if(!output)
		return false;

	for(int axis = 0; axis < 3; ++axis) {
		const int width = dimensions[planeUAxis[axis]];
		const int height = dimensions[planeVAxis[axis]];
		const std::vector<int> indices = volume.getPlaneIndices(axis);
		const std::vector<Intersection> intersections = volume.getPlaneIntersections(axis);
		for(int row = 0; row < height; ++row) {
			for(int column = 0; column < width; ++column) {
				const size_t lineIndex = static_cast<size_t>(column) + static_cast<size_t>(row) * width;
				const int begin = indices[lineIndex];
				const int end = indices[lineIndex + 1];
				for(int i = begin; i < end; i += 2) {
					const float record[5] = {
						static_cast<float>(axis),
						static_cast<float>(row),
						static_cast<float>(column),
						intersections[i].p,
						intersections[i + 1].p
					};
					output.write(reinterpret_cast<const char *>(record), sizeof(record));
					if(!output)
						return false;
				}
			}
		}
	}
	return output.good();
}

bool VolumeIo::loadJSON(const std::string &path, Volume &destination) const {
	std::ifstream input(path.c_str(), std::ios::in | std::ios::binary);
	if(!input) throw std::runtime_error("cannot open JSON volume '" + path + "'");
	std::ostringstream contents;
	contents << input.rdbuf();
	if(!input.good() && !input.eof())
		throw std::runtime_error("failed while reading JSON volume '" + path + "'");

	const std::string source = contents.str();
	JsonValue root;
	if(!JsonParser(source).parse(root) || root.type != JsonValue::Type::Object)
		throw std::runtime_error("invalid JSON syntax or root object in '" + path + "'");
	double version;
	if(!jsonNumber(member(root, "version"), version) || version != 1.0)
		throw std::runtime_error("unsupported or missing JSON volume version in '" + path + "'");

	ParsedVolume parsed;
	const JsonValue *box = member(root, "box");
	const JsonValue *planes = member(root, "planes");
	double step;
	if(!box || box->type != JsonValue::Type::Array || box->array.size() != 6 ||
	   !planes || planes->type != JsonValue::Type::Array ||
	   !jsonNumber(member(root, "step"), step) || step <= 0.0 ||
	   step > std::numeric_limits<float>::max())
		throw std::runtime_error("JSON volume requires a valid step, six-value box, and planes array");
	parsed.step = static_cast<float>(step);
	parsed.hasStep = true;
	for(size_t i = 0; i < 6; ++i) {
		if(!jsonInteger(&box->array[i], parsed.box[i]))
			throw std::runtime_error("JSON box coordinates must be integers");
	}
	parsed.hasBox = true;
	Box3i gridBox;
	makeGridBox(parsed.box, gridBox, parsed.dimensions);
	parsed.hasDimensions = true;
	initializeParsedLines(parsed);

	bool axisPresent[3] = {false, false, false};
	for(const JsonValue &plane : planes->array) {
		int axis;
		const JsonValue *lines = member(plane, "lines");
		if(!jsonInteger(member(plane, "axis"), axis) || axis < 0 || axis > 2 ||
		   axisPresent[axis] || !lines || lines->type != JsonValue::Type::Array)
			throw std::runtime_error("JSON planes require a unique axis and a lines array");
		axisPresent[axis] = true;
		for(const JsonValue &line : lines->array) {
			if(line.type != JsonValue::Type::Array || line.array.size() != 3 ||
			   line.array[2].type != JsonValue::Type::Array)
				throw std::runtime_error("JSON line must contain u, v, and an intersection array");
			int u, v;
			if(!jsonInteger(&line.array[0], u) || !jsonInteger(&line.array[1], v))
				throw std::runtime_error("JSON line coordinates must be integers");
			std::vector<float> values;
			values.reserve(line.array[2].array.size());
			for(const JsonValue &position : line.array[2].array) {
				double number;
				if(!jsonNumber(&position, number) ||
				   std::fabs(number) > std::numeric_limits<float>::max())
					throw std::runtime_error("JSON intersection positions must be finite float values");
				values.push_back(static_cast<float>(number));
			}
			// JSON line tuples use [u, v, intersections].
			putLine(parsed, axis, u, v, values, true);
		}
	}
	installParsedVolume(parsed, destination);
	return true;
}

bool VolumeIo::loadText(const std::string &path, Volume &destination) const {
	std::ifstream input(path.c_str());
	if(!input) throw std::runtime_error("cannot open text volume '" + path + "'");

	ParsedVolume parsed;
	bool dimensionsInitialized = false;
	bool hasVersion = false;
	bool inPlane = false;
	bool planePresent[3] = {false, false, false};
	int currentAxis = -1;
	std::string line;
	while(std::getline(input, line)) {
		const size_t comment = line.find('#');
		if(comment != std::string::npos) {
			std::istringstream commentText(line.substr(comment + 1));
			std::string directive;
			if(commentText >> directive && directive == "MI_FORMAT_VERSION") {
				double version;
				std::string extra;
				if(hasVersion || !(commentText >> version) || version != 1.0 || commentText >> extra)
					throw std::runtime_error("invalid or duplicate text volume version declaration");
				hasVersion = true;
			}
			line.erase(comment);
		}
		std::istringstream row(line);
		std::string tag;
		if(!(row >> tag)) continue;

		if(tag == "BOX") {
			if(parsed.hasBox) throw std::runtime_error("duplicate BOX declaration");
			for(int &coordinate : parsed.box)
				if(!(row >> coordinate)) throw std::runtime_error("BOX requires six integer coordinates");
			std::string extra;
			if(row >> extra) throw std::runtime_error("BOX has extra values");
			parsed.hasBox = true;
			Box3i gridBox;
			makeGridBox(parsed.box, gridBox, parsed.dimensions);
			parsed.hasDimensions = true;
			initializeParsedLines(parsed);
			dimensionsInitialized = true;
		} else if(tag == "STEP") {
			std::string extra;
			if(parsed.hasStep || !(row >> parsed.step) || !std::isfinite(parsed.step) ||
			   parsed.step <= 0.0f || row >> extra)
				throw std::runtime_error("STEP must appear once and contain one positive finite value");
			parsed.hasStep = true;
		} else if(tag == "PLANE") {
			std::string extra;
			if(inPlane || !dimensionsInitialized || !(row >> currentAxis) || currentAxis < 0 ||
			   currentAxis > 2 || planePresent[currentAxis] || row >> extra)
				throw std::runtime_error("PLANE has an invalid or duplicate axis, or appears before BOX");
			planePresent[currentAxis] = true;
			inPlane = true;
		} else if(tag == "END_PLANE") {
			std::string extra;
			if(!inPlane || row >> extra) throw std::runtime_error("unexpected or malformed END_PLANE");
			inPlane = false;
			currentAxis = -1;
		} else if(tag == "LINE") {
			if(!inPlane) throw std::runtime_error("LINE appears outside a plane");
			int rowIndex, columnIndex;
			if(!(row >> rowIndex >> columnIndex))
				throw std::runtime_error("LINE requires row and column coordinates");
			std::vector<float> values;
			std::string valueToken;
			while(row >> valueToken) {
				char *end = nullptr;
				const float value = std::strtof(valueToken.c_str(), &end);
				if(end == valueToken.c_str() || *end != '\0' || !std::isfinite(value))
					throw std::runtime_error("LINE contains an invalid intersection position");
				values.push_back(value);
			}
			if(!row.eof()) throw std::runtime_error("LINE contains malformed trailing data");
			putLine(parsed, currentAxis, rowIndex, columnIndex, values, false);
		} else {
			throw std::runtime_error("unknown text volume directive '" + tag + "'");
		}
	}
	if(!input.eof()) throw std::runtime_error("failed while reading text volume '" + path + "'");
	if(inPlane) throw std::runtime_error("text volume ended before END_PLANE");
	if(!hasVersion) throw std::runtime_error("text volume is missing its version declaration");
	installParsedVolume(parsed, destination);
	return true;
}

bool VolumeIo::loadBin(const std::string &path, Volume &destination) const {
	std::ifstream input(path.c_str(), std::ios::in | std::ios::binary);
	if(!input) throw std::runtime_error("cannot open binary volume '" + path + "'");
	input.seekg(0, std::ios::end);
	const std::streamoff fileBytes = input.tellg();
	constexpr size_t headerBytes = sizeof(float) + sizeof(int32_t) * 6;
	constexpr size_t recordBytes = sizeof(float) * 5;
	if(fileBytes < static_cast<std::streamoff>(headerBytes) ||
	   fileBytes % static_cast<std::streamoff>(sizeof(float)) != 0 ||
	   (fileBytes - static_cast<std::streamoff>(headerBytes)) %
	       static_cast<std::streamoff>(recordBytes) != 0 ||
	   fileBytes > std::numeric_limits<std::streamsize>::max())
		throw std::runtime_error("binary volume is truncated or has an invalid record length");

	const size_t floatCount = static_cast<size_t>(fileBytes) / sizeof(float);
	std::vector<float> data(floatCount);
	input.seekg(0, std::ios::beg);
	input.read(reinterpret_cast<char *>(data.data()), static_cast<std::streamsize>(fileBytes));
	if(input.gcount() != static_cast<std::streamsize>(fileBytes))
		throw std::runtime_error("failed to read the complete binary volume");

	const float step = data[0];
	if(!std::isfinite(step) || step <= 0.0f)
		throw std::runtime_error("binary volume step must be positive and finite");
	int32_t boxValues[6];
	std::memcpy(boxValues, data.data() + 1, sizeof(boxValues));
	Box3i gridBox;
	int dimensions[3];
	makeGridBox(boxValues, gridBox, dimensions);
	Volume loaded(gridBox);
	loaded.step = step;

	int currentAxis = 0;
	const auto getPlaneWidth = [](const int dims[3], int axis) {
		return dims[planeUAxis[axis]];
	};
	const auto getPlaneHeight = [](const int dims[3], int axis) {
		return dims[planeVAxis[axis]];
	};
	int width = getPlaneWidth(dimensions, currentAxis);
	int height = getPlaneHeight(dimensions, currentAxis);
	size_t planeLineCount = static_cast<size_t>(width) * height;
	std::vector<int> indices(planeLineCount + 1, 0);
	std::vector<Intersection> intersections;
	size_t nextLine = 0;
	auto finishPlane = [&]() {
		while(nextLine < planeLineCount) {
			indices[nextLine + 1] = static_cast<int>(intersections.size());
			++nextLine;
		}
		loaded.setPlaneData(currentAxis, std::move(indices), std::move(intersections));
	};
	auto beginPlane = [&](int axis) {
		currentAxis = axis;
		width = getPlaneWidth(dimensions, axis);
		height = getPlaneHeight(dimensions, axis);
		planeLineCount = static_cast<size_t>(width) * height;
		indices.assign(planeLineCount + 1, 0);
		intersections.clear();
		nextLine = 0;
	};

	int previousAxis = -1;
	int previousRow = -1;
	int previousColumn = -1;
	float previousStart = -std::numeric_limits<float>::infinity();
	float previousEnd = -std::numeric_limits<float>::infinity();
	const float *record = data.data() + headerBytes / sizeof(float);
	const float *recordsEnd = data.data() + data.size();
	for(; record < recordsEnd; record += 5) {
		for(int i = 0; i < 5; ++i)
			if(!std::isfinite(record[i]))
				throw std::runtime_error("binary record contains a non-finite value");

		const auto asIndex = [](float value, int &index) {
			if(value < 0.0f || value > static_cast<float>(1 << 24) || std::floor(value) != value)
				return false;
			index = static_cast<int>(value);
			return true;
		};
		int axis, row, column;
		if(!asIndex(record[0], axis) || axis > 2 ||
		   !asIndex(record[1], row) || !asIndex(record[2], column) ||
		   record[3] > record[4])
			throw std::runtime_error("binary record has invalid indices or reversed intersection endpoints");
		if(axis < previousAxis ||
		   (axis == previousAxis && row < previousRow) ||
		   (axis == previousAxis && row == previousRow && column < previousColumn) ||
		   (axis == previousAxis && row == previousRow && column == previousColumn &&
			record[3] < previousStart))
			throw std::runtime_error("binary records are not sorted by plane, line, and intersection");
		if(axis == previousAxis && row == previousRow && column == previousColumn &&
		   record[3] < previousEnd)
			throw std::runtime_error("binary line intersections overlap or are out of order");
		previousAxis = axis;
		previousRow = row;
		previousColumn = column;
		previousStart = record[3];
		previousEnd = record[4];

		if(axis >= 0 && axis < currentAxis)
			throw std::runtime_error("binary plane order is invalid");
		while(currentAxis < axis) {
			finishPlane();
			beginPlane(currentAxis + 1);
		}
		width = getPlaneWidth(dimensions, axis);
		height = getPlaneHeight(dimensions, axis);
		if(row >= height || column >= width)
			throw std::runtime_error("binary record line coordinates are outside the volume dimensions");
		const size_t lineIndex = static_cast<size_t>(row) * width + column;
		if(lineIndex < nextLine)
			throw std::runtime_error("binary records revisit a previously completed line");
		while(nextLine < lineIndex) {
			indices[nextLine + 1] = static_cast<int>(intersections.size());
			++nextLine;
		}
		const float start = record[3];
		const float end = record[4];
		if(!std::isfinite(start) || !std::isfinite(end) ||
		   intersections.size() > static_cast<size_t>(std::numeric_limits<int>::max()) - 2)
			throw std::runtime_error("binary line exceeds supported intersection count");
		intersections.push_back(Intersection(start));
		intersections.push_back(Intersection(end));
	}
	finishPlane();
	while(currentAxis < 2) {
		beginPlane(currentAxis + 1);
		finishPlane();
	}
	destination = loaded;
	return true;
}

bool VolumeIo::loadJSON(const std::string &path) const {
	if(!destination) throw std::runtime_error("loadJSON requires a mutable destination volume");
	return loadJSON(path, *destination);
}

bool VolumeIo::loadText(const std::string &path) const {
	if(!destination) throw std::runtime_error("loadText requires a mutable destination volume");
	return loadText(path, *destination);
}

bool VolumeIo::loadBin(const std::string &path) const {
	if(!destination) throw std::runtime_error("loadBin requires a mutable destination volume");
	return loadBin(path, *destination);
}

} // namespace mi
