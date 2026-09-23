#include <emscripten/bind.h>
#include <emscripten/val.h>
#include "intersections.h"
#include "filter.h"

using namespace emscripten;

// Helper to convert raw JS Float32Array and Uint32Array directly into Volume::fromMesh
void volumeFromJSBuffers(mi::Volume& vol, val jsPositions, val jsIndices, float step) {
    // Convert flat float array [x0, y0, z0, x1, y1, z1, ...] into std::vector<Pos3f>
    std::vector<float> rawPositions = vecFromJSArray<float>(jsPositions);
    std::vector<Pos3f> verts;
    verts.reserve(rawPositions.size() / 3);

    for (size_t i = 0; i < rawPositions.size(); i += 3) {
        verts.push_back(Pos3f(rawPositions[i], rawPositions[i + 1], rawPositions[i + 2]));
    }
    std::vector<int> faces = vecFromJSArray<int>(jsIndices);

    vol.fromMesh(verts, faces, step);
}

struct MeshData {
    val vertices; // Float32Array
    val indices;  // Uint32Array
    MeshData(): vertices(val::undefined()), indices(val::undefined()) {}
    MeshData(val v, val i): vertices(v), indices(i) {}
};

class JSImplicitWrapper {
public:
    val jsFunc;
    Box3f bounds;

    JSImplicitWrapper(val jsFunc, Box3f bounds) : jsFunc(jsFunc), bounds(bounds) {}

    Box3f box() const { return bounds; }

    std::vector<float> getIntersections(int plane, float u, float v) const {
        // Calls the JS function: jsFunc(plane, u, v) -> returns JS Array of floats
        val jsArray = jsFunc(plane, u, v);
        return vecFromJSArray<float>(jsArray);
    }
};

class ImplicitSphere {
public:
    float radius;
    ImplicitSphere(float r) : radius(r) {}

    Box3f box() const {
        Pos3f diag(radius, radius, radius);
        Box3f b;
        b.min = -diag;
        b.max = diag;
        return b;
    }

    std::vector<float> getIntersections(int plane, float u, float v) const {
        std::vector<float> result;
        float d = radius * radius - u * u - v * v;
        if (d > 0) {
            d = sqrt(d);
            result.push_back(-d);
            result.push_back(d);
        }
        return result;
    }
};

MeshData exportToMesh(mi::Volume& vol) {
    std::vector<Pos3f> verts;
    std::vector<int> faces;
    
    vol.toMesh(verts, faces);

    // Flatten vertices into a single continuous float buffer for WebGL
    std::vector<float> flatVerts;
    flatVerts.reserve(verts.size() * 3);
    for (const auto& v : verts) {
        flatVerts.push_back(v[0]);
        flatVerts.push_back(v[1]);
        flatVerts.push_back(v[2]);
    }

    // Create JS-owned TypedArrays (copies) so they don't reference
    // the temporary C++ vectors that will be destroyed on return.
    val jsVerts = val::global("Float32Array").new_(typed_memory_view(flatVerts.size(), flatVerts.data()));
    val jsIndices = val::global("Uint32Array").new_(typed_memory_view(faces.size(), faces.data()));

    return MeshData{ jsVerts, jsIndices };
}

// JS wrapper: carve a sphere centered at world coordinates (wx,wy,wz) with given world radius
void carveSphereAt(mi::Volume &vol, float wx, float wy, float wz, float radiusWorld) {
    // Convert world center to voxel coordinates (center voxel)
    Pos3i centerVoxel((int)std::floor(wx / vol.step + 0.5f), (int)std::floor(wy / vol.step + 0.5f), (int)std::floor(wz / vol.step + 0.5f));

    // Compute required integer box size so that CarveSphereFilter computes approximately the requested radius
    int minDimVox = std::max(1, (int)std::ceil((2.0f * (radiusWorld + 2.0f * vol.step)) / vol.step));

    Pos3i half(minDimVox / 2, minDimVox / 2, minDimVox / 2);
    Pos3i bmin = centerVoxel - half;
    Pos3i bmax = bmin + Pos3i(minDimVox, minDimVox, minDimVox);

    // Clamp to volume box
    if (bmin[0] < vol.box.min[0]) bmin[0] = vol.box.min[0];
    if (bmin[1] < vol.box.min[1]) bmin[1] = vol.box.min[1];
    if (bmin[2] < vol.box.min[2]) bmin[2] = vol.box.min[2];
    if (bmax[0] > vol.box.max[0]) bmax[0] = vol.box.max[0];
    if (bmax[1] > vol.box.max[1]) bmax[1] = vol.box.max[1];
    if (bmax[2] > vol.box.max[2]) bmax[2] = vol.box.max[2];

    Box3i box(bmin, bmax);
    mi::applyCarveSphere(vol, box);
}

EMSCRIPTEN_BINDINGS(marching_lib) {
    register_vector<int>("VectorInt");
    register_vector<mi::Intersection>("VectorIntersection");

    value_object<mi::Intersection>("Intersection")
    .field("p", &mi::Intersection::p)
#if STORE_NORMALS
    .field("n", &mi::Intersection::n)
#endif
    ;
    value_object<MeshData>("MeshData")
        .field("vertices", &MeshData::vertices)
        .field("indices", &MeshData::indices);

    class_<Box3i>("Box3i")
        .constructor<Pos3i, Pos3i>();

    class_<Box3f>("Box3f")
        .constructor<>()
        .property("min", &Box3f::min)
        .property("max", &Box3f::max);

    class_<ImplicitSphere>("ImplicitSphere")
        .constructor<float>();

    function("carveSphereAt", &carveSphereAt);

    
    class_<mi::Volume>("Volume")
        .constructor<>()
        .constructor<Box3i>()
        .property("step", &mi::Volume::step)
        .function("fromMesh", &volumeFromJSBuffers)
        .function("fromImplicitSphere", select_overload<void(ImplicitSphere, float)>(&mi::Volume::fromImplicit<ImplicitSphere>))
        .function("fromJSImplicit", +[](mi::Volume& vol, val jsFunc, Box3f bounds, float step) {
            JSImplicitWrapper wrapper(jsFunc, bounds);
            vol.fromImplicit(wrapper, step);
        })
        .function("unification", &mi::Volume::unification)
        .function("intersection", &mi::Volume::intersection)
        .function("subtraction", &mi::Volume::subtraction)
        .function("translate", select_overload<void(Vec3f)>(&mi::Volume::translate))
        .function("subVolume", &mi::Volume::subVolume)
        .function("mergeSubVolume", &mi::Volume::mergeSubVolume)
        .function("getPlaneIndices", &mi::Volume::getPlaneIndices)
        .function("setPlaneIndices", &mi::Volume::setPlaneIndices)
        .function("getPlaneIntersections", &mi::Volume::getPlaneIntersections)
        .function("setPlaneIntersections", &mi::Volume::setPlaneIntersections)
        .function("toMesh", &exportToMesh);
}