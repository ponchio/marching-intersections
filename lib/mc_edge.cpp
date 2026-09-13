#include "mc_edge.h"
#include "mc_lookup_table.h"

#include "log.h"

using namespace mi;

int McEdge::edges_to_cases[1<<13];

void McEdge::init() {
	for(int i = 0; i < (1<<13); i++)
		edges_to_cases[i] = -1;

	for(int i = 0; i < 256; i++)
		edges_to_cases[McEdge::vertexMaskToEdgeMask(i)] = i;

	edges_to_cases[0x1000] = 0;
}

int McEdge::vertexMaskToEdgeMask(int v) {
	static int first[12] = { 0, 1, 3, 0,   4, 5, 7, 4,   0, 1, 2, 3 };
	int res = 0;
	int i = 0;
	int mink = 1000;
	while(1) {
		int k = vcg::tri::MCLookUpTable::CasesClassic(v, i);
		if(k == -1) break;
		if(mink > k) mink = k;
		res |= (1<<k);
		i++;
	}
	if(mink != 1000 && ((1<<first[mink]) & v)) {
		res |= (1<<12);
	}
	return res;
}


void McEdge::addEdge(int id, int parity, int edge_index) {
	if(min_edge > edge_index) {
		min_edge = edge_index;
		min_parity = parity;
	}
	ids[edge_index] = id;
	edge_mask |= (1<<edge_index);
	//edge_mask ^= (1<<edge_index); //works with double intersections also, but the min_edge way to determine inside or outside could fail.

}

void McEdge::triangulate(std::vector<int> &faces) {

	if(!min_parity)
		edge_mask |= (1<<12);

	int vertex_mask = edges_to_cases[edge_mask];
	if(vertex_mask < 0) {
		Log::debug << "AAAAAAAAARRRRRGGGGGGGHHHHHHHH MERDA VERA";
		//Log::debug() << "Unknown Edge mask: " <<  (void *)edge_mask;
		//exit(-1);
	}

	if(vertex_mask == 0) return;

	int i = 0;
	while(1) {
		int k = vcg::tri::MCLookUpTable::CasesClassic(vertex_mask, i);
		if(k == -1) break;
		faces.push_back(ids[k]);
		i++;
	}
}
