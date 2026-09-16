Marching Intersections
======================

Small library and demo for marching-intersections mesh operations.

Quick overview
--------------
- Native C++ library and test binaries live under `lib/` and are built with CMake.
- A WebAssembly build and a small browser demo live under `wasm/` and `demo/` respectively.

Prerequisites
-------------
- CMake (>= 3.x) and a C++ compiler with C++17 support
- GNU make (or your chosen generator)
- Emscripten SDK (for the WASM build)
- Node.js or any static HTTP server to serve the demo

Build (native)
--------------
1. Create a build directory and run CMake + make:

   mkdir -p build && cd build
   cmake .. -DCMAKE_BUILD_TYPE=Release
   make -j$(nproc)

2. Example binaries (tests) will be available in the build directory (e.g. `mi_test`).

Build (WASM)
------------
1. Ensure `emcmake`/`emmake` from Emscripten are available in your PATH.
2. Run the helper script at the repository root:

   ./build-wasm.sh

   The script invokes CMake (via Emscripten) and copies `marching_lib_wasm.js` and
   `marching_lib_wasm.wasm` into `dist/` so the demo can import them.

Run the demo (Vite)
-------------------

1. Build the WASM artifacts (if you haven't already):

   ./build-wasm.sh

   This places `marching_lib_wasm.js` and `marching_lib_wasm.wasm` into `dist/`.

2. Initialize and install demo dependencies (from the `demo/` folder):

   cd demo
   npm init -y
   npm install --save-dev vite vite-plugin-wasm vite-plugin-top-level-await
   npm install three

3. Start the Vite dev server:

   npx vite

   or run 
   
   npm run dev

4. Open the URL printed by Vite (usually `http://localhost:5173/demo`) and load the demo.


