#include "vec.h"
#include "box.h"
#include <QFile>
#include <QTextStream>
#include <QString>
#include <chrono>
#include "log.h"

#include "intersections.h"

// Small cross-platform timer replacing QTime usage
struct Timer {
	using clock = std::chrono::high_resolution_clock;
	std::chrono::time_point<clock> t;
	void start() { t = clock::now(); }
	void restart() { t = clock::now(); }
	long long elapsed() const { return std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - t).count(); }
};

void loadObj(QString filename, std::vector<Vec3f> &vert, std::vector<int> &face) {
	char buffer[1024];
	QFile file(filename);
	if(!file.open(QFile::ReadOnly))
		throw QString("Could not open file '%1' error: %2").arg(filename).arg(file.errorString());

	while(1) {
		int s = file.readLine(buffer, 1024);
		if(s == -1)                     //end of filebottom_corners[i] = bottom_vert[i]
			break;
		if(s == 0) continue;            //skip empty lines

		if(buffer[0] == '#')            //skip comments
			continue;
		buffer[s] = '\0';               //terminating line, readLine wont do this.

		if(buffer[0] == 'v') {          //vertex
			if(buffer[1] == ' ') {      //skip other properties
				Pos3f v;
				int n = sscanf(buffer, "v %f %f %f", &(v[0]), &(v[1]), &(v[2]));
				if(n != 3) throw QString("Error parsing vertex line: %1").arg(buffer);
				vert.push_back(v);

			}//skipping other properties in OBJ
			continue;
		}
		if(buffer[0] == 'f') {
			int f[4];
			int res=sscanf(buffer, "f %d %d %d %d", &f[0], &f[1], &f[2], &f[3]);
			if (res !=4 && res != 3) {
				int dummy;
				res=sscanf(buffer, "f %d//%d %d//%d %d//%d %d//%d", &f[0], &dummy, &f[1], &dummy,  &f[2] , &dummy ,  &f[3] , &dummy);
				if(res != 8 && res != 6)
					throw QString("Could not parse face: %1").arg(buffer);\
					res /=2;
			}
			if (res == 3) {
				for(int i = 0; i < 3; i++) //obj indexes start from 1
					face.push_back(f[i] -1);
			}
			if (res == 4) {
				for(int i = 0; i < 3; i++) face.push_back(f[i] -1);
				for(int i = 2; i < 5; i++) face.push_back(f[i%4] -1);
			}

		}
	}
}

void saveObj(QString filename, std::vector<Vec3f> &vert, std::vector<Vec3f> &norm, std::vector<int> &face) {
	QFile file(filename);
	if(!file.open(QFile::WriteOnly))
		throw QString("Could not open file '%1' error: %2").arg(filename).arg(file.errorString());
	QTextStream stream(&file);


	for(size_t i = 0; i < vert.size(); i++) {
		Pos3f &v = vert[i];
		stream << "v " << v[0] << " " << v[1] << " " << v[2] << "\n";

		Vec3f &n = norm[i];
		stream << "vn " << n[0] << " " << n[1] << " " << n[2] << "\n";
	}
}

void savePly(QString filename, std::vector<Vec3f> &vert, std::vector<Vec3f> &norm, std::vector<int> &face) {

	QFile file(filename);
	if(!file.open(QFile::WriteOnly))
		throw QString("Could not open file '%1' error: %2").arg(filename).arg(file.errorString());
	QTextStream stream(&file);

	stream << "ply\nformat ascii 1.0\n";
	stream << "element vertex " << vert.size() << "\n";
	stream << "property float x\n" "property float y\n" "property float z\n";
	stream << "property float nx\n" "property float ny\n" "property float nz\n";
	//stream << "element face " << face.size() << "\n";
	//stream << "property list uchar int vertex_indices\n";
	stream << "end_header\n";

	for(size_t i = 0; i < vert.size(); i++) {
		Pos3f &v = vert[i];
		stream << v[0] << " " << v[1] << " " << v[2] << " ";

		Vec3f &n = norm[i];
		stream << n[0] << " " << n[1] << " " << n[2] << "\n";
	}

}

void savePlyBinCloud(QString filename, std::vector<Vec3f> &vert, std::vector<Vec3f> &norm) {

	QFile file(filename);
	if(!file.open(QFile::WriteOnly))
		throw QString("Could not open file '%1' error: %2").arg(filename).arg(file.errorString());
	{
		QTextStream stream(&file);

		stream << "ply\nformat binary_little_endian 1.0\n";
		stream << "element vertex " << vert.size() << "\n";
		stream << "property float x\n" "property float y\n" "property float z\n";
		stream << "property float nx\n" "property float ny\n" "property float nz\n";
		stream << "end_header\n";
	}


	for(size_t i = 0; i < vert.size(); i++) {
		Pos3f &v = vert[i];
		file.write((char *)&v, 3*4);

		Vec3f &n = norm[i];
		file.write((char *)&n, 3*4);
	}

}

void savePlyBinMesh(QString filename, std::vector<Vec3f> &vert, std::vector<int> &face) {

	QFile file(filename);
	if(!file.open(QFile::WriteOnly))
		throw QString("Could not open file '%1' error: %2").arg(filename).arg(file.errorString());
	{
		QTextStream stream(&file);

		stream << "ply\nformat binary_little_endian 1.0\n";
		stream << "element vertex " << vert.size() << "\n";
		stream << "property float x\n" "property float y\n" "property float z\n";
		stream << "element face " << face.size()/3 << "\n";
		stream << "property list uchar int vertex_indices\n";
		stream << "end_header\n";
	}

	file.write((char *)&*vert.begin(), vert.size()*3*4);

	char n = 3;
	for(size_t i = 0; i < face.size(); i += 3) {
		file.write( &n, 1);
		file.write((char *)&(face[i]), 3*4);
	}
}

void savePlyAsciiMesh(QString filename, std::vector<Vec3f> &vert, std::vector<int> &face) {

	QFile file(filename);
	if(!file.open(QFile::WriteOnly))
		throw QString("Could not open file '%1' error: %2").arg(filename).arg(file.errorString());

	QTextStream stream(&file);

	stream << "ply\nformat ascii 1.0\n";
	stream << "element vertex " << vert.size() << "\n";
	stream << "property float x\n" "property float y\n" "property float z\n";
	stream << "element face " << face.size()/3 << "\n";
	stream << "property list uchar int vertex_indices\n";
	stream << "end_header\n";


	//file.write((char *)&*vert.begin(), vert.size()*3*4);

	for(size_t i = 0; i < vert.size(); i++) {
		Pos3f &v = vert[i];
		stream << v[0] << " " << v[1] << " " << v[2] << "\n";
	}
	for(size_t i = 0; i < face.size(); i += 3) {
		stream << 3 << " " << face[i] << " " << face[i+1] << " " << face[i+2] << "\n";
	}

}

void savePlyAsciiQuadMesh(QString filename, std::vector<Vec3f> &vert, std::vector<int> &face) {

	QFile file(filename);
	if(!file.open(QFile::WriteOnly))
		throw QString("Could not open file '%1' error: %2").arg(filename).arg(file.errorString());

	QTextStream stream(&file);

	stream << "ply\nformat ascii 1.0\n";
	stream << "element vertex " << vert.size() << "\n";
	stream << "property float x\n" "property float y\n" "property float z\n";
	stream << "element face " << face.size()/2 << "\n";
	stream << "property list uchar int vertex_indices\n";
	stream << "end_header\n";


	//file.write((char *)&*vert.begin(), vert.size()*3*4);

	for(size_t i = 0; i < vert.size(); i++) {
		Pos3f &v = vert[i];
		stream << v[0] << " " << v[1] << " " << v[2] << "\n";
	}
	for(size_t i = 0; i < face.size(); i += 4) {
		stream << 3 << " " << face[i] << " " << face[i+1] << " " << face[i+2] << "\n";
		stream << 3 << " " << face[i+2] << " " << face[i+3] << " " << face[i] << "\n";
	}

}


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
	for(uint i = 0; i < vert.size(); i++)
		box.Add(vert[i]);
	return box.Diag();
}

Vec3f center(std::vector<Vec3f> &vert) {
	Box3f box;
	for(uint i = 0; i < vert.size(); i++)
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

float step = 0;
char* filename = NULL;

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
void testSweep(){
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
void testSweepMesh(){
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
void testCSM(){
	Timer clock;
	clock.start();

	std::vector<Vec3f> vert;
	std::vector<int> face;

	loadObj(filename, vert, face);

	Log::debug << "Loading time: " << clock.elapsed() << "ms";

	center(vert);

	clock.restart();

	mi::Volume volume1;
	volume1.fromMesh(vert, face, step);

	Log::debug << "Creation time: " << clock.elapsed() << "ms";

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
	volume3.toMeshDual(vert, face);
	Log::debug << "Meshing time: " << clock.elapsed() << "ms";

	clock.restart();
	savePlyAsciiQuadMesh("result.ply", vert, face);
	//savePlyAsciiMesh("result.ply", vert, face);
	Log::debug << "Saving time: " << clock.elapsed() << "ms";
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
	testImplicitSphere();
	//testSimpleSphere();
	return 0;
	//mi::McTable::test2();
	//testMc();
	//testImplicitSphere();

	//return 0;

	if(argc != 3) {
		Log::debug << "Usage: " << argv[0] << " <step> <mesh.obj> \n";
		return -1;
	}

	step = QString(argv[1]).toFloat();
	filename = argv[2];

	testCSM();
	//testSweepMesh();
	return 0;

}


