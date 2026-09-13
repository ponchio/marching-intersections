#ifndef MC_TABLE_H
#define MC_TABLE_H
#include <vector>

/*
 * Vertex / edgs enumeration:
 *         6 ________ 7           _____3__
 *         /|       /|         5/|       /|
 *       /  |     /  |        /  |     /7 |
 *   4 /_______ /    |      /__2____ /    11    Z
 *    |     |  |5    |     |    10  |     |     |
 *    |    2|__|_____|3    |     |__|__1__|     |      Y
 *    |    /   |    /      8   4/   9    /      |    /
 *    |  /     |  /        |  /     |  /6       |  /
 *    |/_______|/          |/___0___|/          |/______ X
 *   0          1
 *
 * Note:
 * Vertex index in binary is its own coords: ZYX
 *
 * Edge index in binary is: AAUV
 *  AA: which direction        (0=X, 1=Y, 2=Z)
 *   V: coord in the next axis:  Y,   Z,   X
 *   U: coord in the last axis:  Z    X,   Y
 */

namespace mi {

typedef unsigned char CubeMask;

class McTable
{
public:
	static void init();
	static void addFaces(CubeMask cm, int* vertIds, std::vector<int> &faces); //append faces

	// tables are indexes for each vertex configuration
	static int table[256][16]; // list of triangles (triplets of numbers, -1 terinated)
	static int polygonCount[256]; // how many separate polygons inside given configuration
	static int polygonOfEdge[256][12]; // to which polygon each given edge belong
	static int isto[10];

	static void test( std::vector< Pos3f >& res );
	static void test2(  );
	static void debugSearchFlawed(  );
private:
	static void convertFromVCG();
	static void computePolygonsFromFaces();
	static void computePolygonsFromSkretch();
	static void stats();

	static void debug_conf( std::vector< Pos3f >& res , CubeMask m, Pos3f t  );



};

}
#endif // MC_TABLE_H
