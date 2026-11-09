#include "line.h"

#include <utility>

namespace mi {

void Line::insert(Intersection a) {
	int i = 0;
	while(a.p > at(i).p) i++;

	while(at(i).p == at(i).p) {
		std::swap(at(i++), a);
	}
	at(i) = a;
}

}  // namespace mi
