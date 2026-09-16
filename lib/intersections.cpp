#include "log.h"

#include "intersections.h"
#include "mc_edge.h"
#include "loom.h"
#include "dual_loom.h"

#include <algorithm>
#include <sstream>

#define EPSILON 0.000001

using namespace mi;


Volume::Volume(Box3i box):isClean(true){
	resize(box);
}

int OutputLine::cleanUp(const Line &a) {

	int i = 0;
	int count = 0;
	while(i < a.size()) {
		if(i == a.size()-1) {
			push_back(a[i]);
			break;
		}
		int prev = floor(a.at(i).p);
		int next = floor(a.at(i+1).p);
		if(prev == next) {
			i += 2;
			count += 2;
		} else {
			push_back(a[i++]);
		}
	}
	return count;
}

int OutputLine::scaleAndCleanup(const Line &a, int scale, int add) {

	int i = 0;
	int count = 0;
	while(i < a.size()) {
		if(i == a.size()-1) {
			push_back((a[i] - add)/scale);
			break;
		}
		int prev = floor((a.at(i  ).p-add)/scale);
		int next = floor((a.at(i+1).p-add)/scale);
		if(prev == next) {
			i += 2;
			count += 2;
		} else {
			push_back((a[i] - add)/scale);
			i++;
		}
	}
	return count;
}

void OutputLine::scale(const Line &a, int scale, int add) {

	for(int i = 0; i < a.size(); i++)
		push_back((a[i]-add)/scale);
}



void OutputLine::unification(const Line &a, const Line &b) {

	int i = 0;
	int j = 0;

	while(true) {

		bool win_a;
		if(j >= b.size()) {
			if(i >= a.size()) break;
			win_a = true;
		} else {
			if(i >= a.size())
				win_a = false;
			else {
				win_a = (a[i] < b[j]);
			}
		}

		bool was_inside = (i%2) || (j%2);
		if(win_a) i++; else j++;

		bool is_inside = (i%2) || (j%2);
		if(was_inside == is_inside) continue;
		if(win_a) {
			push_back(a[i-1]);
		} else {
			push_back(b[j-1]);
		}
	}

}

void OutputLine::intersection(const Line &a, const Line &b) {
	int i = 0;
	int j = 0;

	while(true) {

		bool win_a;
		if(j >= b.size()) {
			if(i >= a.size()) break;
			win_a = true;
		} else {
			if(i >= a.size())
				win_a = false;
			else {
				win_a = (a[i] < b[j]);
			}
		}

		bool was_inside = (i%2) && (j%2);
		if(win_a) i++; else j++;

		bool is_inside = (i%2) && (j%2);
		if(was_inside == is_inside) continue;
		if(win_a) {
			push_back(a[i-1]);
		} else {
			push_back(b[j-1]);
		}
	}
}

void OutputLine::subtraction(const Line &a, const Line &b) {
	int i = 0;
	int j = 0;

	while(true) {

		bool win_a;
		if(j >= b.size()) {
			if(i >= a.size()) break;
			win_a = true;
		} else {
			if(i >= a.size())
				win_a = false;
			else {
				win_a = (a[i] < b[j]);
			}
		}

		bool was_inside = (i%2) && !(j%2);
		if(win_a) i++; else  j++;

		bool is_inside = (i%2) && !(j%2);
		if(was_inside == is_inside) continue;
		if(win_a) {
			push_back(a[i-1]);
		} else {
			push_back(b[j-1].flip());
		}
	}
}

static bool aisInside(float x, float y, const Pos2f &a, const Pos2f &b, float &res) {
	Vec2f C((float)x, (float)y);

	if(a < b) {
		res = -((C - b)^(a - b));
		return res <= 0;
	} else {
		res = ((C - a)^(b - a));
		return  res < 0;
	}
}

void Line::insert(Intersection a) {
	//    int count = 0;
	int i = 0;
	while(a.p > at(i).p) i++;

	while(at(i).p == at(i).p) { //nan
		//        count++;
		/*        if(count > 100) {
			Log::debug << "a: " << a.p;
			for(int k =0 ; k < 10; k++) {
				Log::debug << at(k).p;
			}
			exit(0);
		} */
		std::swap(at(i++), a);
	}
	at(i) = a;
}

bool close(Pos2f a, Pos2f b) {
	return fabs(a[0] - b[0]) < 0.01 && fabs(a[1] - b[1]) < 0.01;
}

void move(Pos3f &a) {
	for(int k = 0; k < 3; k++) {
		if(fabs(a[k] - floor(a[k]) < 0.001) ||
				fabs(a[k] - ceil(a[k]) < 0.001))
			a[k] += 0.0011;
	}
}

void Plane::fillTriangleOld(const Pos3f &a, const Pos3f &b, const Pos3f &c, std::vector<std::pair<int, Intersection > > & result) {

	Vec2f e0(c[1] - a[1], a[0] - c[0]); //Magic.
	Vec2f e1(a[1] - b[1], b[0] - a[0]);

	float det = e0[0]*e1[1] - e0[1]*e1[0];
	//if(fabs(det) < EPSILON) return;

	//compute bounding box
	const int minx = (int)ceil (std::min(a[0], std::min(b[0], c[0])));
	const int miny = (int)ceil (std::min(a[1], std::min(b[1], c[1])));
	const int maxx = (int)floor(std::max(a[0], std::max(b[0], c[0])));
	const int maxy = (int)floor(std::max(a[1], std::max(b[1], c[1])));

	const float za = a[2]/det;
	const float zb = b[2]/det;
	const float zc = c[2]/det;

	const Vec2f a2(a[0], a[1]);
	const Vec2f b2(b[0], b[1]);
	const Vec2f c2(c[0], c[1]);

#if STORE_NORMALS
	Vec3f n = ((a-b)^(c-b)).Normalize();
#endif

	for(int y = miny; y <= maxy; y++) {
		for(int x = minx; x <= maxx; x++) {

			float ax = x;
			float ay = y;

			float DA, DB, DC;
			bool A = aisInside(ax, ay, b2, c2, DA);
			bool B = aisInside(ax, ay, c2, a2, DB);
			bool C = aisInside(ax, ay, a2, b2, DC);

			if((A != B || B != C)) continue;

			float z = -(za*DA + zb*DB + zc*DC);
#if STORE_NORMALS
			result.push_back(std::make_pair( index(x, y), Intersection(z,n) ));
#else
			result.push_back(std::make_pair( index(x, y), Intersection(z) ));
#endif
		}
	}
}


void Plane::fillTriangle(const Pos3f &af, const Pos3f &bf, const Pos3f &cf, std::vector<std::pair<int, Intersection > > & result) {

	/*
	// compute zX, zY, zC, s.t.   z = zC + y*zY + x*zX
	Vec2f eX( af[0]-cf[0] , bf[0]-af[0] );
	Vec2f eY( cf[1]-af[1] , af[1]-bf[1] );
	Vec2f eZ( bf[2]-af[2] , cf[2]-af[2] );
	float det = eY ^ eX ;
	float zX = eY * eZ / det;
	float zY = eX * eZ / det;
	float zS = af[2] - zX*af[0] - zY*af[1];
	*/

#if STORE_NORMALS
	Vec3f n = ((af-bf)^(cf-bf)).Normalize();
#endif

	const int SHIFT = 6;
	const float MULT = (1<<SHIFT);
	const int FF = (1<<SHIFT)-1;

	// fixed precision positions of vertices
	const Pos2i a( round(af[0]*MULT), round(af[1]*MULT) );
	const Pos2i b( round(bf[0]*MULT), round(bf[1]*MULT) );
	const Pos2i c( round(cf[0]*MULT), round(cf[1]*MULT) );

	// compute bounding box
	const int minx = (((std::min(a[0], std::min(b[0], c[0])))+FF) >> SHIFT);
	const int miny = (((std::min(a[1], std::min(b[1], c[1])))+FF) >> SHIFT);
	const int maxx = (((std::max(a[0], std::max(b[0], c[0])))   ) >> SHIFT);
	const int maxy = (((std::max(a[1], std::max(b[1], c[1])))   ) >> SHIFT);

	Vec2i min (minx<<SHIFT, miny<<SHIFT);
	Vec2i ea = b - a;
	Vec2i eb = c - b;
	Vec2i ec = a - c;

	Vec3i edgeFunSt( (min-a)^ea, (min-b)^eb, (min-c)^ec );
	Vec3i edgeFunDx( +ea[1]<<SHIFT , +eb[1]<<SHIFT , +ec[1]<<SHIFT );
	Vec3i edgeFunDy( -ea[0]<<SHIFT , -eb[0]<<SHIFT , -ec[0]<<SHIFT );

	float edgeFunSum = edgeFunSt[0] + edgeFunSt[1] + edgeFunSt[2];
	Vec3f zetas = Vec3f(cf[2],af[2],bf[2]) / edgeFunSum;

	// if ties possible, small bonuses to break them: corresponds
	// to give infinetesimal +dx and +dy to pixel coords (with dx >> dy)
	if ( ( (edgeFunSt[0] & FF) == 0) && (b>a) ) edgeFunSt[0]++;
	if ( ( (edgeFunSt[1] & FF) == 0) && (c>b) ) edgeFunSt[1]++;
	if ( ( (edgeFunSt[2] & FF) == 0) && (a>c) ) edgeFunSt[2]++;

	for(int y = miny; y <= maxy; y++, edgeFunSt+=edgeFunDy ) {

		Vec3i edgeFun = edgeFunSt;
		for(int x = minx; x <= maxx; x++ , edgeFun += edgeFunDx ) {

			if(((edgeFun[0]>0) != (edgeFun[1]>0))
					|| ((edgeFun[1]>0) != (edgeFun[2]>0))) continue;

			//float z = zS + zX*x + zY*y;
			float z = Vec3f::Construct( edgeFun ) * zetas;

#if STORE_NORMALS
			result.push_back(std::make_pair( index(x, y), Intersection(z,n) ));
#else
			result.push_back(std::make_pair( index(x, y), Intersection(z) ));
#endif

		}
	}
}

void Plane::accumulateLengths() {
	int count = 0;
	for(unsigned int i = 0; i < indices.size(); i++) {
		int len = indices[i];
		indices[i] = count;
		count += len;
	}
}

void Plane::allocate() {
	const unsigned int NaNi = 0xffc00000;;
	const float NaN = *(float *)&NaNi;
	intersections.resize(0);
#if STORE_NORMALS
	intersections.resize(indices.back(), Intersection(NaN,Vec3f(0,0,0)) );
#else
	intersections.resize(indices.back(), NaN);
#endif
}

bool Plane::isInside(Vec3i p) const {
	const Line line = at(p[0], p[1]);
	int i = 0;
	for(; i < line.size(); i++) {
		if(p[2] <= line[i].p)
			break;
	}
	return (i%2) == 1;
}

Intersection *Plane::getClosest(Vec3i p) {
	Line line = at(p[0], p[1]);
	Intersection *result = NULL;
	int i = 0;
	for(; i < line.size(); i++)
		if(line[i].p >= p[2]-1 && line[i].p < p[2]+1)
			if(result == NULL || fabs(result->p - p[2]) > fabs(line[i].p - p[2]))
				result = &line[i];

	return result;
}

void Volume::resize(Box3i _box) {
	if(pow(_box.Volume(), 0.6666) > 100000000) {
		Log::debug << "Danger, Will Robinson! Playing with big numbers are you?";
		exit(-1);
	}
	box = _box;

	planes[0].resize( toLocal( box , 0 ) );
	planes[1].resize( toLocal( box , 1 ) );
	planes[2].resize( toLocal( box , 2 ) );

}


void Volume::fromMesh(const std::vector<Pos3f> &overts, const std::vector<int> &faces, float _step) {
	step = _step;

	std::vector<Pos3f> verts = overts;
	if(verts.size() == 0) return;

	Vec3f min(verts[0]);
	Vec3f max(verts[0]);
	for(size_t i = 0; i < verts.size(); i++) {
		Vec3f &v = verts[i];
		v /= step;
#define OLD_TRIANGULATOR
#ifdef OLD_TRIANGULATOR
		move(v);
#endif
		for(int k = 0; k < 3; k++) {
			if(min[k] > v[k]) min[k] = v[k];
			if(max[k] < v[k]) max[k] = v[k];
		}
	}

	const int border = 1;
	box.min[0] = (int)floor(min[0]) - border;
	box.min[1] = (int)floor(min[1]) - border;
	box.min[2] = (int)floor(min[2]) - border;

	box.max[0] = (int)ceil(max[0]) + border;
	box.max[1] = (int)ceil(max[1]) + border;
	box.max[2] = (int)ceil(max[2]) + border;

	// Log::debug<<"Box of"<<verts.size()<<"triangles: "<<box.min[0]<<box.min[1]<<box.min[2] << "to" <<box.max[0]<<box.max[1]<<box.max[2];

	resize(box);

	std::vector< std::pair<int, Intersection > > results;

	for(int pi = 0; pi < 3; pi++) {
		results.clear();
		Plane &p = planes[pi];
		for(size_t i = 0; i < faces.size(); i += 3) {
			const Pos3f &a = verts[faces[i]];
			const Pos3f &b = verts[faces[i+1]];
			const Pos3f &c = verts[faces[i+2]];
#ifdef OLD_TRIANGULATOR
			p.fillTriangleOld(toLocal(a, pi), toLocal(b, pi), toLocal(c, pi), results);
#else
			p.fillTriangle(toLocal(a, pi), toLocal(b, pi), toLocal(c, pi), results);
#endif
		}
		for(size_t i = 0; i < results.size(); i++) {
			p.indices[ results[i].first ]++;
		}

		planes[pi].accumulateLengths();
		planes[pi].allocate();

		for(size_t i = 0; i < results.size(); i++) {
			p.at(results[i].first).insert(Intersection(results[i].second));
		}
	}

	checkIntersectionParity();
	enforceConsistency();
	isClean = false;
}

class Edge {
public:
	Edge() {}
	Edge(int _v0, int _v1): v0(_v0), v1(_v1) {}
	int v0, v1;
	bool operator<(const Edge &e) const {
		if(v0 == e.v0)
			return v1 < e.v1;
		return v0 < e.v0;
	}
	bool operator==(const Edge &e) const {
		return v0 == e.v0 && v1 == e.v1;
	}
};

void Volume::fromSweep(const std::vector<Pos3f> &verts, const std::vector<int> &faces, const Vec3f &start, const Vec3f &end, float step) {
	Vec3f dir = end - start;

	std::vector<Pos3f> new_verts(verts.size()*2);
	for(unsigned int i = 0; i < verts.size(); i++) {
		new_verts[i] = verts[i] + start;
		new_verts[verts.size() + i] = verts[i] + end;
	}

	std::vector<Edge> edges;
	//classify faces and find edges
	std::vector<int> new_faces(faces.size());
	std::vector<bool> orientation(faces.size()/3, false);
	for(unsigned int i = 0; i < faces.size(); i += 3) {
		const Pos3f &a = verts[faces[i+0]];
		const Pos3f &b = verts[faces[i+1]];
		const Pos3f &c = verts[faces[i+2]];
		Vec3f n = ((a-b)^(c-b)).Normalize();
		int offset = 0;
		if(n * dir > 0) { //back

			edges.push_back(Edge(faces[i+1], faces[i+0]));
			edges.push_back(Edge(faces[i+2], faces[i+1]));
			edges.push_back(Edge(faces[i+0], faces[i+2]));
		} else {
			offset = verts.size();
			orientation[i/3] = true;
			edges.push_back(Edge(faces[i+0], faces[i+1]));
			edges.push_back(Edge(faces[i+1], faces[i+2]));
			edges.push_back(Edge(faces[i+2], faces[i+0]));
		}
		new_faces[i + 0] = offset + faces[i+0];
		new_faces[i + 1] = offset + faces[i+1];
		new_faces[i + 2] = offset + faces[i+2];
	}
	//find silhouette edges edges and sweep them
	std::sort(edges.begin(), edges.end());
	for(unsigned int i = 0; i < edges.size()-1; i++) {
		Edge &e = edges[i];
		if(e == edges[i+1]) {
			new_faces.push_back(e.v0);
			new_faces.push_back(e.v0 + verts.size());
			new_faces.push_back(e.v1);

			new_faces.push_back(e.v0 + verts.size());
			new_faces.push_back(e.v1 + verts.size());
			new_faces.push_back(e.v1);
			i++;
		}
	}
	fromMesh(new_verts, new_faces, step);
}

/* old way: use class Loom now */
void Volume::triangulateOld(std::vector<char> &visited, const Pos3i &pos, std::vector<int> &faces) {

	static int edge_index[3][2][2] = {         //i, u, v
											   { {0,4},{2,6} },
											   { {3,1},{7,5} },
											   { {8,11},{9,10} }
									 };
	McEdge cube;
	int plane_offset = 0;
	for(int i = 0; i < 3; i++) {
		const Plane &p = planes[i];
		Vec3i local = toLocal(pos, i);
		for(int dv = 0; dv <= 1; dv++) {
			for(int du = 0; du <= 1; du++) {
				int index = p.index(local[0] + du, local[1] + dv);
				const Line line = p.at(local[0] + du, local[1] + dv);
				int line_offset = plane_offset + p.indices[index];

				for(int j = 0; j < line.size(); j++) {
					const Intersection &in = line.at(j);
					if(local[2] > in.p) continue;
					if(local[2]+1 <= in.p) break;

					int id = line_offset + j;

					char &mask = visited[id];
					mask |= (1<<(du + 2*dv));
					// e' lui!
					cube.addEdge(id, j&1, edge_index[i][du][dv]);
				}
			}
		}
		plane_offset += p.indices.back();
	}
	cube.triangulate(faces);

	isClean = false;
}

void Volume::toMesh(std::vector<Pos3f> &verts, std::vector<int> &faces) {
	verts.clear();
	faces.clear();

	cleanUp();

	toPointCloud(verts);
	Loom loom(*this);
	loom.weave( faces );
}

void Volume::toMeshDual(std::vector<Pos3f> &verts, std::vector<int> &quads) {

	verts.clear();
	quads.clear();

	cleanUp();

	DualLoom loom(*this);
	loom.weave( verts , quads );

	optimizeQuadDiagonals( quads, verts.size() );
	optimizeQuadDiagonals( quads, verts.size() );
	optimizeQuadDiagonals( quads, verts.size() );

}

int Volume::getCentroid(int id, std::vector<int> &visited, const Pos3i &pos, std::vector<Pos3f> &vert) {

	if(visited[id] != -1)
		return visited[id];

	Pos3f center(0, 0, 0);
	int count = 0;

	int current_vertex = vert.size();

	int plane_offset = 0;
	for(int i = 0; i < 3; i++) {
		const Plane &p = planes[i];
		Vec3i local = toLocal(pos, i);
		for(int dv = 0; dv <= 1; dv++) {
			for(int du = 0; du <= 1; du++) {
				int index = p.index(local[0] + du, local[1] + dv);
				const Line line = p.at(local[0] + du, local[1] + dv);

				int line_offset = plane_offset + p.indices[index];

				for(int j = 0; j < line.size(); j++) {
					const Intersection &in = line.at(j);
					if(local[2] > in.p) continue;
					if(local[2]+1 <= in.p) break;

					int id = 4*(line_offset + j) + du + 2*dv;
					visited[id] = current_vertex;

					center += toGlobal(Pos3f(local[0]+du, local[1] + dv, in.p), i);
					count++;
				}
			}
		}
		plane_offset += p.indices.back();
	}
	center /= count;
	vert.push_back(center);
	return current_vertex;
}

void Volume::toMeshDualOld(std::vector<Pos3f> &verts, std::vector<int> &faces) {

	verts.clear();
	faces.clear();

	cleanUp();

	std::vector<int> visited(4*(planes[0].indices.back() + planes[1].indices.back() + planes[2].indices.back()), -1);
	faces.reserve(visited.size()/2);
	verts.reserve(faces.size()/2);

	int plane_offset = 0;
	for(int i = 0; i < 3; i++) {
		Plane &p = planes[i];
		for(int v = p.box.min[1]; v < p.box.max[1]; v++) {
			for(int u = p.box.min[0]; u < p.box.max[0]; u++) {
				int index = p.index(u, v);
				Line line = p.at(index);
				int offset = plane_offset + p.indices[index];

				for(int j = 0; j < line.size(); j++) {
					Intersection &in = line[j];
					int pos = (int)floor(in.p);
					int id = (offset + j)*4;

					int vertices[4];
					vertices[0] = getCentroid(id + 0, visited, toGlobal(Pos3i(u,   v,   pos), i), verts);
					vertices[1] = getCentroid(id + 1, visited, toGlobal(Pos3i(u-1, v,   pos), i), verts);
					vertices[2] = getCentroid(id + 2, visited, toGlobal(Pos3i(u,   v-1, pos), i), verts);
					vertices[3] = getCentroid(id + 3, visited, toGlobal(Pos3i(u-1, v-1, pos), i), verts);
					float d03 = (verts[vertices[0]] - verts[vertices[3]]).SquaredNorm();
					float d12 = (verts[vertices[1]] - verts[vertices[2]]).SquaredNorm();
					if(j%2) {
						if(d03 < d12) {
							faces.push_back(vertices[0]);
							faces.push_back(vertices[1]);
							faces.push_back(vertices[3]);

							faces.push_back(vertices[3]);
							faces.push_back(vertices[2]);
							faces.push_back(vertices[0]);
						} else {
							faces.push_back(vertices[0]);
							faces.push_back(vertices[1]);
							faces.push_back(vertices[2]);

							faces.push_back(vertices[2]);
							faces.push_back(vertices[1]);
							faces.push_back(vertices[3]);
						}
					} else {
						if(d03 < d12) {
							faces.push_back(vertices[1]);
							faces.push_back(vertices[0]);
							faces.push_back(vertices[3]);

							faces.push_back(vertices[2]);
							faces.push_back(vertices[3]);
							faces.push_back(vertices[0]);
						} else {
							faces.push_back(vertices[1]);
							faces.push_back(vertices[0]);
							faces.push_back(vertices[2]);

							faces.push_back(vertices[1]);
							faces.push_back(vertices[2]);
							faces.push_back(vertices[3]);
						}
					}
				}
			}
		}
		plane_offset += p.indices.back();
	}
}

void Volume::toMeshOld(std::vector<Pos3f> &verts, std::vector<int> &faces) {

	verts.clear();
	faces.clear();

	cleanUp();

	McEdge::init(); // actually, needed only once

	std::vector<char> visited(planes[0].indices.back() + planes[1].indices.back() + planes[2].indices.back(), 0);

	int plane_offset = 0;
	for(int i = 0; i < 3; i++) {
		Plane &p = planes[i];
		for(int v = p.box.min[1]; v < p.box.max[1]; v++) {
			for(int u = p.box.min[0]; u < p.box.max[0]; u++) {
				int index = p.index(u, v);
				Line line = p.at(index);
				int offset = plane_offset + p.indices[index];

				for(int j = 0; j < line.size(); j++) {
					Intersection &in = line[j];
					int pos = (int)floor(in.p);
					int id = offset + j;
					char mask = visited[id];

					if(!(mask & 0x1))
						triangulateOld(visited, toGlobal(Pos3i(u,   v,   pos), i), faces);
					if(!(mask & 0x2))
						triangulateOld(visited, toGlobal(Pos3i(u-1, v,   pos), i), faces);
					if(!(mask & 0x4))
						triangulateOld(visited, toGlobal(Pos3i(u,   v-1, pos), i), faces);
					if(!(mask & 0x8))
						triangulateOld(visited, toGlobal(Pos3i(u-1, v-1, pos), i), faces);
				}
			}
		}
		plane_offset += p.indices.back();
	}
}


void Volume::unification(const Volume &a, const Volume &b) {
	if(a.step != b.step) {
		Log::debug << "CHE CAZZO FAI STRONZONE CON GLI STEP DIVERSI?!";
		Log::debug << a.step << b.step;
	}
	step = a.step;
	box = a.box;
	box.Add(b.box);
	resize(box);

	for(int i = 0; i < 3; i++) {
		Plane &p = planes[i];
		const Plane &pa = a.planes[i];
		const Plane &pb = b.planes[i];

		p.intersections.resize(pa.intersections.size() + pb.intersections.size());

		int index = 0;
		for(int v = p.box.min[1]; v < p.box.max[1]; v++)
			for(int u = p.box.min[0]; u < p.box.max[0]; u++) {
				OutputLine line = p.output(index++);
				line.unification(pa.at(u, v), pb.at(u, v));
			}
		p.intersections.resize(p.indices.back());
	}

	isClean = false;
}

void Volume::intersection(const Volume &a, const Volume &b) {

	if(a.step != b.step) {
		Log::debug << "CHE CAZZO FAI STRONZONE CON GLI STEP DIVERSI?!";
		Log::debug << a.step << b.step;
	}
	step = a.step;
	box = a.box;
	box.Intersect(b.box);
	resize(box);

	for(int i = 0; i < 3; i++) {
		Plane &p = planes[i];
		const Plane &pa = a.planes[i];
		const Plane &pb = b.planes[i];

		p.intersections.resize(pa.intersections.size() + pb.intersections.size());

		int index = 0;
		for(int v = p.box.min[1]; v < p.box.max[1]; v++)
			for(int u = p.box.min[0]; u < p.box.max[0]; u++) {
				OutputLine line = p.output(index++);
				line.intersection(pa.at(u, v), pb.at(u, v));
			}
		p.intersections.resize(p.indices.back());
	}
	isClean = false;
}

void Volume::subtraction(const Volume &a, const Volume &b) {
	if(a.step != b.step) {
		Log::debug << "CHE CAZZO FAI STRONZONE CON GLI STEP DIVERSI?!";
		Log::debug << a.step << b.step;
	}
	step = a.step;

	box = a.box;
	resize(box);

	for(int i = 0; i < 3; i++) {
		Plane &p = planes[i];
		const Plane &pa = a.planes[i];
		const Plane &pb = b.planes[i];

		p.intersections.resize(pa.intersections.size() + pb.intersections.size());

		int index = 0;
		for(int v = p.box.min[1]; v < p.box.max[1]; v++)
			for(int u = p.box.min[0]; u < p.box.max[0]; u++) {
				OutputLine line = p.output(index++);
				line.subtraction(pa.at(u, v), pb.at(u, v));
			}
		p.intersections.resize(p.indices.back());
	}
	isClean = false;
}

void Volume::intersectPlane(const Vec3f &n, const Vec3f &pos) {
	float d = pos*n/step;
	for(int i = 0; i < 3; i++) {
		Plane &p = planes[i];

		Vec3f sn = toLocal(n, i);
		if(p.indices.size() == 1) continue;

		Line next_line = p.at(0);
		for(int v = p.box.min[1], j = 0; v < p.box.max[1]; v++) {
			for(int u = p.box.min[0]; u < p.box.max[0]; u++, j++) {
				Line line = next_line;
				if(j != (int)p.indices.size()-1)
					next_line = p.at(j+1);

				float h = u*sn[0] + v*sn[1] - d; //constant part of the dot product, -d
				OutputLine output = p.output(j);
				bool wasInside = false;

				for(int k = 0; k < line.size(); k++) {
					bool isInside = (h + line.at(k).p*sn[2] > 0);
					if((isInside != wasInside) && (k%2))
#if STORE_NORMALS
						output.push_back( Intersection( -h/sn[2] , sn ) );
#else
						output.push_back(-h/sn[2]);
#endif

					if(isInside)
						output.push_back(line.at(k));

					wasInside = isInside;
				}
			}
		}
		p.intersections.resize(p.indices.back());
	}
	enforceConsistency();
	isClean = false;
}



void Volume::fastSweep(const Volume &v, Vec3f start, Vec3f end, int subsample ) {

	start /= v.step;
	end /= v.step;
	Vec3i starti( (int)start[0], (int)start[1], (int)start[2] );
	Vec3i endi( (int)end[0], (int)end[1], (int)end[2] );
	Vec3i diff = endi - starti;

	std::vector< Vec3i > step;

	Vec3i next = -diff;
	while (1) {
		Vec3i rest = next - next/2;
		next /= 2;
		if ( rest.SquaredNorm() == 0 ) break;
		step.push_back( rest );
		Log::debug << rest[0]<<rest[1]<<rest[2];
	}

	*this = v;
	for (int i=step.size()-1 ; i>=0 ; i-- ) {
		Volume a = *this;
		Volume b = *this;
		b.translate( step[i] );

		/*if (i==0) {
			b.adjustNormalsForSweep( start - end );
			a.adjustNormalsForSweep( end - start );
		}*/

		unification( a , b );
	}
	adjustNormalsForSweep( end - start );

	Volume a = *this;
	subsampled(a,subsample,Vec3i(0,0,0));

}

void Volume::sweep(const Volume &v, Vec3f start, Vec3f end, int subsample) {    

	step = v.step*subsample;
	start *= subsample/v.step;
	end *= subsample/v.step;
	Vec3i starti((int)start[0], (int)start[1], (int)start[2]);
	Vec3i endi((int)end[0], (int)end[1], (int)end[2]);

	Volume res;

	Vec3i diff = endi - starti;
	int max = 0;
	if(fabs(diff[1]) > fabs(diff[0])) max = 1;
	if(fabs(diff[2]) > fabs(diff[max])) max = 2;
	Vec3i r(diff[max]/2, diff[max]/2, diff[max]/2);

#define ORIGINAL_SWEEP
#ifdef ORIGINAL_SWEEP

	for(int i = starti[max]; true; diff[max]>0?i++:i--) {
		int k = i - starti[max];
		Vec3i d = starti + (diff*k + r)/diff[max];

		if(k == 0)
			res.subsampled(v, subsample, d);
		else {
			Volume a;
			a.subsampled(v, subsample, d);
			unification(res, a);
			res = *this; //TODO turn it into a swap (obviously)
		}
		if(i == endi[max]) break;
	}
	adjustNormalsForSweep( end - start );
#else
	//get man and mix for all directions (in normal (not subsampled space)
	Box3i box;
	for(int k = 0; k < 3; k++) {
		box.min[k] = std::min(starti[k], endi[k]);
		box.max[k] = std::max(starti[k], endi[k]);
	}
	box.min = (v.box.min + box.min - Vec3i(subsample, subsample, subsample)) / subsample;
	box.max = (v.box.max + box.max + Vec3i(subsample, subsample, subsample)) / subsample;
	resize(box);

	std::vector<Pos3i> line;
	for(int i = starti[max]; true; diff[max]>0?i++:i--) {
		int k = i - starti[max];
		Pos3i d = starti + (diff*k + r)/diff[max];
		line.push_back(d);
		if(i == endi[max]) break;
		//Log::debug << "d: " << d[0] << d[1] << d[2];
	}
	std::vector<Intersection> tmp_line, tmp_previous, tmp_acc;


	float scale = subsample;
	for(int i = 0; i < 3; i++) {
		Plane &p = planes[i];
		const Plane &pa = v.planes[i];
		for(int v = p.box.min[1], j = 0; v < p.box.max[1]; v++) {
			for(int u = p.box.min[0]; u < p.box.max[0]; u++, j++) {
				int len_previous = 0;

				for(unsigned int k = 0; k < line.size(); k++) {
					Pos3i d = toLocal(line[k], i);
					Line input = pa.at(u*subsample - d[0], v*subsample - d[1]);
					if(input.size() == 0) continue;
					//Log::debug() << "u: " << u << " pa.u" << u*subsample - d[0];
					tmp_line.resize(input.size());

					int len_line =0;
					OutputLine scaled(&*tmp_line.begin(), &len_line);
					scaled.scale(input, scale, -d[2]);

					tmp_acc.resize(tmp_previous.size() + input.size());
					Line previous(&*tmp_previous.begin(), len_previous);

					int len_acc = 0;
					OutputLine acc(&* tmp_acc.begin(), &len_acc);
					acc.unification(previous, Line(&*tmp_line.begin(), len_line));

					std::swap(tmp_previous, tmp_acc);
					len_previous = len_acc;

				}
				p.intersections.resize(p.intersections.size() + len_previous);
				OutputLine output = p.output(j);
				for(int k = 0; k < len_previous; k++)
					output.push_back(tmp_previous[k]);
			}
		}
		p.intersections.resize(p.indices.back());
	}
	enforceConsistency();
#endif
	isClean = false;
}

void Volume::subsampled(const Volume &a, int stepdiv, Vec3i offset) {
	step = a.step*stepdiv;
	box = a.box;
	box.min -= offset;
	box.min -= Vec3i(stepdiv, stepdiv, stepdiv);
	box.min /= stepdiv;
	box.max -= offset;
	box.max += Vec3i(stepdiv, stepdiv, stepdiv);
	box.max /= stepdiv;

	resize(box);

	float scale = stepdiv;
	for(int i = 0; i < 3; i++) {
		Plane &p = planes[i];
		const Plane &pa = a.planes[i];

		p.intersections.resize(pa.intersections.size());

		Vec3i local_offset = toLocal(offset, i);
		int index = 0;
		for(int v = p.box.min[1]; v < p.box.max[1]; v++) {
			for(int u = p.box.min[0]; u < p.box.max[0]; u++) {
				OutputLine output = p.output(index++);
				Line input = pa.at(u*stepdiv + local_offset[0], v*stepdiv + local_offset[1]);
				output.scale(input, scale, local_offset[2]);
			}
		}
		p.intersections.resize(p.indices.back());
	}
	enforceConsistency();
	isClean = false;
}

void Plane::translate(Vec3i d) {
	for(unsigned int i = 0; i < intersections.size(); i++)
		intersections[i].p += d[2];
	box.min[0] += d[0];
	box.min[1] += d[1];
	box.max[0] += d[0];
	box.max[1] += d[1];
}

void Volume::translate(Vec3i delta) {
	box.min += delta;
	box.max += delta;
	for(int i = 0; i < 3; i++)
		planes[i].translate(toLocal(delta, i));
}

void Volume::translate(Vec3f d) {
	Vec3i delta(int(round(d[0]/step)), int(round(d[1]/step)), int(round(d[2]/step)));
	translate(delta);
}


bool Volume::checkIntersectionParity() const {
	for(int i = 0; i < 3; i++) {
		const Plane &p = planes[i];
		for(int v = p.box.min[1]; v < p.box.max[1]; v++) {
			for(int u = p.box.min[0]; u < p.box.max[0]; u++) {
				Line line = p.at(u, v);
				if((line.len%2) == 1) {
					{
						std::ostringstream oss;
						for(int i =0 ; i < line.size(); i++)
							oss << line[i].p << ' ';
						Log::debug << oss.str();
					}

					//exit(-1);
				}
			}
		}
	}
	return true;
}

bool Volume::checkConsistency(Vec3i p) const {
	/*if (enforceConsistency(p) != -1 ) {
		Log::debug << "MERDA VERA davvero!";
	}*/

	bool a = planes[0].isInside(toLocal(p, 0));
	bool b = planes[1].isInside(toLocal(p, 1));
	bool c = planes[2].isInside(toLocal(p, 2));


	if(a != b || a != c) {
		//Log::debug() << "MERDA VERA!";
		//Log::debug() << "p: " << p[0] << p[1] << p[2];
		return false;
	}
	return true;
}


bool Volume::checkConsistency() const {
	int count = 0;
	int out_of_boundaries = 0;
	for(int i = 0; i < 3; i++) {
		int zmin = toLocal(box.min, i)[2];
		int zmax = toLocal(box.max, i)[2];

		const Plane &p = planes[i];
		for(int v = p.box.min[1]; v < p.box.max[1]; v++) {
			for(int u = p.box.min[0]; u < p.box.max[0]; u++) {
				const Line &line = p.at(u, v);
				for(int j = 0; j < line.size(); j++) {
					const Intersection &in = line[j];
					int w = (int)floor(in.p + 0.5);
					if (!checkConsistency(toGlobal(Vec3i(u, v, w),   i))) count++;
					if (!checkConsistency(toGlobal(Vec3i(u, v, w-1), i))) count++;
					if (!checkConsistency(toGlobal(Vec3i(u, v, w+1), i))) count++;
					if(in.p < zmin || in.p >= zmax) out_of_boundaries++;
				}
			}
		}
	}
	if(count)
		Log::debug<<"Found"<<count<<"inconsistencies ("<<count/3<<"x 3 = " << count/3*3 << ")";
	if(out_of_boundaries)
		Log::debug<<"Found"<<count<<"out of boundaries ("<< out_of_boundaries << ")";
	return count == 0;
}

#if STORE_NORMALS
void Volume::adjustNormalsForSweep( Vec3f d ){

	for (int p=0; p<3; p++)
		for (unsigned int i=0; i<planes[p].intersections.size(); i++) {
			Vec3f &n = planes[p].intersections[i].n;
			//if (n*d>0)
			n = Vec3f(0,0,0);//(d^n^d).normalized();
		}
}
#else
void Volume::adjustNormalsForSweep( Vec3f ){

}
#endif

bool Volume::enforceConsistency(Vec3i p) {
	bool inside[3];
	inside[0] = planes[0].isInside(toLocal(p, 0));
	inside[1] = planes[1].isInside(toLocal(p, 1));
	inside[2] = planes[2].isInside(toLocal(p, 2));

	if(inside[0] == inside[1] && inside[1] == inside[2]) return false;


	Intersection *in[3];
	float cost[3];

	for(int i = 0; i < 3; i++) {
		in[i] = planes[i].getClosest(toLocal(p, i)); //null if nothing is close by.
		if(in[i])
			cost[i] = fabs(in[i]->p - p[i]);
		else
			cost[i] = 1e20; //cannot fix by simply moving an intersection, infinite cost.
	}

	float total_cost[2] = { 0, 0 }; // 0/1 if moving everything outside/inside
	for(int i = 0; i < 3; i++)
		total_cost[!inside[i]] += cost[i];

	const float epsilon = 0.001;
	bool move_all_inside = (total_cost[true] < total_cost[false]);
	if(total_cost[move_all_inside] > 1) {
		Log::debug << "INCAZZATI!";
		return false;
	}

	for(int i = 0; i < 3; i++) {
		if(move_all_inside != inside[i]) {
			if(in[i]->p >= p[i])
				in[i]->p = p[i] - epsilon;
			else
				in[i]->p = p[i];

		}
	}
	return true;
}


bool Volume::enforceConsistency() {

	int fixed = 0;
	// first pass: detect which line to fix and where
	for(int i = 0; i < 3; i++) {
		const Plane &p = planes[i];
		for(int v = p.box.min[1]; v < p.box.max[1]; v++) {
			for(int u = p.box.min[0]; u < p.box.max[0]; u++) {
				const Line &line = p.at(u, v);
				for(int j = 0; j < line.size(); j++) {
					const Intersection &in = line[j];
					int w = (int)floor(in.p + 0.5);
					if(fabs(in.p - (float)w) < 0.1) {
						Vec3i pos = toGlobal( Vec3i(u, v, w), i);
						bool ie = enforceConsistency(pos);
						if(ie) fixed++;
					}
				}
			}
		}
	}

	//if(fixed) Log::debug << "Fixed " << fixed << "inconsistencies (the easy way)";
	return true;
}


int Volume::cleanUp() {
	if (isClean) return 0;
	int count = 0;
	for(int i = 0; i < 3; i++) {
		Plane &p = planes[i];

		if(p.indices.size() == 1) continue;

		Line next_line = p.at(0);
		for(unsigned int j = 0; j < p.indices.size()-1; j++) {
			Line line = next_line;
			if(j != p.indices.size()-1)
				next_line = p.at(j+1);

			OutputLine output = p.output(j);
			count += output.cleanUp(line);

		}
		p.intersections.resize(p.indices.back());
	}
	//Log::debug() << "Removed intersection in the same interval: " << count;
	isClean = true;
	return count;
}

int Volume::totalIntersections() const{
	return planes[0].intersections.size()+
			planes[1].intersections.size()+
			planes[2].intersections.size() ;
}

void Volume::toPointCloud(std::vector<Pos3f> &verts) const {
	verts.clear();
	verts.reserve( totalIntersections() );
	for(int i = 0; i < 3; i++) {
		const Plane &p = planes[i];
		for(int v = p.box.min[1]; v < p.box.max[1]; v++) {
			for(int u = p.box.min[0]; u < p.box.max[0]; u++) {
				Line line = p.at(u, v);
				for(int j = 0; j < line.size(); j++) {
					Intersection &intersection = line[j];
					verts.push_back(toGlobal(Pos3f(u, v, intersection.p), i)*step);
				}
			}
		}
	}
}

void Volume::optimizeQuadDiagonals(std::vector<int> &q, int nv){
	std::vector<int> val(nv,-6);
	for (int i=0; i<(int)q.size(); ) {
		val[ q[i++] ] +=2;
		val[ q[i++] ] +=1;
	}
	int count = 0;
	for (int i=0; i<(int)q.size(); i+=4) {
		int flipScore = 2 - val[q[i+0]]
				+ val[q[i+1]]
				- val[q[i+2]]
				+ val[q[i+3]];

		if (flipScore<0) {
			int tmp = q[i];
			q[i]=q[i+1];
			q[i+1]=q[i+2];
			q[i+2]=q[i+3];
			q[i+3]=tmp;
			count++;
			val[q[i]]++;
			val[q[i+1]]--;
			val[q[i+2]]++;
			val[q[i+3]]--;
		}
	}
	Log::debug << "Rotated" << count << "quads";
}
