#include "vec.h"
#include "box.h"
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <cmath>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <string>
#include <chrono>
#include <limits>
#include "log.h"

#include "volume.h"
#include "volume_io.h"
#include "mesh_io.h"

// Small cross-platform timer replacing QTime usage
struct Timer {
	using clock = std::chrono::high_resolution_clock;
	std::chrono::time_point<clock> t;
	void start() { t = clock::now(); }
	void restart() { t = clock::now(); }
	long long elapsed() const { return std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - t).count(); }
};

void rescale(std::vector<Vec3f> &vert, float factor) {
	for(size_t i = 0; i < vert.size(); i++)
		vert[i] *= factor;
}

void translate(std::vector<Vec3f> &vert, Vec3f t) {
	for(size_t i = 0; i < vert.size(); i++) {
		vert[i] += t;
	}
}

void rotate(std::vector<Vec3f> &vert) {
	for(size_t i = 0; i < vert.size(); i++) {
		//vert[i] = Pos3f( vert[i].Y(),-vert[i].X(),vert[i].Z() );
		vert[i] = Pos3f( vert[i].X(),-vert[i].Z(),vert[i].Y() );
	}
}

float diagLen(const std::vector<Vec3f> &vert){
	Box3f box;
	for(unsigned int i = 0; i < vert.size(); i++)
		box.Add(vert[i]);
	return box.Diag();
}

Vec3f center(std::vector<Vec3f> &vert) {
	Box3f box;
	for(unsigned int i = 0; i < vert.size(); i++)
		box.Add(vert[i]);

	translate(vert, -box.Center());
	return box.Center();
}

void addNoise(std::vector<Vec3f> &vert){
	//int t = time(NULL);
	//srand(time(NULL));
	srand(1409248851);
	//Log::debug << "Rand seed: " << t;

	float dx = (rand()%10000)/10000.0;
	float dy = (rand()%10000)/10000.0;
	float dz = (rand()%10000)/10000.0;

	translate(vert, -Vec3f(dx, dy, dz));
}


// a class for an implicit function
class ImplicitSphere {
public:
	float radius;

	ImplicitSphere(float r): radius(r) {}

	Box3f box() {
		Pos3f diag(radius, radius, radius);
		Box3f box;
		box.min = -diag;
		box.max = diag;
		return box;
	}

	std::vector<float> getIntersections(int /*plane*/, float u, float v) {
		std::vector<float> result;
		float d = radius*radius - u*u - v*v;
		if(d > 0) {
			result.push_back(-sqrt(d));
			result.push_back(sqrt(d));
		}
		return result;
	}
	std::vector<float> getIntersections(int /*plane*/, float u, float v,std::vector<Vec3f> &norms) {
		std::vector<float> result;
		float d = radius*radius - u*u - v*v;
		if(d > 0) {
			d = sqrt(d);
			result.push_back(-d);
			norms.push_back( Vec3f(u,v,-d).normalized() );
			result.push_back(d);
			norms.push_back( Vec3f(u,v,+d).normalized() );
		}
		return result;
	}
};


// test an implicit function
void testImplicitSphere(){
	std::vector<Vec3f> vert;
	std::vector<int> face;
	int radius = 1.0f;
	float step = 0.051;

	ImplicitSphere sphere(radius);
	mi::Volume vol;
	vol.fromImplicit(sphere, step);

	vol.intersectPlane( Vec3f(1.0f,	 2.0f, 1.0f), -Vec3f(0.3,0.3,0.25));
	vol.intersectPlane( Vec3f(1.0f, -2.5f, 1.0f), -Vec3f(0.33,-0.32,0.25));

	ImplicitSphere sphere2(radius * 1.2f);
	mi::Volume vol2;
	vol2.fromImplicit(sphere2, step);
	vol2.translate( Vec3f(12.0f/20.0f,12.0f/20.0f,2.0f/20.0f) );

	mi::Volume vol3;
	vol3.unification(vol,vol2);

	vol3.toMeshDual(vert, face);
	savePlyAsciiQuadMesh("dual.ply", vert, face);

	vol3.toMesh(vert, face);
	savePlyAsciiMesh("primal.ply", vert, face);
}

void testSimpleSphere() {
	std::vector<Vec3f> vert;
	std::vector<int> face;
	int radius = 1.0f;
	float step = 1.0f;

	ImplicitSphere sphere(radius);
	mi::Volume vol;
	vol.fromImplicit(sphere, step);


	vol.toMesh(vert, face);
	savePlyAsciiMesh("per_callieri.ply", vert, face);
}

// test an sweep of an implicit function
void testSweep() {
	std::vector<Vec3f> vert;
	std::vector<int> face;
	int radius = 1.0f;
	float step = 0.025;
	ImplicitSphere sphere(radius);

	mi::Volume vol1;
	vol1.fromImplicit(sphere, step);

	mi::Volume vol2;
	vol2.sweep(vol1, Vec3f(0, 0, 0), Vec3f(1.3, 1.3, 1.3), 1);

	vol2.toMeshDual(vert, face);
	savePlyAsciiQuadMesh("sphere.ply", vert, face);
}

// sweef of a mesh
void testSweepMesh(const char *filename, float step) {
	std::vector<Vec3f> vert;
	std::vector<int> face;

	loadObj(filename, vert, face);

	mi::Volume vol1;
	vol1.fromMesh(vert,face, step);

	mi::Volume vol2;
	Vec3f dest = Vec3f(0.312, 0.23,-0.33)*diagLen(vert);
	//vol2.fastSweep( vol1, Vec3f(0, 0, 0), dest*4 ,4);
	vol2.sweep( vol1, Vec3f(0, 0, 0),  dest ,4 );

	vol2.toMeshDual(vert, face);
	savePlyAsciiQuadMesh("result.ply", vert, face);
}

#include "mc_table.h"
void testMc(){
	mi::McTable::init();
	std::vector<Vec3f> vert;
	std::vector<int> face;

	mi::McTable::test( vert );

	face.resize(vert.size());
	for (int i=0; i<(int)vert.size(); i++) face[i]=i;

	savePlyBinMesh("testMC.ply", vert, face);

}


namespace {

enum class FileFormat { Unknown, Obj, Ply, Text, Binary, Json };

FileFormat formatFromPath(const std::string &path) {
	const size_t slash = path.find_last_of("/\\");
	const size_t dot = path.find_last_of('.');
	if(dot == std::string::npos || (slash != std::string::npos && dot < slash))
		return FileFormat::Unknown;
	std::string extension = path.substr(dot);
	for(char &c : extension)
		c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	if(extension == ".obj") return FileFormat::Obj;
	if(extension == ".ply") return FileFormat::Ply;
	if(extension == ".mit") return FileFormat::Text;
	if(extension == ".mib") return FileFormat::Binary;
	if(extension == ".mij") return FileFormat::Json;
	return FileFormat::Unknown;
}

bool writeObj(const std::string &path, const std::vector<Vec3f> &vertices,
			  const std::vector<int> &faces, bool dual) {
	const size_t faceSize = dual ? 4 : 3;
	if(faces.size() % faceSize != 0)
		return false;
	for(int index : faces)
		if(index < 0 || static_cast<size_t>(index) >= vertices.size())
			return false;
	std::ofstream output(path.c_str());
	if(!output)
		return false;
	output << std::setprecision(std::numeric_limits<float>::max_digits10);
	for(const Vec3f &vertex : vertices)
		output << "v " << vertex[0] << ' ' << vertex[1] << ' ' << vertex[2] << '\n';

	for(size_t i = 0; i < faces.size(); i += faceSize) {
		output << "f";
		for(size_t j = 0; j < faceSize; ++j)
			output << ' ' << faces[i + j] + 1;
		output << '\n';
	}
	return output.good();
}

bool sameFilePath(const std::string &input, const std::string &output) {
	std::error_code error;
	const std::filesystem::path inputPath = std::filesystem::absolute(input, error).lexically_normal();
	if(error)
		return input == output;
	const std::filesystem::path outputPath = std::filesystem::absolute(output, error).lexically_normal();
	if(error)
		return input == output;
	if(inputPath == outputPath)
		return true;
	error.clear();
	return std::filesystem::exists(outputPath, error) && !error &&
		std::filesystem::equivalent(inputPath, outputPath, error) && !error;
}

bool objToObj(const std::string &inputPath, const std::string &outputPath,
			  float sampleValue, bool percentStep, bool dual) {
	if(sameFilePath(inputPath, outputPath)) {
		std::cerr << "OBJ input and output must be different files.\n";
		return false;
	}

	std::vector<Vec3f> vertices;
	std::vector<int> faces;
	loadObj(inputPath.c_str(), vertices, faces);
	if(vertices.empty() || faces.empty() || faces.size() % 3 != 0)
		throw std::string("OBJ must contain vertices and triangularized faces.");
	for(const Vec3f &vertex : vertices)
		for(int axis = 0; axis < 3; ++axis)
			if(!std::isfinite(vertex[axis]))
				throw std::string("OBJ contains a non-finite vertex coordinate");
	for(int index : faces)
		if(index < 0 || static_cast<size_t>(index) >= vertices.size())
			throw std::string("OBJ contains a face index outside the vertex array");

	float step = sampleValue;
	if(percentStep) {
		Box3f bounds;
		for(const Vec3f &vertex : vertices)
			bounds.Add(vertex);
		step = bounds.Diag() * (sampleValue / 100.0f);
	}
	if(!std::isfinite(step) || step <= 0.0f)
		throw std::string("Sampling step must be positive and finite");

	mi::Volume volume;
	volume.fromMesh(vertices, faces, step);
	if(dual)
		volume.toMeshDual(vertices, faces);
	else
		volume.toMesh(vertices, faces);
	return writeObj(outputPath, vertices, faces, dual);
}

bool readVolume(mi::VolumeIo &io, FileFormat format, const std::string &path) {
	switch(format) {
	case FileFormat::Text: return io.loadText(path);
	case FileFormat::Binary: return io.loadBin(path);
	case FileFormat::Json: return io.loadJSON(path);
	default: return false;
	}
}

bool writeVolume(const mi::VolumeIo &io, FileFormat format, const std::string &path) {
	switch(format) {
	case FileFormat::Text: return io.saveText(path);
	case FileFormat::Binary: return io.saveBin(path);
	case FileFormat::Json: return io.saveJSON(path);
	default: return false;
	}
}

void printUsage(const char *program) {
	std::cerr << "Usage: " << program << " [-s step | -p percent] [-d | -m] [-e] <input.obj|input.mit|input.mib|input.mij> <output.obj|output.mit|output.mib|output.mij>\n"
			  << "  -s step     Absolute voxel spacing; required for OBJ input.\n"
			  << "  -p percent  Voxel spacing as a percentage of the OBJ bounding-box diagonal; required for OBJ input.\n"
			  << "  -d          Export a dual mesh (default).\n"
			  << "  -m          Export a primal mesh.\n"
			  << "  -e          Use the even-odd rule for OBJ input (default: oriented intersections).\n";
}

} // namespace

int main(int argc, char *argv[]) {
	bool hasStep = false;
	bool hasPercent = false;
	bool hasDualOption = false;
	bool dual = true;
	bool useParity = false;
	float step = 0.0f;
	float percent = 0.0f;
	std::vector<std::string> files;

	for(int i = 1; i < argc; ++i) {
		const std::string argument(argv[i]);
		if(argument == "-d" || argument == "-m") {
			if(hasDualOption) {
				std::cerr << "Specify only one of -d or -m.\n";
				return 2;
			}
			hasDualOption = true;
			dual = argument == "-d";
		} else if(argument == "-e") {
			useParity = true;
		} else if(argument == "-s" || argument == "-p") {
			if(i + 1 >= argc || hasStep || hasPercent) {
				printUsage(argv[0]);
				return 2;
			}
			char *end = nullptr;
			const float value = std::strtof(argv[++i], &end);
			if(end == argv[i] || *end != '\0' || !std::isfinite(value) || value <= 0.0f) {
				std::cerr << "Invalid positive finite value for " << argument << '\n';
				return 2;
			}
			if(argument == "-s") { hasStep = true; step = value; }
			else { hasPercent = true; percent = value; }
		} else if(!argument.empty() && argument[0] == '-') {
			printUsage(argv[0]);
			return 2;
		} else {
			files.push_back(argument);
		}
	}

	if(files.size() != 2 || (hasStep && hasPercent)) {
		printUsage(argv[0]);
		return 2;
	}

	const FileFormat inputFormat = formatFromPath(files[0]);
	const FileFormat outputFormat = formatFromPath(files[1]);
	if(inputFormat == FileFormat::Unknown || outputFormat == FileFormat::Unknown) {
		std::cerr << "Unsupported input or output extension. Use .obj, .ply, .mit, .mib, or .mij.\n";
		return 2;
	}

	if(sameFilePath(files[0], files[1])) {
		std::cerr << "Input and output must be different files.\n";
		return 2;
	}

	if(useParity && inputFormat != FileFormat::Obj) {
		std::cerr << "-e is only valid with OBJ input.\n";
		return 2;
	}

	if(inputFormat == FileFormat::Obj || inputFormat == FileFormat::Ply) {
		if(!hasStep && !hasPercent) {
			std::cerr << "OBJ input requires either -s <step> or -p <percent>.\n";
			printUsage(argv[0]);
			return 2;
		}

	} else {
		if((hasStep || hasPercent)) {
			std::cerr << "-s and -p are only valid with OBJ input.\n";
			return 2;
		}

	}

	if((outputFormat != FileFormat::Obj && outputFormat != FileFormat::Ply) && hasDualOption) {
		std::cerr << "-d and -m are only valid when output is OBJ.\n";
		return 2;
	}


	try {
		/*if(inputFormat == FileFormat::Obj && outputFormat == FileFormat::Obj) {
			if(!objToObj(files[0], files[1], hasPercent ? percent : step, hasPercent, dual))
				throw std::string("Could not write OBJ output: ") + files[1];
			return 0;
		}*/

		mi::Volume volume;
		mi::VolumeIo volumeIo(volume);
		if(inputFormat == FileFormat::Obj) {
			std::vector<Vec3f> vertices;
			std::vector<int> faces;
			loadObj(files[0].c_str(), vertices, faces);
			if(vertices.empty() || faces.empty() || faces.size() % 3 != 0) {
				std::cerr << "OBJ must contain vertices and triangularized faces.\n";
				return 1;
			}
			Box3f bounds;
			for(const Vec3f &vertex : vertices) {
				for(int axis = 0; axis < 3; ++axis)
					if(!std::isfinite(vertex[axis]))
						throw std::string("OBJ contains a non-finite vertex coordinate");
				bounds.Add(vertex);
			}
			for(int index : faces)
				if(index < 0 || static_cast<size_t>(index) >= vertices.size())
					throw std::string("OBJ contains a face index outside the vertex array");
			if(hasPercent)
				step = bounds.Diag() * (percent / 100.0f);
			if(!std::isfinite(step) || step <= 0.0f)
				throw std::string("Sampling step must be positive and finite");
			volume.fromMesh(vertices, faces, step, useParity);

		} else if(!readVolume(volumeIo, inputFormat, files[0])) {
			throw std::string("Could not read or validate input volume: ") + files[0];
		}

		std::string consistencyError;
		if(!volume.checkConsistency(&consistencyError)) {
			throw std::string("Invalid volume: ") + consistencyError;
		}
		if(outputFormat == FileFormat::Obj) {
			std::vector<Vec3f> vertices;
			std::vector<int> faces;
			if(dual)
				volume.toMeshDual(vertices, faces);
			else
				volume.toMesh(vertices, faces);

			if(dual)
				savePlyBinQuadMesh((files[1] + ".ply").c_str(), vertices, faces);
			else
				savePlyBinMesh((files[1] + ".ply").c_str(), vertices, faces);

			if(!writeObj(files[1], vertices, faces, dual))
				throw std::string("Could not write OBJ output: ") + files[1];
		} else if(!writeVolume(volumeIo, outputFormat, files[1])) {
			throw std::string("Could not write volume output: ") + files[1];
		}
	} catch(const std::string &error) {
		std::cerr << error << '\n';
		return 1;
	} catch(const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}

	return 0;
}
