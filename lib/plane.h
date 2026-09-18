#ifndef PLANE_H
#define PLANE_H

#include "log.h"
#include "box.h"
#include <vector>

namespace mi {

class Loom;
class DualLoom;


class Intersection {
public:
	float p;
#if STORE_NORMALS
	Vec3f n;
#endif
	//add your attributes here.

	Intersection() {}
#if STORE_NORMALS
	Intersection(float _p, Vec3f _n): p(_p),n(_n) {}
#else
	Intersection(float _p): p(_p) {}
#endif

	bool operator<(const Intersection &b) const { return p < b.p; }
	bool operator>(const Intersection &b) const { return p > b.p; }
	bool operator==(const Intersection &b) const { return p == b.p; }
#if STORE_NORMALS
	Intersection flip() const { return Intersection(p,-n); }
	Intersection operator-(int i) const { return Intersection(p-i,n); }
	Intersection operator/(int i) const { return Intersection(p/i,n); }
#else
	Intersection flip() const { return Intersection(p); }
	Intersection operator-(int i) const { return Intersection(p-i); }
	Intersection operator/(int i) const { return Intersection(p/i); }
#endif
};


class Line {
public:

	Intersection *in;
	int len; //how many intersections belong to this line.
	Line(Intersection *_in = NULL, int _len = 0): in(_in), len(_len) {}

	Intersection &at(int i) { return in[i]; }
	const Intersection &at(int i) const { return in[i]; }
	Intersection &operator[](int i) { return in[i]; }
	const Intersection &operator[](int i) const { return in[i]; }

	int size() const { return len; }
	void resize(int i) { len = i; }

	void insert(Intersection a);
};

/* Temporary structure to perform operations and save result in
plane intersection array. */

class OutputLine {
public:
	Intersection *start; //not the start of the line, it's the start of the plane!
	int *final_position; //this points to the index array.

	OutputLine(Intersection *_start, int *counter): start(_start), final_position(counter) {}

	void push_back(const Intersection &i) { start[(*final_position)++] = i; }

	//join intervals if end == start of the next.
	int cleanUp(const Line &a);

	int scaleAndCleanup(const Line &a, int scale, int add);
	void scale(const Line &a, int scale, int add);
	void unification(const Line &a, const Line &b); //damn you, reserved 'union' keyword!
	void intersection(const Line &a, const Line &b);
	void subtraction(const Line &a, const Line &b);
};

class Plane {
public:
	Box2i box;

	std::vector<int> indices;
	std::vector<Intersection> intersections;

	Plane() { indices.push_back(0); }
	Plane(Box2i _box) { resize(_box); }

	void resize(Box2i _box) { box = _box; width = box.DimX(); indices.clear(); indices.resize(box.DimX()*box.DimY()+1, 0); }

	int index(int u, int v) const {
		u -= box.min[0];
		v -= box.min[1];
		return u + v*width;
	}

	int relIndex(int u, int v) const {
		return u + v*width;
	}
	
	Line at(int i) {
		int offset = indices[i];
		int len = indices[i + 1] - offset;
		return Line(&intersections[offset], len);
	}

	Line at(int u, int v) {
		if(u >= box.max[0] || v >= box.max[1] || u < box.min[0] || v < box.min[1]) {
			Log::debug << "Ciccia: u: " << u << " v: " << v;
			Log::debug << box.min[0] << box.min[1] << "-" << box.max[0]  << box.max[1];
			throw std::string("Out of bound at arguments in plane.");
		}
		int i = index(u, v);
		int offset = indices[i];
		int len = indices[i + 1] - offset;
		return Line(&intersections[offset], len);
	}

	const Line at(int u, int v) const {
		static Line empty_line;
		if(u >= box.max[0] || v >= box.max[1] || u < box.min[0] || v < box.min[1])
			return empty_line;

		int i = index(u, v);
		int offset = indices[i];
		int len = indices[i + 1] - offset;

		std::vector<Intersection> &vaf = *(std::vector<Intersection> *)&intersections;
		return Line(&vaf[offset], len);
	}

	OutputLine output(int index) {
		indices[index+1] = indices[index];
		return OutputLine(&*intersections.begin(), &indices[index+1]);
	}


	void translate(Vec3i d);
	void accumulateLengths();
	void allocate();
	void fillTriangle(const Pos3f &a, const Pos3f &b, const Pos3f &c, std::vector<std::pair<int, Intersection> > & result); //a, b, c in local coordinates, n in object space
	void fillTriangleOld(const Pos3f &a, const Pos3f &b, const Pos3f &c, std::vector<std::pair<int, Intersection> > & result); //a, b, c in local coordinates, n in object space

	bool isInside(Vec3i p) const;
	Intersection *getClosest(Vec3i p);



private:
	int width;
	friend class Loom;
	friend class DualLoom;
};

}  // namespace mi

#endif // PLANE_H
