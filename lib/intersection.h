#ifndef INTERSECTION_H
#define INTERSECTION_H

#include "vec.h"

namespace mi {

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

}  // namespace mi

#endif // INTERSECTION_H
