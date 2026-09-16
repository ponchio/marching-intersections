
#include "log.h"

#include "dual_loom.h"
#include "mc_table.h"

using namespace mi;
const int UNDEF = std::numeric_limits<int>::min();

/* class DualLoom: an helper class used to extract ("weave") a mesh froma a mi::volume */

DualLoom::DualLoom( const Volume &_vol ): vol(_vol) {

	idOffsetY = vol.planes[0].intersections.size()  ;
	idOffsetZ = vol.planes[1].intersections.size() + idOffsetY ;

	minX = vol.box.min[0];
	minY = vol.box.min[1];
	minZ = vol.box.min[2];
	dimX = vol.box.max[0] - minX;
	dimY = vol.box.max[1] - minY;
	dimZ = vol.box.max[2] - minZ;

}


void DualLoom::init(FourLinesX &f, int u, int v){

	const Plane &p (vol.planes[ 0 ]);

	int ind = p.relIndex( u , v );
	int w = p.width;
	int wind[4] = { ind, ind+1, ind+w, ind+w+1};

	for (int j=0; j<4; j++) {
		f[j] = p.indices[ wind[j]+1 ] -1;
		f.last[j] = p.indices[ wind[j] ];
		f.d[j] = (f[j]<f.last[j])? UNDEF : (int)floor(p.intersections[ f[j] ].p);
	}
}


void DualLoom::init(FourLinesY &f, int u, int v){
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


void DualLoom::init(FourLinesZ &f, int u, int v){
	const Plane &p (vol.planes[ 2 ]);

	int w = p.width;
	int i = u + v*w;
	f[0] = p.indices[ 1+i     ] - 1;
	f[1] = p.indices[ 1+i+1   ] - 1;
	f[2] = p.indices[ 1+i+w   ] - 1;
	f[3] = p.indices[ 1+i+w+1 ] - 1;

	f.last0 = p.indices[ i ] ;

}


void DualLoom::doLineX(int y, int z, std::vector<Pos3f>& verts, std::vector<int>& faces ) {

	const Plane &p (vol.planes[ 0 ]);

	FourLinesX f;
	init( f, y, z );

	while (1) {
		// find next intersection on the 4 lines
		int x = std::max( std::max(f.d[0],f.d[1]), std::max(f.d[2],f.d[3]) );
		if (x==UNDEF) break; // line over

		CubeConf cube;

		for (int i=0; i<4; i++) {
			if ( f.d[i] == x ) {
				cube.mask |= (0x01<<(i*2+ (f[i] & 1) )); // set one vertex as present
				cube.edges[i] = (f[i]--) ;

				f.d[i] = (f[i]<f.last[i]) ? UNDEF : (int)floor(p.intersections[ f[i] ].p);

			}  else {
				if (f[i] & 1) cube.mask |= (0x03<<(i*2)); // all inside: set two vertices as present
			}
		}

		bool needsY = ( (cube.mask)^(cube.mask>>2) ) & 0x33;
		if (needsY)
			doLineY( z, x-minX, y+minY, cube, verts, faces );

		bool needsZ = (cube.mask & 0x0F)!=( cube.mask>>4);
		if (needsZ)
			doLineZ( x-minX, y, z+minZ, cube, verts, faces );

		processCube( cube, Vec3i(x-minX, y,z ), verts, faces);
	}
}

void DualLoom::doLineY(int z, int x, int targetY,
					   CubeConf & targetCube,
					   std::vector<Pos3f> &verts,
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
			doLineZ( x, posY-minY , z+minZ , cube, verts, faces );

		processCube( cube, Vec3i(x,posY-minY,z), verts, faces);
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

struct Averager{
	Vec3f sum;
	int c;
	Averager():sum(0,0,0),c(0){}
	void addPoint( Vec3f p){ sum+=p; c++; }
	Vec3f solve() const{ return sum/c; }
};

/* PlaneSytem: an helper class.
 * Given a set of points + planes passing through them, it gives:
 * - if all the planes claerly intersect on a point, that point;
 * - if all the planes clearly intersect on a line, a point on that line
 *   (closest to the given points)
 * - the average of the given points, otherwise
*/
struct PlaneSystem{
#define yx xy
#define zy yz
#define xz zx
	PlaneSystem(){init();}
	float xx,yy,zz,xy,yz,zx; // coefficients of the 3x3 Jacobian matrix
	float dx,dy,dz;
	Vec3f sum;
	int avrgDiv;

	void init(){
		xx=yy=zz=xy=yz=zx=sum[0]=sum[1]=sum[2]=avrgDiv=0;
		dx = dy = dz = 0;
	}

	void addPlane( Vec3f p, Vec3f n){
		xx += n[0]*n[0];
		yy += n[1]*n[1];
		zz += n[2]*n[2];
		xy += n[0]*n[1];
		yz += n[1]*n[2];
		zx += n[2]*n[0];
		float d = -n.dot(p);
		dx -= d*n[0];
		dy -= d*n[1];
		dz -= d*n[2];
		sum += p;
		avrgDiv++;
	}

	Vec3f solve(){
		Vec3f smooth = sum / avrgDiv;

		// add a small bias to the energy, wanting the returned point
		// intersection to be exactly in the averaged position
		// (just enough to disambiguate when planes intersect barely)
		float w = 0.020 * avrgDiv; // weight of this term
		xx+=w;
		yy+=w;
		zz+=w;
		dx += smooth[0]*w;
		dy += smooth[1]*w;
		dz += smooth[2]*w;


		float det = +zz*xx*yy
					-xx*yz*yz
					-yy*zx*zx
					-zz*xy*xy
					+xy*yz*zx*2;
		float detX = yy*zz - yz*yz;
		float detY = zz*xx - zx*zx;
		float detZ = xx*yy - xy*xy;

		float ssXY = xz*yz - xy*zz;
		float ssZX = zy*xy - zx*yy;
		float ssYZ = zx*yx - yz*xx;

		//if (fabs(det)>0.25*avrgDiv);
		{
			// clear intersections at a point
			return Vec3f( detX * dx + ssXY *dy + ssZX * dz ,
						 ssXY * dx + detY *dy + ssYZ * dz ,
						 ssZX * dx + ssYZ *dy + detZ * dz ) / det;
		}

		//return smooth;
		/*
		float best = std::max( std::max( fabs(detX), fabs(detY) ), fabs(detZ) );
		if (best>0.2*avrgDiv) {
			// clear intersection on a line
			if (best==fabs(detX)) {
				float fx = smooth[0];
				return Vec3f(   fx,
							 (    +zz*(dy-yx*fx) - yz*(dz-zx*fx))/detX,
							 (    -yz*(dy-yx*fx) + yy*(dz-zx*fx))/detX
							);
			}
			else if (best==fabs(detY)) {
				float fy = smooth[1];
				return Vec3f(
							 (+zz*(dx-xy*fy)     - zx*(dz-zy*fy))/detY,
											  fy,
							 (-zx*(dx-xy*fy)     + xx*(dz-zy*fy))/detY
							);
			} else {
				float fz = smooth[2];
				return Vec3f(
							 (+yy*(dx-xz*fz) - xy*(dy-yz*fz)    )/detZ,
							 (-xy*(dx-xz*fz) + xx*(dy-yz*fz)    )/detZ,
															 fz
							);
			}
		}

		// 3x3 system and all 2x2 subsystems failed
		return smooth;*/
	}

	Vec3f solveAndClamp(){
		return clamp( solve() );
	}

	static float clamp(float f) {
		return std::max(0.0f,std::min(1.0f,f));
	}

	static Vec3f clamp(Vec3f p) {
		return Vec3f( clamp(p[0]), clamp(p[1]),clamp(p[2]));
	}


#undef yx
#undef zy
#undef yx
};

void DualLoom::processCube(const CubeConf &cube, const Vec3i& pos, std::vector<Pos3f> &verts, std::vector<int> &faces ){

	//int remap[4] = {0,1,3,2};
	int remap[4] = {3,0,2,1};
	int vi = (int)verts.size();
	for (int e=0; e<12; e++) {
		int fi = cube.edges[e];
		if (fi<0) continue;
		faces[fi*4 + remap[e&3] ] = vi + McTable::polygonOfEdge[cube.mask][e];
	}

	int vertsToAdd = McTable::polygonCount[cube.mask];
	//if (vertsToAdd>1) Log::debug << "I avoided a non manifold vertex!!!";

#if 0
	// blocky:
	for (int i=0; i<vertsToAdd; i++)
		verts.push_back( Vec3f(
							 pos[0]+minX+0.5,
						 pos[1]+minY+0.5,
				pos[2]+minZ+0.5
				) );
#elif STORE_NORMALS
	// creases:
	PlaneSystem sys[10];
	for (int e=0,pi=0,offset=0; pi<3; pi++) {
		const Plane& p( vol.planes[pi]);
		for (int dv=0; dv<2; dv++) {
			for (int du=0; du<2; du++,e++){
				int fi = cube.edges[e];
				if (fi<0) continue;
				int vi = McTable::polygonOfEdge[cube.mask][e];
				if (vi==-1) error();
				const Intersection& in (p.intersections[ fi - offset ]);
				float dw = in.p - floor(in.p);
				Pos3f p = Volume::toGlobal( Vec3f( du, dv, dw) , pi );
				sys[vi].addPlane( p , Volume::toGlobal( in.n, pi )  ) ;
			}
		}
		offset+=p.intersections.size();
	}
	for (int vi=0; vi<vertsToAdd; vi++)
		verts.push_back( sys[vi].solve() + Vec3f( pos[0], pos[1], pos[2] ) );
#else
	// smooth
	Averager sys[10];
	for (int e=0,pi=0,offset=0; pi<3; pi++) {
		const Plane& p( vol.planes[pi]);
		for (int dv=0; dv<2; dv++) {
			for (int du=0; du<2; du++,e++){
				int fi = cube.edges[e];
				if (fi<0) continue;
				int vi = McTable::polygonOfEdge[cube.mask][e];
				if (vi==-1) error();
				const Intersection& in (p.intersections[ fi - offset ]);
				float dw = in.p - floor(in.p);
				Pos3f p = Volume::toGlobal( Vec3f( du, dv, dw) , pi );
				sys[vi].addPoint( p ) ;
			}
		}
		offset+=p.intersections.size();
	}
	for (int vi=0; vi<vertsToAdd; vi++)
		verts.push_back( sys[vi].solve() + Vec3f( pos[0], pos[1], pos[2] ) );
#endif

}

void DualLoom::doLineZ(int x, int y, int targetZ,
					   CubeConf & targetCube,
					   std::vector<Pos3f>& verts,
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

		int tmpz = (int)floor(p.intersections[ f[0]+1 ].p) - minZ;
		processCube( cube, Vec3i(x,y, tmpz), verts, faces);
	}

	if (targetZ == UNDEF) return; // we only had to comlpete the line

	// we got to targetCube: add its intersections on Z
	int mask = targetCube.mask ^ (targetCube.mask>>4);
	if (mask&1) targetCube.edges[8]  = (f[0]--) + idOffsetZ;
	if (mask&2) targetCube.edges[9]  = (f[1]--) + idOffsetZ;
	if (mask&4) targetCube.edges[10] = (f[2]--) + idOffsetZ;
	if (mask&8) targetCube.edges[11] = (f[3]--) + idOffsetZ;
}

void DualLoom::initSideArray(int z){
	sideArray.resize( dimX );
	for (unsigned int i=0; i<sideArray.size(); i++) {
		init( sideArray[i], z, i );
	}
}

void DualLoom::initBottomGrid(){
	bottomGrid.resize( dimX*dimY );
	for (int y=0; y<dimY-1; y++)
		for (int x=0; x<dimX-1; x++) {
			int i = vol.planes[2].relIndex(x,y);
			init(bottomGrid[i], x, y );
		}
}

void DualLoom::completeSideArray(int z, std::vector<Pos3f> &verts,  std::vector<int> &faces ){
	static CubeConf dummy;
	for (int x=0; x<dimX-1; x++) {
		doLineY( z, x, UNDEF, dummy, verts, faces); // UNDEF => do line to end
	}

}

void DualLoom::completeBottomGrid( std::vector<Pos3f>& verts, std::vector<int> &faces ){
	static CubeConf dummy;

	for (int y=0; y<dimY-1; y++)
		for (int x=0; x<dimX-1; x++){
			doLineZ( x, y, UNDEF, dummy, verts, faces ); // UNDEF => do line to end
		}
}


void DualLoom::weave(std::vector<Pos3f> &verts, std::vector<int> &faces){

	faces.resize( vol.totalIntersections() * 4 );

	verts.reserve( vol.totalIntersections() ); // estimation

	McTable::init(); // needed only once, actually

	initBottomGrid();

	for (int z = dimZ-2; z>=0; z--) {
		initSideArray(z);
		for (int y = dimY-2; y>=0; y--) {
			doLineX( y, z , verts, faces );
		}

		completeSideArray( z, verts, faces );
	}
	completeBottomGrid( verts, faces );

	//flipAllEvenFaces
	for (int i=0; i<(int)faces.size(); i+=8) {
		//std::swap( faces[i] , faces[i+2] );
		std::swap( faces[i] , faces[i+1] );
		std::swap( faces[i+2] , faces[i+3] );
	}

	for (int i=0; i<(int)verts.size(); i++) {
		Vec3f min = vol.box.min;
		verts[i] += min;
		verts[i] *= vol.step;
	}


	Log::debug << "Total "<< faces.size()/4 <<" quads and "<<verts.size()<<"verts weaved (DUAL)";
}

void DualLoom::error(int code){
	Log::debug << "MERDA VERA "<< code;
	exit(0);
}
