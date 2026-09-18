#ifndef INTERSECTIONS_H
#define INTERSECTIONS_H

#include "log.h"
#include "vec.h"
#include "box.h"
#include "plane.h"

#include <vector>

/* class mi::Volume :  a Marching Intersection Volume.
 *
 * An efficient volumetric representation of an closed 3D object.
 *  + can be produced from a closed mesh
 *  + can produce a closed mesh
 *  + allows volumetric operations
 *  + (can also be produced from implcit surfaces etc)
 *
 * Similar to Marching Cubes of a distance field:
 *  + connectivity of produced mesh: equivalent.
 * but:
 *  + more accurate (a bit):
 *     generated vertices always lie on the input surface.
 *  + more efficient (a lot):
 *     O(N) space, O(N) time (for all operations, unless specified otherwise)
 *     N = number of vertices of the output mesh.
 *     Thus: O(Resolution^2), instead of the typical O(Resolution^3) of volumes
 *
 */

namespace mi {

class Volume {
public:
	float step;
	Box3i box;

	Volume() {}

	Volume(Box3i box);

	/* creation */
	void fromMesh(const std::vector<Pos3f> &verts, const std::vector<int> &faces, float step = 1);
	void fromSweep(const std::vector<Pos3f> &verts, const std::vector<int> &faces, const Vec3f &start, const Vec3f &end, float step = 1);
	template<class Implicit> void fromImplicit(Implicit function, float _step = 1);
	void fullSolid(Box3i box); // a full bounding box [unimplemented]
	void vacuum(); // empties all [unimplemented]

	/* volumetric operations */
	void unification(const Volume &a, const Volume &b); //damn you, reserved 'union' keyword!
	void intersection(const Volume &a, const Volume &b);
	void subtraction(const Volume &a, const Volume &b);
	// Extract a sub-volume containing only the intersection lines that cross the
	// provided integer bounding box (in voxel/grid coordinates). The returned
	// Volume has the same `step` as the source and its `box` equals `_box`.
	// Only intersections whose global coordinates lie inside `_box` are copied.
	Volume subVolume(Box3i _box) const;
	// Merge a previously extracted sub-volume back into this volume. The
	// `sub` must have the same `step` and a `box` fully contained in `this->box`.
	// Lines in the sub region are replaced and consistency is enforced.
	void mergeSubVolume(const Volume &sub);
	void sweep(const Volume &v, Vec3f start, Vec3f end, int subsample); // produces a sweep of the volume v (which can be subsampled)   [O(N*K): todo: reduce to O(N)]
	void fastSweep(const Volume &v, Vec3f start, Vec3f end, int subsample); // produces a sweep of the volume v (which can be subsampled)   [O(N*K): todo: reduce to O(N)]
	void intersectPlane(const Vec3f &n, const Vec3f &pos); // intersects with specified half-space

	/* spatial transformations */
	void translate(Vec3i d);
	void translate(Vec3f d);
	void rotateX( int mult_of_90_deg ); // [unimplemented]
	void rotateY( int mult_of_90_deg ); // [unimplemented]
	void rotateZ( int mult_of_90_deg ); // [unimplemented]
	void rotateXYZ( bool clockwise );   // constant time [unimplemented]

	/* export */
	void toPointCloud(std::vector<Pos3f> &verts) const;
	void toMesh(std::vector<Pos3f> &verts, std::vector<int> &tri_faces);
	void toMeshDual(std::vector<Pos3f> &verts, std::vector<int> &quads);
	// old ways
	void toMeshOld(std::vector<Pos3f> &verts, std::vector<int> &tri_faces);
	void toMeshDualOld(std::vector<Pos3f> &verts, std::vector<int> &tri_faces);

	/* maintenance and stats */
	int cleanUp(); // removes redundant (double) intersections. Call for efficiecy, but automatically called before extracting mesh anyway
	void subsampled(const Volume &a, int step, Vec3i offset);
	int totalIntersections() const;

	/* sanity checks */
	bool checkIntersectionParity() const;
	bool checkConsistency() const;

	/* direct plane access for external editing (getters/setters) */
	std::vector<int> getPlaneIndices(int plane) const;
	void setPlaneIndices(int plane, const std::vector<int> &indices);
	std::vector<Intersection> getPlaneIntersections(int plane) const;
	void setPlaneIntersections(int plane, const std::vector<Intersection> &intersections);

private:    

	bool isClean; // if false, might have double interceptions (to be removed calling cleanUp)

	Plane planes[3];

	void resize(Box3i _box);
	bool enforceConsistency(); // must be called after any op which relies on numeric precision on the intercepts

	int  getCentroid(int id, std::vector<int> &visited, const Pos3i &pos, std::vector<Pos3f> &vert);
	void triangulateOld(std::vector<char> &visited, const Pos3i &cube, std::vector<int> &faces);
	bool checkConsistency(Vec3i p) const;
	bool enforceConsistency(Vec3i p);

	void adjustNormalsForSweep( Vec3f i );

	static void optimizeQuadDiagonals(std::vector<int> &q, int nv);

	static Pos3f toLocal(Pos3f a, int plane) { return swizzle(a, plane+1); }
	static Pos3f toGlobal(Pos3f a, int plane) { return swizzle(a, 2-plane); }
	static Pos3i toLocal(Pos3i a, int plane) { return swizzle(a, plane+1); }
	static Pos3i toGlobal(Pos3i a, int plane) { return swizzle(a, 2-plane); }

	static Box2i toLocal(Box3i box, int n) {
		Pos3i min = toLocal(box.min, n);
		Pos3i max = toLocal(box.max, n);
		Box2i res;
		res.min = Pos2i( min[0], min[1] );
		res.max = Pos2i( max[0], max[1] );
		return res;
	}

	static Pos3f swizzle(Pos3f a, int n) { //rotate vertex by n
		Pos3f b;
		b[0] = a[(0 + n)%3];
		b[1] = a[(1 + n)%3];
		b[2] = a[(2 + n)%3];
		return b;
	}

	static Pos3i swizzle(Pos3i a, int n) { //rotate vertex by n
		Pos3i b;
		b[0] = a[(0 + n)%3];
		b[1] = a[(1 + n)%3];
		b[2] = a[(2 + n)%3];
		return b;
	}

	friend class Loom;
	friend class DualLoom;
};


/* implementation of templated functions */
/* ------------------------------------- */

template<class Implicit>
void Volume::fromImplicit(Implicit function, float _step ) {
	step = _step;
	Box3f boxf = function.box();
	for(int k = 0; k < 3; k++) {
		box.min[k] = floor(boxf.min[k]/step);
		box.max[k] = ceil(boxf.max[k]/step) + 1; //max is excluded to avoids <= all the time.
	}
	resize(box);
	for(int i = 0; i < 3; i++) {
		Plane &p = planes[i];
		int index = 0;
		for(int v = p.box.min[1]; v < p.box.max[1]; v++) {
			for(int u = p.box.min[0]; u < p.box.max[0]; u++) {
#if STORE_NORMALS
				std::vector<Vec3f> normals;
				std::vector<float> intersections = function.getIntersections(i, u*step, v*step, normals );
#else
				std::vector<float> intersections = function.getIntersections(i, u*step, v*step);
#endif
				p.intersections.resize(p.intersections.size() + intersections.size());

				OutputLine output = p.output(index++);
				for(unsigned int k =0 ; k < intersections.size(); k++)
#if STORE_NORMALS
					output.push_back(Intersection(intersections[k]/step,normals[k]));
#else
					output.push_back(Intersection(intersections[k]/step));
#endif
			}
		}
	}
	enforceConsistency();
}



} //namespace mi
#endif // INTERSECTIONS_H
