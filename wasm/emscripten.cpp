#include <emscripten/bind.h>
#include <emscripten/val.h>
#include "intersections.h"

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

EMSCRIPTEN_BINDINGS(marching_lib) {
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
        .function("toMesh", &exportToMesh);
}