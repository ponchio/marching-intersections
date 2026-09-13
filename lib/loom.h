#ifndef LOOM_H
#define LOOM_H

#include <vector>
#include "intersections.h"

namespace mi {


typedef unsigned char CubeMask;
struct FourLinesX;
struct FourLinesY;
struct FourLinesZ;
struct CubeConf;

class Loom{
public:

	Loom( const Volume &_vol );

	void weave( std::vector<int> &faces );

private:
	std::vector< FourLinesZ > bottomGrid; // a 2D grid of threads, on the bottom
	std::vector< FourLinesY > sideArray; // a 1D grid of threads, on the side

	const Volume & vol;

	// redundant variables, from vol
	int idOffsetZ, idOffsetY;
	int minX, minY, minZ;
	int dimX, dimY, dimZ;

	void initSideArray(int z);
	void initBottomGrid();
	void completeSideArray(int z, std::vector<int> &faces );
	void completeBottomGrid( std::vector<int> &faces );

	void init(FourLinesX &f, int u, int v );
	void init(FourLinesY &f, int u, int v );
	void init(FourLinesZ &f, int u, int v );

	void doLineX( int y, int z , std::vector<int> &faces);
	void doLineY( int z, int x , int targetY, CubeConf &cubeConf ,std::vector<int> &faces );
	void doLineZ( int x, int y , int targetZ, CubeConf &cubeConf ,std::vector<int> &faces );

};

struct FourLines{
	// a 2x2 patch of oriziontal lines, i.e. a thread in the loom

	int c[4]; // current pos in this line. Goes backward
	int& operator[] (int i) {return c[i];}
};

struct FourLinesX:public FourLines{
	// specialization for lines over X
	int last[4];
	int d[4]; // 4 precomputed depths
};

struct FourLinesY{

	int vi0,vi1,vi2,vi3;
	int last0, last1; // how many left
	int d0, d1;

	const Intersection *pos0, *pos1;

};

struct FourLinesZ:public FourLines{
	int last0;
};

// configuration of a virtual cube
struct CubeConf{

	CubeMask mask;
	int edges[12];

	void clean(){
		for (int i=0; i<12; i++) edges[i]=-1;
	}

	CubeConf():mask(0x00) {
		clean(); // only needed for testing
	}
};


}

#endif // LOOM_H
