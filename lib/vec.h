#ifndef LIB_VEC_H
#define LIB_VEC_H

#include <cmath>

template<typename T>
struct Vec2 {
	T v[2];
	Vec2() { v[0]=v[1]=T(); }
	Vec2(T x, T y){ v[0]=x; v[1]=y; }
	T& operator[](int i){ return v[i]; }
	const T& operator[](int i) const { return v[i]; }
	Vec2 operator+(const Vec2&o) const { return Vec2(v[0]+o.v[0], v[1]+o.v[1]); }
	Vec2 operator-(const Vec2&o) const { return Vec2(v[0]-o.v[0], v[1]-o.v[1]); }
	Vec2 operator*(T s) const { return Vec2(v[0]*s, v[1]*s); }
	Vec2 operator/(T s) const { return Vec2(v[0]/s, v[1]/s); }
	Vec2 operator-() const { return Vec2((T)-v[0], (T)-v[1]); }
	static inline Vec2 Construct(const Vec2 &b) { return b; }
	template<class Q> static inline Vec2 Construct(const Vec2<Q> &b) { return Vec2((T)b[0],(T)b[1]); }
	template<class Q> static inline Vec2 Construct(const Q &a, const Q &b) { return Vec2((T)a,(T)b); }
	static inline Vec2 Zero() { return Vec2((T)0,(T)0); }
	static inline Vec2 One() { return Vec2((T)1,(T)1); }
	inline const T & X() const { return v[0]; }
	inline const T & Y() const { return v[1]; }
	inline T & X() { return v[0]; }
	inline T & Y() { return v[1]; }
	template<typename Q> Vec2(const Vec2<Q> &b) { v[0] = (T)b[0]; v[1] = (T)b[1]; }
	template<typename Q> Vec2 & operator=(const Vec2<Q> &b) { v[0] = (T)b[0]; v[1] = (T)b[1]; return *this; }
};

using Vec2f = Vec2<float>;
using Pos2f = Vec2<float>;
using Vec2i = Vec2<int>;
using Pos2i = Vec2<int>;

// External 2D cross product: returns scalar (z component) following Eigen-like conventions
template<typename T>
inline T cross(const Vec2<T> &a, const Vec2<T> &b) {
	return a[0]*b[1] - a[1]*b[0];
}

template<typename T>
struct Vec3T {
	T v[3];
	Vec3T(){ v[0]=v[1]=v[2]=T(); }
	Vec3T(T x,T y,T z){ v[0]=x; v[1]=y; v[2]=z; }
	T& operator[](int i){ return v[i]; }
	const T& operator[](int i) const { return v[i]; }
	Vec3T operator+(const Vec3T&o) const { return Vec3T(v[0]+o.v[0], v[1]+o.v[1], v[2]+o.v[2]); }
	Vec3T operator-(const Vec3T&o) const { return Vec3T(v[0]-o.v[0], v[1]-o.v[1], v[2]-o.v[2]); }
	Vec3T operator*(T s) const { return Vec3T(v[0]*s, v[1]*s, v[2]*s); }
	inline T operator*(const Vec3T & p) const { return dot(p); }
	Vec3T operator/(T s) const { return Vec3T(v[0]/s, v[1]/s, v[2]/s); }
	Vec3T operator-() const { return Vec3T((T)-v[0], (T)-v[1], (T)-v[2]); }
	Vec3T cross(const Vec3T&o) const {
		return Vec3T(v[1]*o.v[2]-v[2]*o.v[1], v[2]*o.v[0]-v[0]*o.v[2], v[0]*o.v[1]-v[1]*o.v[0]);
	}
	T dot(const Vec3T&o) const { return v[0]*o.v[0]+v[1]*o.v[1]+v[2]*o.v[2]; }
	inline T Norm() const { return std::sqrt(dot(*this)); }
	inline T SquaredNorm() const { return dot(*this); }
	inline Vec3T & Normalize() { T n = Norm(); if(n> (T)0) { v[0]/=n; v[1]/=n; v[2]/=n; } return *this; }
	inline void normalize() { Normalize(); }
	inline Vec3T normalized() const { Vec3T r = *this; r.Normalize(); return r; }

	inline Vec3T & operator += ( const Vec3T & o ) { v[0]+=o.v[0]; v[1]+=o.v[1]; v[2]+=o.v[2]; return *this; }
	inline Vec3T & operator -= ( const Vec3T & o ) { v[0]-=o.v[0]; v[1]-=o.v[1]; v[2]-=o.v[2]; return *this; }
	inline Vec3T & operator *= ( const T s ) { v[0]*=s; v[1]*=s; v[2]*=s; return *this; }
	inline Vec3T & operator /= ( const T s ) { v[0]/=s; v[1]/=s; v[2]/=s; return *this; }

	static inline Vec3T Construct(const Vec3T &b) { return b; }
	template<class Q> static inline Vec3T Construct(const Vec3T<Q> &b) { return Vec3T((T)b[0],(T)b[1],(T)b[2]); }
	template<class Q> static inline Vec3T Construct(const Q &a, const Q &b, const Q &c) { return Vec3T((T)a,(T)b,(T)c); }
	static inline Vec3T Zero() { return Vec3T((T)0,(T)0,(T)0); }
	static inline Vec3T One() { return Vec3T((T)1,(T)1,(T)1); }

	inline const T & X() const { return v[0]; }
	inline const T & Y() const { return v[1]; }
	inline const T & Z() const { return v[2]; }
	inline T & X() { return v[0]; }
	inline T & Y() { return v[1]; }
	inline T & Z() { return v[2]; }
	inline size_t MaxCoeffId() const { if(v[0]>v[1]) return v[0]>v[2]?0:2; else return v[1]>v[2]?1:2; }
	template<typename Q> Vec3T(const Vec3T<Q> &b) { v[0] = (T)b[0]; v[1] = (T)b[1]; v[2] = (T)b[2]; }
	template<typename Q> Vec3T & operator=(const Vec3T<Q> &b) { v[0] = (T)b[0]; v[1] = (T)b[1]; v[2] = (T)b[2]; return *this; }
};

// External cross product following Eigen conventions: `cross(a,b)`
template<typename T>
inline Vec3T<T> cross(const Vec3T<T> &a, const Vec3T<T> &b) {
	return Vec3T<T>(
		a[1]*b[2] - a[2]*b[1],
		a[2]*b[0] - a[0]*b[2],
		a[0]*b[1] - a[1]*b[0]
		);
}

// Eigen-like operator^ overloads: `a ^ b` performs cross product
template<typename T>
inline Vec3T<T> operator^(const Vec3T<T> &a, const Vec3T<T> &b) { return cross(a,b); }

template<typename T>
inline T operator^(const Vec2<T> &a, const Vec2<T> &b) { return cross(a,b); }

// Lexicographic operator< for Vec2 and Vec3
template<typename T>
inline bool operator<(const Vec2<T> &a, const Vec2<T> &b) {
	if(a[0] < b[0]) return true;
	if(a[0] > b[0]) return false;
	return a[1] < b[1];
}

template<typename T>
inline bool operator<(const Vec3T<T> &a, const Vec3T<T> &b) {
	if(a[0] < b[0]) return true;
	if(a[0] > b[0]) return false;
	if(a[1] < b[1]) return true;
	if(a[1] > b[1]) return false;
	return a[2] < b[2];
}

// operator> implemented via operator<
template<typename T>
inline bool operator>(const Vec2<T> &a, const Vec2<T> &b) { return b < a; }

template<typename T>
inline bool operator>(const Vec3T<T> &a, const Vec3T<T> &b) { return b < a; }

using Vec3f = Vec3T<float>;
using Pos3f =Vec3T<float>;
using Vec3i = Vec3T<int>;
using Pos3i = Vec3T<int>;

#endif
