#include "vec.h"
#include "box.h"
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <chrono>
#include "log.h"

#include "intersections.h"
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

// CSM on a mesh
void testCSM(const char *filename, float step, bool dual) {
	Timer clock;
	clock.start();

	std::vector<Vec3f> vert;
	std::vector<int> face;

	loadObj(filename, vert, face);

	Log::debug << "Loading time: " << clock.elapsed() << "ms";

	//center(vert);

	clock.restart();

	mi::Volume volume1;
	volume1.fromMesh(vert, face, step);

	Log::debug << "Creation time: " << clock.elapsed() << "ms";


	clock.restart();
	//if(dual) {
		volume1.toMeshDual(vert, face);
		Log::debug << "Meshing time: " << clock.elapsed() << "ms";
		savePlyAsciiQuadMesh("dual.ply", vert, face);

	//} else {
		volume1.toMesh(vert, face);
		Log::debug << "Meshing time: " << clock.elapsed() << "ms";
		savePlyAsciiMesh("primal.ply", vert, face);

	//}

/*
	rotate(vert);
	mi::Volume volume2;
	volume2.fromMesh(vert, face, step);
	//volume2.fromSweep(vert, face, Vec3f(0, 0, 0), Vec3f(0.3, 0.3, 0.3), step);

	clock.restart();
	mi::Volume volume3;
	volume3.unification( volume1, volume2 );
	//volume3.subtraction( volume1, volume2 );
	//volume3.intersection( volume1, volume2 );
	//volume3.intersectPlane( Vec3f(1,2,1).normalized(), Vec3f(0,0,0) );
	Log::debug << "Operation  time: " << clock.elapsed() << "ms";

	clock.restart();
	if(dual) {
		volume3.toMeshDual(vert, face);
		Log::debug << "Meshing time: " << clock.elapsed() << "ms";
		savePlyAsciiQuadMesh("dual.ply", vert, face);
	} else {
		volume3.toMesh(vert, face);
		Log::debug << "Meshing time: " << clock.elapsed() << "ms";
		savePlyAsciiMesh("primal.ply", vert, face);
	} */
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


int main(int argc, char *argv[])
{
	//testImplicitSphere();
	//testSimpleSphere();
	//return 0;
	//mi::McTable::test2();
	//testMc();
	//testImplicitSphere();

	//return 0;

	if(argc != 3) {
		Log::debug << "Usage: " << argv[0] << " <step> <mesh.obj> \n";
		return -1;
	}

	float step = static_cast<float>(std::atof(argv[1]));
	const char *filename = argv[2];
	bool dual = false;
	try {
		testCSM(filename, step, dual);
	} catch(const std::string &error) {
		std::cerr << error << std::endl;
		return -1;
	}

	//testSweepMesh();
	return 0;

}


