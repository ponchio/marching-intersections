#ifndef LIB_BOX_H
#define LIB_BOX_H

#include "vec.h"

template <class Vec3> struct Box3 {
	Vec3 min, max;
	Box3() {
		SetNull();
	}
	Box3(const Vec3 &mi, const Vec3 &ma): min(mi), max(ma) {}

	void Set(const Vec3 &p) {
		min = max = p;
	}
	void SetNull() {
		min[0] = min[1] = min[2] = 1;
		max[0] = max[1] = max[2] = -1;
	}
	bool IsNull() const {
		return min[0] > max[0] || min[1] > max[1] || min[2] > max[2];
	}
	void Add(const Vec3 &p) {
		if(IsNull())
			Set(p);
		else {
			if(min[0] > p[0])
				min[0] = p[0];
			if(min[1] > p[1])
				min[1] = p[1];
			if(min[2] > p[2])
				min[2] = p[2];
			if(max[0] < p[0])
				max[0] = p[0];
			if(max[1] < p[1])
				max[1] = p[1];
			if(max[2] < p[2])
				max[2] = p[2];
		}
	}
	void Add(const Box3 &b) {
		if(b.IsNull())
			return;
		if(IsNull())
			*this=b;
		else {
			Add(b.min);
			Add(b.max);
		}
	}
	bool IsIn(const Vec3 &p) const {
		return min[0] <= p[0] && p[0] <= max[0] && min[1] <= p[1] && p[1] <= max[1] && min[2] <= p[2] && p[2] <= max[2];
	}
	void Intersect( const Box3<Vec3> & b ) {
		if(min[0] < b.min[0]) min[0] = b.min[0];
		if(min[1] < b.min[1]) min[1] = b.min[1];
		if(min[2] < b.min[2]) min[2] = b.min[2];

		if(max[0] > b.max[0]) max[0] = b.max[0];
		if(max[1] > b.max[1]) max[1] = b.max[1];
		if(max[2] > b.max[2]) max[2] = b.max[2];

		if(min[0] > max[0] || min[1] > max[1] || min[2] > max[2]) SetNull();
	}
	inline auto DimX() const { return max[0] - min[0]; }
	inline auto DimY() const { return max[1] - min[1]; }
	inline auto DimZ() const { return max[2] - min[2]; }
	inline auto Volume() const { return DimX() * DimY() * DimZ(); }
	inline auto Diag() const { 
		return std::sqrt(DimX()*DimX() + DimY()*DimY() + DimZ()*DimZ());
	}
	Vec3 Center() const { return (min + max)*0.5; }
};

using Box3i = Box3<Pos3i>;
using Box3f = Box3<Vec3f>;

template <class Pos2> struct Box2 {
	Pos2 min, max;
	Box2() { SetNull(); }
	Box2(const Pos2 &mi, const Pos2 &ma): min(mi), max(ma) {}

	void Set(const Pos2 &p) {
		min = max = p;
	}
	void SetNull() {
		min[0] = min[1] = 1;
		max[0] = max[1] = -1;
	}
	bool IsNull() const {
		return min[0] > max[0] || min[1] > max[1];
	}
	void Add(const Pos2 &p) {
		if(IsNull())
			Set(p);
		else {
			if(min[0] > p[0]) min[0] = p[0];
			if(min[1] > p[1]) min[1] = p[1];
			if(max[0] < p[0]) max[0] = p[0];
			if(max[1] < p[1]) max[1] = p[1];
		}
	}
	void Add(const Box2 &b) {
		if(b.IsNull()) return;
		if(IsNull()) *this = b;
		else {
			Add(b.min);
			Add(b.max);
		}
	}
	bool IsIn(const Pos2 &p) const {
		return min[0] <= p[0] && p[0] <= max[0] && min[1] <= p[1] && p[1] <= max[1];
	}

	void Intersect( const Box3<Pos2> & b ) {
		if(min.X() < b.min.X()) min.X() = b.min.X();
		if(min.Y() < b.min.Y()) min.Y() = b.min.Y();

		if(max.X() > b.max.X()) max.X() = b.max.X();
		if(max.Y() > b.max.Y()) max.Y() = b.max.Y();

		if(min.X()>max.X() || min.Y()>max.Y()) SetNull();
	}
	inline auto DimX() const { return max[0] - min[0]; }
	inline auto DimY() const { return max[1] - min[1]; }
};

using Box2i = Box2<Pos2i>;
using Box2f = Box2<Pos2f>;

#endif
