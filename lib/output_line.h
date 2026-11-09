#ifndef OUTPUT_LINE_H
#define OUTPUT_LINE_H

#include "line.h"

namespace mi {

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

}  // namespace mi

#endif // OUTPUT_LINE_H
