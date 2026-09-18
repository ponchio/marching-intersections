#include "filter.h"
#include "intersections.h"

#include <algorithm>
#include <cmath>

namespace mi {

// Minimal implicit sphere used with Volume::fromImplicit
struct ImplicitSphereLocal {
    float radius;
    ImplicitSphereLocal(float r): radius(r) {}

    Box3f box() const {
        Pos3f diag(radius, radius, radius);
        Box3f b;
        b.min = -diag;
        b.max = diag;
        return b;
    }

    std::vector<float> getIntersections(int /*plane*/, float u, float v) const {
        std::vector<float> result;
        float d = radius*radius - u*u - v*v;
        if(d > 0) {
            float s = sqrt(d);
            result.push_back(-s);
            result.push_back(s);
        }
        return result;
    }
};

void CarveSphereFilter::apply(Volume &vol, const Box3i &box) {
    // Compute world-space center and radius (radius is 2 voxels smaller)
    Pos3i centerVoxel = box.Center();
    Vec3f centerWorld(centerVoxel[0]*vol.step, centerVoxel[1]*vol.step, centerVoxel[2]*vol.step);
    float minDim = std::min(std::min(box.DimX(), box.DimY()), box.DimZ());
    float radius = (minDim * vol.step) * 0.5f - 2.0f * vol.step;
    if(radius <= 0.0f) return;

    Volume sphereVol;
    sphereVol.fromImplicit(ImplicitSphereLocal(radius), vol.step);
    sphereVol.translate(centerWorld);

    Volume result;
    result.subtraction(vol, sphereVol);
    vol = result;
}

void applyCarveSphere(Volume &vol, const Box3i &box) {
    CarveSphereFilter f;
    f.apply(vol, box);
}

} // namespace mi
