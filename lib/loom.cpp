
#include "log.h"

#include "loom.h"
#include "mc_table.h"

using namespace mi;
const int UNDEF = std::numeric_limits<int>::min();

/* class Loom: an helper class used to extract ("weave") a mesh froma a mi::volume
 *
 * Implementation notes:
 * --------------------
 *
 * - Linear time: each intersection of MI is processed at most 4 times (belongs to 4 "threads")
 *
 * - Metaphor:
 *   A "thread" in the "loom" is a a 2x2 grid of lines on X, Y, or Z.
 *   They form the "warp" and "weft" and... "hyper-weft" of the "loom".
 *
 * - At any given time, there's:
 *     1  active thread on X
 *     N  active threads on Y  (on a side array, parallel to the active X threads)
 *    NxN active threads on Z  (on a bottom grid)
 *
 * - The volume is swept searching for "virtual cubes",
 *   which would be non empty-cubes in a MC equivalent of the MI structure.
 *   A virtual cube needs to be completed (we need to know all its intersections on the 4X+4Y+4Z = 12 edges)
 *   before it is triangulated (using MC tables).
 *
 * - The volume is traversed in each dimension. In X,Y,Z order.
 *   Main traversal is on X, which can issue additional traversals on Y and/or Z.
 *   Traversals on Y can issue traversals on Z.
 *   Specifically:
 *   - when running a thread along X:
 *     as a virtual cube is found, it might be necessary to issue a run on active Y and/or Z threads, to complete it
 *   - when running on Y or Z, (always toward a "target cube" waiting to be completed):
 *     additional virtual cubes ("companion" cubes) might be encountered on the way. They are processed on the fly.
 *     - on Y:
 *       * it can be assumed that there's no intersection in the X direction (before target)
 *       * companion cubes might require a further run on Z to be completed
 *     - on Z:
 *       * it can be assumed that there's no intersection on either X or Y (target target)
 *       * companion cubes are solved directly. They are one of two simple MC configuration.
 *
 * - An active thread on X/Y/Z is represented by classes FourLinesX/Y/Z.
 *   They are different classes because:
 *    - they need different balances between memory / time (different number of active threads)
 *    - they can rely on different assumptions when traversed
 *
 * - All computations are done in relative spaces, where 0,0,0 is the first cube (not vol.box.min)
 *
 * - All thread are traversed backward (from end to start).
 */


Loom::Loom( const Volume &_vol ): vol(_vol) {

	idOffsetY = vol.planes[0].intersections.size()  ;
	idOffsetZ = vol.planes[1].intersections.size() + idOffsetY ;

	minX = vol.box.min[0];
	minY = vol.box.min[1];
	minZ = vol.box.min[2];
	dimX = vol.box.max[0] - minX;
	dimY = vol.box.max[1] - minY;
	dimZ = vol.box.max[2] - minZ;

}

void Loom::init(FourLinesX &f, int u, int v){

	const Plane &p (vol.planes[ 0 ]);

	int ind = p.relIndex( u , v );
	int w = p.width;
	int wind[4] = { ind, ind+1, ind+w, ind+w+1};

	for (int j=0; j < 4; j++) {
		f[j] = p.indices[ wind[j]+1 ] -1;
		f.last[j] = p.indices[ wind[j] ];
		f.d[j] = (f[j] < f.last[j])? UNDEF : (int)floor(p.intersections[ f[j] ].p);
	}
}


void Loom::init(FourLinesY &f, int u, int v){
	const Plane &p = vol.planes[1];

	int ind = p.relIndex( u , v );
	int w = p.width;

	f.vi0 = p.indices[ ind     +1 ] - 1;
	f.vi1 = p.indices[ ind+1   +1 ] - 1;
	f.vi2 = p.indices[ ind+w   +1 ] - 1;
	f.vi3 = p.indices[ ind+w+1 +1 ] - 1;

	f.last0 = p.indices[ ind   ] ;
	f.last1 = p.indices[ ind+1 ] ;

	f.pos0 = &(p.intersections[ f.vi0 ]);
	f.pos1 = &(p.intersections[ f.vi1 ]);

	f.d0 = (f.vi0<f.last0) ? UNDEF : (int)floor(f.pos0-- ->p);
	f.d1 = (f.vi1<f.last1) ? UNDEF : (int)floor(f.pos1-- ->p);

}


void Loom::init(FourLinesZ &f, int u, int v){
	const Plane &p (vol.planes[ 2 ]);

	int w = p.width;
	int i = u + v*w;
	f[0] = p.indices[ 1+i     ] - 1;
	f[1] = p.indices[ 1+i+1   ] - 1;
	f[2] = p.indices[ 1+i+w   ] - 1;
	f[3] = p.indices[ 1+i+w+1 ] - 1;

	f.last0 = p.indices[ i ] ;

}


void Loom::doLineX(int y, int z, std::vector<int>& faces ) {

	const Plane &p (vol.planes[ 0 ]);

	FourLinesX f;
	init( f, y, z );

	while (1) {
		// find next intersection on the 4 lines
		int x = std::max( std::max(f.d[0],f.d[1]), std::max(f.d[2],f.d[3]) );
		if (x == UNDEF) break; // line over

		CubeConf cube;

		for (int i = 0; i < 4; i++) {
			if ( f.d[i] == x ) {
				cube.mask |= (0x01<<(i*2 + (f[i] & 1) )); // set one vertex as present
				cube.edges[i] = (f[i]--) ;

				f.d[i] = (f[i] < f.last[i]) ? UNDEF : (int)floor(p.intersections[ f[i] ].p);

			}  else {
				if (f[i] & 1) cube.mask |= (0x03<<(i*2)); // all inside: set two vertices as present
			}
		}

		bool needsY = ( (cube.mask)^(cube.mask>>2) ) & 0x33;
		if (needsY)
			doLineY( z, x-minX, y+minY, cube, faces );

		bool needsZ = (cube.mask & 0x0F)!=( cube.mask>>4);
		if (needsZ)
			doLineZ( x-minX, y, z+minZ, cube, faces );

		McTable::addFaces( cube.mask, cube.edges, faces);
	}
}

void Loom::doLineY( int z, int x, int targetY,
				   CubeConf & targetCube,
				   std::vector<int>& faces)
{
	// assumption: no intersection in X direction, before target ( f[0] <=> f[2]  and  f[1] <=> f[3] )

	// z and x are in relative space, targetY in absolute space

	FourLinesY &f ( sideArray[ x ]);

	while(1) {
		int posY = std::max( f.d0, f.d1 );
		if (posY<=targetY) break;

		// found an unprocessed companion cube on the way: process it

		CubeConf cube;

		if (f.d0 == posY) {
			if ( f.vi0 & 1 ) cube.mask = 0x0C; else cube.mask = 0x03;
			cube.edges[4] = idOffsetY + f.vi0--;
			cube.edges[6] = idOffsetY + f.vi2--;
			f.d0 = (f.vi0<f.last0) ? UNDEF : (int)floor( f.pos0-- ->p );
		} else if ( f.vi0 & 1 ) cube.mask = 0x0F;

		if (f.d1 == posY) {
			if ( f.vi1 & 1 ) cube.mask |= 0xC0; else cube.mask |= 0x30;
			cube.edges[5] = idOffsetY + f.vi1--;
			cube.edges[7] = idOffsetY + f.vi3--;
			f.d1 = (f.vi1<f.last1) ? UNDEF : (int)floor( f.pos1-- ->p );
		} else if ( f.vi1 & 1 ) cube.mask |= 0xF0;

		// maybe the companion cube needs be completed on Z
		bool needsZ = (cube.mask & 0x0F)!=( cube.mask>>4);
		if (needsZ)
			doLineZ( x, posY-minY , z+minZ , cube, faces );

		McTable::addFaces( cube.mask, cube.edges, faces);
	}

	if (targetY == UNDEF) return; // we only had to comlpete the line

	// got to the target cube: add its intersections on Y

	int mask = targetCube.mask ^ (targetCube.mask>>2);

	if (mask&0x01)  {
		targetCube.edges[4] = idOffsetY + f.vi0--;
		f.d0 = (f.vi0<f.last0) ? UNDEF : (int)floor( f.pos0-- ->p );
	}
	if (mask&0x10)  {
		targetCube.edges[5] = idOffsetY + f.vi1--;
		f.d1 = (f.vi1<f.last1) ? UNDEF : (int)floor( f.pos1-- ->p );
	}
	if (mask&0x02)  {
		targetCube.edges[6] = idOffsetY + f.vi2--;
	}
	if (mask&0x20)  {
		targetCube.edges[7] = idOffsetY + f.vi3--;
	}
}

void Loom::doLineZ(int x, int y, int targetZ,
				   CubeConf & targetCube,
				   std::vector<int>&    faces)
{

	// assumption: no intersection in X or Y direction before target ( f[0] <=> f[1] <=> f[2] <=> f[3] )

	// x and z in relative space, targetY in absolute space
	const Plane &p (vol.planes[ 2 ]);
	FourLinesZ &f ( bottomGrid[ p.relIndex(x,y) ] );
	//int last0 =  p.indices[ p.relIndex(x,y) ]; // or, used stored one f.last0? balance memory / time efficency

	while ((f[0]>=f.last0) && (int)floor(p.intersections[ f[0] ].p) > targetZ) {
		// add a (quite trivial) companion cube
		CubeConf cube;

		cube.mask = (f[0] & 1)? 0xF0 : 0x0F;
		cube.edges[ 8] = (f[0]--) + idOffsetZ;
		cube.edges[ 9] = (f[1]--) + idOffsetZ;
		cube.edges[10] = (f[2]--) + idOffsetZ;
		cube.edges[11] = (f[3]--) + idOffsetZ;

		McTable::addFaces( cube.mask, cube.edges, faces);
	}

	if (targetZ == UNDEF) return; // we only had to comlpete the line

	// we got to targetCube: add its intersections on Z
	int mask = targetCube.mask ^ (targetCube.mask>>4);
	if (mask&1) targetCube.edges[8]  = (f[0]--) + idOffsetZ;
	if (mask&2) targetCube.edges[9]  = (f[1]--) + idOffsetZ;
	if (mask&4) targetCube.edges[10] = (f[2]--) + idOffsetZ;
	if (mask&8) targetCube.edges[11] = (f[3]--) + idOffsetZ;
}

void Loom::initSideArray(int z){
	sideArray.resize( dimX );
	for (unsigned int i=0; i<sideArray.size(); i++) {
		init( sideArray[i], z, i );
	}
}

void Loom::initBottomGrid(){
	bottomGrid.resize( dimX*dimY );
	for (int y=0; y<dimY-1; y++)
		for (int x=0; x<dimX-1; x++) {
			int i = vol.planes[2].relIndex(x,y);
			init(bottomGrid[i], x, y );
		}
}

void Loom::completeSideArray( int z, std::vector<int> &faces ){
	static CubeConf dummy;
	for (int x=0; x<dimX-1; x++) {
		doLineY( z, x, UNDEF, dummy, faces); // UNDEF => do line to end
	}

}

void Loom::completeBottomGrid( std::vector<int> &faces ){
	static CubeConf dummy;

	for (int y = 0; y < dimY-1; y++)
		for (int x=0; x < dimX-1; x++){
			doLineZ( x, y, UNDEF, dummy, faces ); // UNDEF => do line to end
		}
}


void Loom::weave(std::vector<int> &faces){

	McTable::init(); // needed only once, actually

	faces.reserve( vol.totalIntersections() * 6 ); // estimation

	initBottomGrid();

	for (int z = dimZ-2; z >= 0; z--) {
		initSideArray(z);
		for (int y = dimY-2; y >= 0; y--) {
			doLineX( y, z , faces );
		}

		completeSideArray( z, faces );
	}
	completeBottomGrid( faces );

	Log::debug << "Total "<< faces.size()/3 <<" faces weaved";
}
