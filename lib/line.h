#ifndef LINE_H
#define LINE_H

#include "intersection.h"

namespace mi {

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

}  // namespace mi

#endif // LINE_H
