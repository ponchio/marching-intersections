#!/bin/bash
mkdir -p build-wasm
cd build-wasm

# Configure with Emscripten toolchain
emcmake cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_STANDARD=17

# Build target
emmake make -j4

cd ..
mkdir -p dist
if [ -f build-wasm/marching_lib_wasm.js ]; then
    cp build-wasm/marching_lib_wasm.js dist/
    cp build-wasm/marching_lib_wasm.wasm dist/
elif [ -f build-wasm/wasm/marching_lib_wasm.js ]; then
    cp build-wasm/wasm/marching_lib_wasm.js dist/
    cp build-wasm/wasm/marching_lib_wasm.wasm dist/
else
    echo "WASM output not found in expected locations"
fi
echo "Wasm build complete in ./dist"