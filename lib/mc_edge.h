#ifndef MC_EDGE_H
#define MC_EDGE_H

#include <vector>

// a class used only by the obsolete triangulate Old:
// a MC table indexed by edge rather than by vertice.

namespace mi {

class McEdge {
public:
	McEdge(): min_edge(1000), edge_mask(0) {}
	static void init();
	void addEdge(int id, int parity, int edge_index);
	void triangulate(std::vector<int> &faces); //append faces


private:
	static int vertexMaskToEdgeMask(int e);
	static int edges_to_cases[1<<13];

	int min_edge;
	int min_parity;
	int ids[12];
	int edge_mask;
};

}  // namespace mi

#endif // MC_EDGE_H
