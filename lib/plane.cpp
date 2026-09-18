#include "plane.h"
#include "vec.h"

#include <algorithm>
#include <cmath>

namespace mi {

static bool aisInside(float x, float y, const Pos2f &a, const Pos2f &b, float &res) {
    Vec2f C((float)x, (float)y);

    if(a < b) {
        res = -((C - b)^(a - b));
        return res <= 0;
    } else {
        res = ((C - a)^(b - a));
        return  res < 0;
    }
}

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

void Line::insert(Intersection a) {
    int i = 0;
    while(a.p > at(i).p) i++;

    while(at(i).p == at(i).p) {
        std::swap(at(i++), a);
    }
    at(i) = a;
}

void Plane::fillTriangleOld(const Pos3f &a, const Pos3f &b, const Pos3f &c, std::vector<std::pair<int, Intersection > > & result) {

    Vec2f e0(c[1] - a[1], a[0] - c[0]); //Magic.
    Vec2f e1(a[1] - b[1], b[0] - a[0]);

    float det = e0[0]*e1[1] - e0[1]*e1[0];
    //if(fabs(det) < EPSILON) return;

    //compute bounding box
    const int minx = (int)ceil (std::min(a[0], std::min(b[0], c[0])));
    const int miny = (int)ceil (std::min(a[1], std::min(b[1], c[1])));
    const int maxx = (int)floor(std::max(a[0], std::max(b[0], c[0])));
    const int maxy = (int)floor(std::max(a[1], std::max(b[1], c[1])));

    const float za = a[2]/det;
    const float zb = b[2]/det;
    const float zc = c[2]/det;

    const Vec2f a2(a[0], a[1]);
    const Vec2f b2(b[0], b[1]);
    const Vec2f c2(c[0], c[1]);

#if STORE_NORMALS
    Vec3f n = ((a-b)^(c-b)).Normalize();
#endif

    for(int y = miny; y <= maxy; y++) {
        for(int x = minx; x <= maxx; x++) {

            float ax = x;
            float ay = y;

            float DA, DB, DC;
            bool A = aisInside(ax, ay, b2, c2, DA);
            bool B = aisInside(ax, ay, c2, a2, DB);
            bool C = aisInside(ax, ay, a2, b2, DC);

            if((A != B || B != C)) continue;

            float z = -(za*DA + zb*DB + zc*DC);
#if STORE_NORMALS
            result.push_back(std::make_pair( index(x, y), Intersection(z,n) ));
#else
            result.push_back(std::make_pair( index(x, y), Intersection(z) ));
#endif
        }
    }
}


void Plane::fillTriangle(const Pos3f &af, const Pos3f &bf, const Pos3f &cf, std::vector<std::pair<int, Intersection > > & result) {

    /*
    // compute zX, zY, zC, s.t.   z = zC + y*zY + x*zX
    Vec2f eX( af[0]-cf[0] , bf[0]-af[0] );
    Vec2f eY( cf[1]-af[1] , af[1]-bf[1] );
    Vec2f eZ( bf[2]-af[2] , cf[2]-af[2] );
    float det = eY ^ eX ;
    float zX = eY * eZ / det;
    float zY = eX * eZ / det;
    float zS = af[2] - zX*af[0] - zY*af[1];
    */

#if STORE_NORMALS
    Vec3f n = ((af-bf)^(cf-bf)).Normalize();
#endif

    const int SHIFT = 6;
    const float MULT = (1<<SHIFT);
    const int FF = (1<<SHIFT)-1;

    // fixed precision positions of vertices
    const Pos2i a( round(af[0]*MULT), round(af[1]*MULT) );
    const Pos2i b( round(bf[0]*MULT), round(bf[1]*MULT) );
    const Pos2i c( round(cf[0]*MULT), round(cf[1]*MULT) );

    // compute bounding box
    const int minx = (((std::min(a[0], std::min(b[0], c[0])))+FF) >> SHIFT);
    const int miny = (((std::min(a[1], std::min(b[1], c[1])))+FF) >> SHIFT);
    const int maxx = (((std::max(a[0], std::max(b[0], c[0])))   ) >> SHIFT);
    const int maxy = (((std::max(a[1], std::max(b[1], c[1])))   ) >> SHIFT);

    Vec2i min (minx<<SHIFT, miny<<SHIFT);
    Vec2i ea = b - a;
    Vec2i eb = c - b;
    Vec2i ec = a - c;

    Vec3i edgeFunSt( (min-a)^ea, (min-b)^eb, (min-c)^ec );
    Vec3i edgeFunDx( +ea[1]<<SHIFT , +eb[1]<<SHIFT , +ec[1]<<SHIFT );
    Vec3i edgeFunDy( -ea[0]<<SHIFT , -eb[0]<<SHIFT , -ec[0]<<SHIFT );

    float edgeFunSum = edgeFunSt[0] + edgeFunSt[1] + edgeFunSt[2];
    Vec3f zetas = Vec3f(cf[2],af[2],bf[2]) / edgeFunSum;

    // if ties possible, small bonuses to break them: corresponds
    // to give infinetesimal +dx and +dy to pixel coords (with dx >> dy)
    if ( ( (edgeFunSt[0] & FF) == 0) && (b>a) ) edgeFunSt[0]++;
    if ( ( (edgeFunSt[1] & FF) == 0) && (c>b) ) edgeFunSt[1]++;
    if ( ( (edgeFunSt[2] & FF) == 0) && (a>c) ) edgeFunSt[2]++;

    for(int y = miny; y <= maxy; y++, edgeFunSt+=edgeFunDy ) {

        Vec3i edgeFun = edgeFunSt;
        for(int x = minx; x <= maxx; x++ , edgeFun += edgeFunDx ) {

            if(((edgeFun[0]>0) != (edgeFun[1]>0))
                    || ((edgeFun[1]>0) != (edgeFun[2]>0))) continue;

            //float z = zS + zX*x + zY*y;
            float z = Vec3f::Construct( edgeFun ) * zetas;

#if STORE_NORMALS
            result.push_back(std::make_pair( index(x, y), Intersection(z,n) ));
#else
            result.push_back(std::make_pair( index(x, y), Intersection(z) ));
#endif

        }
    }
}

void Plane::accumulateLengths() {
    int count = 0;
    for(unsigned int i = 0; i < indices.size(); i++) {
        int len = indices[i];
        indices[i] = count;
        count += len;
    }
}

void Plane::allocate() {
    const unsigned int NaNi = 0xffc00000;;
    const float NaN = *(float *)&NaNi;
    intersections.resize(0);
#if STORE_NORMALS
    intersections.resize(indices.back(), Intersection(NaN,Vec3f(0,0,0)) );
#else
    intersections.resize(indices.back(), NaN);
#endif
}

bool Plane::isInside(Vec3i p) const {
    const Line line = at(p[0], p[1]);
    int i = 0;
    for(; i < line.size(); i++) {
        if(p[2] <= line[i].p)
            break;
    }
    return (i%2) == 1;
}

Intersection *Plane::getClosest(Vec3i p) {
    Line line = at(p[0], p[1]);
    Intersection *result = NULL;
    int i = 0;
    for(; i < line.size(); i++)
        if(line[i].p >= p[2]-1 && line[i].p < p[2]+1)
            if(result == NULL || fabs(result->p - p[2]) > fabs(line[i].p - p[2]))
                result = &line[i];

    return result;
}

void Plane::translate(Vec3i d) {
    for(unsigned int i = 0; i < intersections.size(); i++)
        intersections[i].p += d[2];
    box.min[0] += d[0];
    box.min[1] += d[1];
    box.max[0] += d[0];
    box.max[1] += d[1];
}

} // namespace mi
