#include <assert.h>

#include "log.h"


#include "mc_lookup_table.h"

#include "vec.h"

#include "mc_table.h"

using namespace mi;

void McTable::init(){
	convertFromVCG();
	computePolygonsFromFaces();
	//stats();
	//test();
}

void McTable::test2(){
	convertFromVCG();
	computePolygonsFromFaces();
	Log::debug << "Computing polygons with face duals:";
	stats();
	debugSearchFlawed();

	Log::debug << "";

	Log::debug << "Computing polygons from vertices in/out:";
	computePolygonsFromSkretch();
	stats();
	debugSearchFlawed();

}

int McTable::table[256][16];
int McTable::polygonCount[256];
int McTable::polygonOfEdge[256][12];
int McTable::isto[10];


void McTable::computePolygonsFromSkretch(){

	// edge to vert connectivity
	int e2v[12][2];

	for (int p=0,e=0; p<3; p++)
		for (int v=0; v<2; v++)
			for (int u=0; u<2; u++,e++){
				for (int w=0; w<2; w++) {
					int pos[3];
					pos[(p+0)%3] = w;
					pos[(p+1)%3] = u;
					pos[(p+2)%3] = v;
					e2v[e][w] = pos[0] + pos[1]*2 + pos[2]*4;
				}
			}

	for (CubeMask mask=0; mask<255; mask++) {

		int vIn[8];
		for (int v=0; v<8; v++) vIn[v] = (mask & (1<<v))!=0;

		// color vertices: init
		int vc[8] = {0,1,2,3,4,5,6,7}; // vertex coloring

		// color vertices: flood fill
		for (int unused=0; unused<10; unused++ ){
			for (int e=0; e<12; e++) {
				int v0 = e2v[e][0];
				int v1 = e2v[e][1];
				if (vIn[v0] == vIn[v1] ){
					vc[v0] = vc[v1] = std::min(vc[v0],vc[v1]);
				}
			}
			/*if (vIn[0] && vIn[3] ) vc[0] = vc[3] = std::min(vc[0],vc[3]);
            if (vIn[4] && vIn[7] ) vc[4] = vc[7] = std::min(vc[4],vc[7]);
            if (vIn[1] && vIn[2] ) vc[1] = vc[2] = std::min(vc[1],vc[2]);
            if (vIn[5] && vIn[6] ) vc[5] = vc[6] = std::min(vc[5],vc[6]);*/
		}

		// color vertices: count
		int count[2] = {0,0}; // count[0] = count_ouside, count[1] = count_inside

		int remap[8] = {0,0,0,0,0,0,0,0};
		for (int v=0; v<8; v++) remap[ vc[v] ] = 1;
		for (int v=0; v<8; v++) {
			int inOrOut = vIn[ v ];
			if (remap[v]) {
				remap[v] = count[ inOrOut ]++;
			}
		}

		int use1s = 1; //(count[1]>count[0]);
		if (count[1-use1s]==0) use1s = 1-use1s;
		//Log::debug() << count[1] << "+" << count[0];

		// final
		polygonCount[mask] = count[use1s];
		for (int i=0; i<10; i++) if (isto[i]) Log::debug << isto[i] << "conf with " << i << "polys";
		for (int e=0; e<12; e++) {
			int v0 = e2v[e][0];
			int v1 = e2v[e][1];
			if ( (vIn[v0] == use1s) && (vIn[v1] == !use1s) )
				polygonOfEdge[mask][e] = remap[v0];

			if ( (vIn[v1] == use1s) && (vIn[v0] == !use1s) )
				polygonOfEdge[mask][e] = remap[v1];
		}
	}
}

void McTable::computePolygonsFromFaces(){
	for (int i=0; i<256; i++) {

		/* trivial!
         *  uncomment to experience errors which occur
         *  if one fails to account that a few configurations
         *  include multiple disjoint polygons...*/
		//polygonCount[i]= ((i!=0)&&(i!=256) );
		//for (int e=0; e<12; e++) polygonOfEdge[i][e] = 0;
		//continue;
		/* trivial end */

		int np = 0; // number of polys
		int e2p[12]; // which poly for an edge
		for (int e=0; e<12; e++) e2p[e] = -1;

		for (int j=0; table[i][j]!=-1; j+=3) {
			int e0 = table[i][j+0];
			int e1 = table[i][j+1];
			int e2 = table[i][j+2];

			int p0 = e2p[e0];
			int p1 = e2p[e1];
			int p2 = e2p[e2];
			int p = std::max(std::max(p0,p1),p2);
			if (p==-1) {
				e2p[e0] = e2p[e1] = e2p[e2] = np++;
				continue;
			}
			if (p0!=p) {
				if (p0==-1) e2p[e0] = p;
				else for (int e=0; e<12; e++) if (e2p[e]==p0) e2p[e]=p; // union
			}
			if (p1!=p) {
				if (p1==-1) e2p[e1] = p;
				else for (int e=0; e<12; e++) if (e2p[e]==p1) e2p[e]=p; // union
			}
			if (p2!=p) {
				if (p2==-1) e2p[e2] = p;
				else for (int e=0; e<12; e++) if (e2p[e]==p2) e2p[e]=p; // union
			}
		}

		// compress
		polygonCount[i] = 0;
		for (int e=0; e<12; e++) polygonOfEdge[i][e] = -1;
		for (int e=0; e<12; e++) {
			int p = e2p[e];
			if (p < 0) continue;
			for (int f=0; f<12; f++) {
				if (e2p[f]==p) {
					polygonOfEdge[i][f] = polygonCount[i];
					e2p[f]=-1;
				}
			}
			polygonCount[i]++;
		}
	}
}

void McTable::stats(){
	for (int i=0; i<10; i++) isto[i] = 0;
	for (int i=0; i<256; i++) isto[ polygonCount[i] ]++;
	for (int i=0; i<10; i++) if (isto[i]) Log::debug << isto[i] << "conf with " << i << "polys";
}

void McTable::convertFromVCG(){

	// translate from vcg vertex/edge numeration

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
     */

	/*
     * Orig vcg enumeration:
     *         7 ________ 6           _____6__
     *         /|       /|         7/|       /|
     *       /  |     /  |        /  |     /5 |
     *   4 /_______ /    |      /__4____ /    10    Z
     *    |     |  |5    |     |    11  |     |     |
     *    |    3|__|_____|2    |     |__|__2__|     |      Y
     *    |    /   |    /      8   3/   9    /      |    /
     *    |  /     |  /        |  /     |  /1       |  /
     *    |/_______|/          |/___0___|/          |/______ X
     *   0          1
     */
	/*-1,0,1,2,3,4,5,6,7,8,9,10,11*/
	int fromVcgEdgeId[13] = {-1,0,6,1,4,2,7,3,5,8,9,11,10};
	int fromVcgVertId[ 8] =    {0,1,3,2,4,5,7,6};

	for (int k = 0; k<256; k++) {
		CubeMask m = 0x00;
		for (int i=0; i<8; i++) if (k & (1<<i) ) m |= (1<<fromVcgVertId[i]);
		for (int i=0; i<16; i++) {
			int e = vcg::tri::MCLookUpTable::CasesClassic( k, i );
			table[m][i] = fromVcgEdgeId[ e+1 ];
		}
	}

}

void McTable::addFaces(CubeMask cm, int *vertIds, std::vector<int> &faces){
	//for (int i=0; i<12; i++) Log::debug() << table[cm][i+0];

	for (int i=0; i<16; i++) {
		if (table[cm][i]==-1) break;
		int a = vertIds[ table[cm][i+0] ];
		if (a<0) Log::debug << "MERDA VERA";
		faces.push_back(a);

		/*
        int a = vertIds[ table[cm][i+0] ];
        int b = vertIds[ table[cm][i+1] ];
        int c = vertIds[ table[cm][i+2] ];
        if ((a!=-1) && (b!=-1) && (c!=-1)) {
            faces.push_back(a);
            faces.push_back(b);
            faces.push_back(c);
        }*/
	}
}



void McTable::test(std::vector< Pos3f >& res ){

	int flawed = 0;
	int not_flawed = 0;

	/*
    for(int i=0; i<(1<<12); i++) {
        CubeMask above = i>>4;
        CubeMask below = i&255;
        if ( ( ((above&1)==0)!=((above&2)==0)) &&
             ( ((above&2)==0)!=((above&8)==0)) &&
             ( ((above&8)==0)!=((above&4)==0)) &&
             ( ((above&4)==0)!=((above&1)==0)) ) {
            if (polygonCount[above]==1 && polygonCount[below]==1)
                flawed++;
            else
                not_flawed++;
        }
    }*/


	for(int i=0; i<(1<<12); i++) {
		CubeMask above = i>>4;
		CubeMask below = i&255;
		if ( ( ((above&1)==0)!=((above&2)==0)) &&
			( ((above&2)==0)!=((above&8)==0)) &&
			( ((above&8)==0)!=((above&4)==0)) &&
			( ((above&4)==0)!=((above&1)==0)) ) {
			if (
				(polygonOfEdge[above][0]==polygonOfEdge[above][6]) &&
				(polygonOfEdge[above][6]==polygonOfEdge[above][1]) &&
				(polygonOfEdge[above][1]==polygonOfEdge[above][4]) &&

				(polygonOfEdge[below][2]==polygonOfEdge[below][7]) &&
				(polygonOfEdge[below][7]==polygonOfEdge[below][3]) &&
				(polygonOfEdge[below][3]==polygonOfEdge[below][5])
				)
			//if (polygonCount[above]==1 && polygonCount[below]==1)
			{
				Pos3f cubePos(2.5*(flawed%6), 0 , 3.5*(flawed/6));
				debug_conf( res, above, cubePos+Pos3f(0,0,1.05) );
				debug_conf( res, below, cubePos+Pos3f(0,0,0) );
				flawed++;
				//return;
			}
			else {
				not_flawed++;
			}
		}
	}
	Log::debug << "Found" << flawed <<"combo which will produce non manifold edges";
	Log::debug << "(and" << not_flawed <<"which wont)";

}

void McTable::debugSearchFlawed(){

	int not_closed_4 = 0;
	int not_closed_2 = 0;
	int not_manif = 0;
	int not_flawed = 0;

	for(int i=0; i<(1<<12); i++) {
		CubeMask above = i>>4;
		CubeMask below = i&255;

		const int N = 20;
		std::pair<int, int> e[N];
		int ne = 0; // number of edges
		if ( ((above&1)==0)!=((above&2)==0)) {
			e[ne].first = polygonOfEdge[above][0];
			e[ne].second = polygonOfEdge[below][2];
			ne++;
		}

		if ( ((above&2)==0)!=((above&8)==0)) {
			e[ne].first = polygonOfEdge[above][6];
			e[ne].second = polygonOfEdge[below][7];
			ne++;
		}
		if ( ((above&8)==0)!=((above&4)==0)) {
			e[ne].first = polygonOfEdge[above][1];
			e[ne].second = polygonOfEdge[below][3];
			ne++;
		}
		if ( ((above&4)==0)!=((above&1)==0)) {
			e[ne].first = polygonOfEdge[above][4];
			e[ne].second = polygonOfEdge[below][5];
			ne++;
		}
		if (ne==4) {
			if (e[0]==e[1] && e[1]==e[2] && e[2]==e[3]) {
				not_manif++;
			} else {
				if (!(e[0]==e[1] && e[2]==e[3])
					&&
					!(e[0]==e[2] && e[1]==e[3])
					&&
					!(e[0]==e[3] && e[1]==e[2]) )
					not_closed_4 ++;
				else not_flawed++;
			}
		} else if (ne==2){
			if (e[0]!=e[1]) not_closed_2 ++;
			else not_flawed++;
		} else if (ne==0){
			not_flawed++;
		} else {
			assert(0);
		}
	}
	Log::debug << "Found " << not_manif <<"combo which will produce non manifold edges";
	Log::debug << "and " << (not_closed_2+not_closed_4) << "(" << not_closed_2<<"+"<<not_closed_4 << ") combo which will produce non closed edges";
	Log::debug << "(and" << not_flawed <<"which wont)";

}

void McTable::debug_conf(std::vector<Pos3f> &res, CubeMask m, Pos3f t){
	Pos3f da(-1,+0,-0);
	Pos3f db(+1,+1,-1);
	Pos3f dc(+1,-1,+1);

	for (int dz = 0,i=0; dz<2; dz++)
		for (int dy = 0; dy<2; dy++)
			for (int dx = 0; dx<2; dx++,i++){
				float inside = (m & (1<<i))?0.05:0.005;

				// draw octa
				res.push_back( Pos3f(dx,dy,dz) + t + da*inside);
				res.push_back( Pos3f(dx,dy,dz) + t + db*inside);
				res.push_back( Pos3f(dx,dy,dz) + t + dc*inside);

			}
	static Pos3f typicalEdgePos[12];
	static bool once = true;
	if (once)
		for (int p=0,i=0; p<3; p++)
			for (int u=0; u<2; u++)
				for (int v=0; v<2; v++,i++){
					typicalEdgePos[i][(p+0)%3] = 0.5;
					typicalEdgePos[i][(p+2)%3] = u;
					typicalEdgePos[i][(p+1)%3] = v;
				}
	once=false;
	for (int i=0; table[m][i]!=-1; i++)
		res.push_back( t + typicalEdgePos[ table[m][i] ]);

}
