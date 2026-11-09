#include "output_line.h"

#include <cmath>

namespace mi {

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

}  // namespace mi
