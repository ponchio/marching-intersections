import { defineConfig } from 'vite';
import wasm from 'vite-plugin-wasm';
import topLevelAwait from 'vite-plugin-top-level-await';
import { resolve } from 'path';

export default defineConfig({
  // 1. Tell Vite where the web source files (index.html) live
  root: 'demo',

  plugins: [
    wasm(),
    topLevelAwait()
  ],

  // 2. Allow Vite to serve static assets from dist/ if needed during dev
  server: {
    fs: {
      allow: ['..']
    }
  },

  // 3. Configure where production builds are output
  build: {
    outDir: '../dist-demo',
    emptyOutDir: true,
    rollupOptions: {
      input: {
        main: resolve(__dirname, 'demo/index.html')
      }
    }
  }
});