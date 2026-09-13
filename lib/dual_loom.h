#ifndef DUAL_LOOM_H
#define DUAL_LOOM_H

#include <vector>
#include "intersections.h"
#include "loom.h"

namespace mi {


typedef unsigned char CubeMask;
struct FourLinesX;
struct FourLinesY;
struct FourLinesZ;
struct CubeConf;

class DualLoom{
public:

	DualLoom( const Volume &_vol );

	void weave(std::vector<Pos3f> &verts, std::vector<int> &quad_faces );

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
	void completeSideArray(int z, std::vector<Pos3f>& verts, std::vector<int> &faces );
	void completeBottomGrid(std::vector<Pos3f> &verts, std::vector<int> &faces );

	void init(FourLinesX &f, int u, int v );
	void init(FourLinesY &f, int u, int v );
	void init(FourLinesZ &f, int u, int v );

	void doLineX( int y, int z , std::vector<Pos3f>& verts, std::vector<int> &faces);
	void doLineY( int z, int x , int targetY, CubeConf &cubeConf , std::vector<Pos3f> &verts, std::vector<int> &faces );
	void doLineZ(int x, int y , int targetZ, CubeConf &cubeConf , std::vector<Pos3f> &verts, std::vector<int> &faces );

	void processCube(const CubeConf &cube, const Vec3i &pos, std::vector<Pos3f> &verts, std::vector<int> &faces );

	void error(int code=0);
};


}

#endif // LOOM_H
