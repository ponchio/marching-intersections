import * as THREE from 'three';
import { OrbitControls } from 'three/examples/jsm/controls/OrbitControls.js';
import { OBJLoader } from 'three/examples/jsm/loaders/OBJLoader.js';
import createModule from '../dist/marching_lib_wasm.js';

let Module, scene, camera, renderer, controls;
let activeMesh = null;
let volumeInstance = null;

async function init() {
  // 1. Initialize Emscripten Wasm Engine
  Module = await createModule();
  volumeInstance = new Module.Volume();

  // 2. Setup Three.js Scene
  const container = document.getElementById('canvas-container');
  scene = new THREE.Scene();
  scene.background = new THREE.Color(0x1e1e1e);

  camera = new THREE.PerspectiveCamera(45, window.innerWidth / window.innerHeight, 0.1, 1000);
  camera.position.set(0, 2, 5);

  renderer = new THREE.WebGLRenderer({ antialias: true });
  renderer.setSize(window.innerWidth, window.innerHeight);
  renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
  container.appendChild(renderer.domElement);

  controls = new OrbitControls(camera, renderer.domElement);
  controls.enableDamping = true;

  // 3. Lighting Setup
  const ambientLight = new THREE.AmbientLight(0xffffff, 0.6);
  scene.add(ambientLight);

  const dirLight = new THREE.DirectionalLight(0xffffff, 0.8);
  dirLight.position.set(5, 10, 7);
  scene.add(dirLight);

  // 4. Attach Toolbar Event Handlers
  setupUI();

  // Resize handler
  window.addEventListener('resize', onWindowResize);

  // Animation Loop
  animate();
}

function setupUI() {
  // Open Mesh Button
  const fileInput = document.getElementById('file-input');
  fileInput.addEventListener('change', (e) => {
    const file = e.target.files[0];
    if (file) {
      const reader = new FileReader();
      reader.onload = (event) => loadObjContent(event.target.result);
      reader.readAsText(file);
    }
  });

  // Ensure a step-factor control exists (fraction of average edge length)
  let stepInputEl = document.getElementById('step-factor');
  if (!stepInputEl && fileInput && fileInput.parentNode) {
    const wrapper = document.createElement('div');
    wrapper.style.display = 'inline-block';
    wrapper.style.marginLeft = '12px';
    const label = document.createElement('label');
    label.style.color = '#ddd';
    label.style.fontSize = '0.9em';
    label.textContent = 'Step factor: ';
    const input = document.createElement('input');
    input.type = 'number';
    input.id = 'step-factor';
    input.value = '0.5';
    input.step = '0.05';
    input.min = '0.01';
    input.max = '2';
    input.style.width = '60px';
    label.appendChild(input);
    wrapper.appendChild(label);
    fileInput.parentNode.insertBefore(wrapper, fileInput.nextSibling);
    stepInputEl = input;
  }

  // Sculpting Filter Stubs
  document.getElementById('btn-filter-grow').addEventListener('click', () => applyFilter('grow'));
  document.getElementById('btn-filter-dig').addEventListener('click', () => applyFilter('dig'));
  document.getElementById('btn-filter-flatten').addEventListener('click', () => applyFilter('flatten'));
}

// Compute average triangle edge length from flat position array and index array
function computeAverageEdgeLength(positions, indices) {
  const triCount = Math.floor(indices.length / 3);
  if (triCount <= 0) return 0.05; // fallback
  let sum = 0.0;
  let edgeCount = 0;
  for (let i = 0; i < triCount; ++i) {
    const ia = indices[3 * i], ib = indices[3 * i + 1], ic = indices[3 * i + 2];
    const ax = positions[3 * ia], ay = positions[3 * ia + 1], az = positions[3 * ia + 2];
    const bx = positions[3 * ib], by = positions[3 * ib + 1], bz = positions[3 * ib + 2];
    const cx = positions[3 * ic], cy = positions[3 * ic + 1], cz = positions[3 * ic + 2];
    const ab = Math.hypot(ax - bx, ay - by, az - bz);
    const bc = Math.hypot(bx - cx, by - cy, bz - cz);
    const ca = Math.hypot(cx - ax, cy - ay, cz - az);
    sum += ab + bc + ca;
    edgeCount += 3;
  }
  return sum / edgeCount;
}

function getStepFactor() {
  const el = document.getElementById('step-factor');
  if (!el) return 0.5;
  const v = parseFloat(el.value);
  return Number.isFinite(v) && v > 0 ? v : 0.5;
}

// Validate mesh: check for NaN, out-of-bounds indices, and degenerate triangles.
function validateMesh(positions, indices, avgEdge) {
  const errors = [];
  const triCount = Math.floor(indices.length / 3);
  const vertexCount = Math.floor(positions.length / 3);

  if (positions.length % 3 !== 0) {
    errors.push('Positions array length is not a multiple of 3: ' + positions.length);
    return { ok: false, errors };
  }

  // Check positions for non-finite values
  for (let i = 0; i < positions.length; ++i) {
    if (!Number.isFinite(positions[i])) {
      errors.push('Non-finite position component at index ' + i + ': ' + positions[i]);
      return { ok: false, errors };
    }
  }

  // Check indices for bounds and integerness
  for (let i = 0; i < indices.length; ++i) {
    const idx = indices[i];
    if (!Number.isFinite(idx) || Math.floor(idx) !== idx) {
      errors.push('Invalid index at array position ' + i + ': ' + idx);
      return { ok: false, errors };
    }
    if (idx < 0 || idx >= vertexCount) {
      errors.push('Index out of bounds at array position ' + i + ': ' + idx + ' (vertexCount=' + vertexCount + ')');
      return { ok: false, errors };
    }
  }

  // Filter degenerate triangles (zero-area or repeated vertices)
  const cleaned = [];
  let degenerateCount = 0;
  const areaNormThreshold = 1e-6; // area / (avgEdge^2) below this is considered degenerate

  for (let t = 0; t < triCount; ++t) {
    const ia = indices[3 * t], ib = indices[3 * t + 1], ic = indices[3 * t + 2];
    if (ia === ib || ib === ic || ic === ia) { // repeated indices
      degenerateCount++;
      continue;
    }

    const ax = positions[3 * ia], ay = positions[3 * ia + 1], az = positions[3 * ia + 2];
    const bx = positions[3 * ib], by = positions[3 * ib + 1], bz = positions[3 * ib + 2];
    const cx = positions[3 * ic], cy = positions[3 * ic + 1], cz = positions[3 * ic + 2];

    const abx = bx - ax, aby = by - ay, abz = bz - az;
    const acx = cx - ax, acy = cy - ay, acz = cz - az;

    const crossx = aby * acz - abz * acy;
    const crossy = abz * acx - abx * acz;
    const crossz = abx * acy - aby * acx;
    const crossLen = Math.hypot(crossx, crossy, crossz);
    const area = 0.5 * crossLen;

    let isDeg = false;
    if (avgEdge > 0) {
      const norm = area / (avgEdge * avgEdge);
      if (norm < areaNormThreshold) isDeg = true;
    } else {
      if (area < 1e-12) isDeg = true;
    }

    if (isDeg) {
      degenerateCount++;
      continue;
    }

    cleaned.push(ia, ib, ic);
  }

  return { ok: errors.length === 0, errors, degenerateCount, triCount, cleanedIndices: cleaned };
}

// Parse OBJ and ingest into Marching Intersection volume
function loadObjContent(objText, stepFactorOverride) {
  const loader = new OBJLoader();
  const obj = loader.parse(objText);

  obj.traverse((child) => {
    if (child.isMesh) {
      const geom = child.geometry;

      // Extract position & index TypedArrays
      const positions = Array.from(geom.attributes.position.array);
      let indices = geom.index
        ? Array.from(geom.index.array)
        : Array.from({ length: positions.length / 3 }, (_, i) => i);

      // Initial average edge for validation threshold
      const avgEdgeInitial = computeAverageEdgeLength(positions, indices);
      const validation = validateMesh(positions, indices, avgEdgeInitial);
      if (!validation.ok) {
        console.error('Mesh validation failed:', validation.errors);
        alert('Mesh validation failed: ' + validation.errors[0]);
        return;
      }

      if (validation.degenerateCount > 0) {
        if (validation.degenerateCount >= validation.triCount) {
          alert('All triangles appear degenerate; cannot import mesh.');
          return;
        }
        console.warn('Skipped', validation.degenerateCount, 'degenerate triangles (out of', validation.triCount + ')');
        indices = validation.cleanedIndices;
      }

      // Recompute a sensible step from the (possibly cleaned) mesh
      const avgEdge = computeAverageEdgeLength(positions, indices);
      const factor = (typeof stepFactorOverride === 'number') ? stepFactorOverride : getStepFactor();
      const step = Math.max(avgEdge * factor, 1e-6);

      // Call bound C++ Volume function with computed step
      volumeInstance.fromMesh(positions, indices, step);
      updateMeshFromWasm();
    }
  });
}

// Re-generate Three.js geometry from C++ Marching Intersection Volume
function updateMeshFromWasm() {
  const meshData = volumeInstance.toMesh();

  if (activeMesh) {
    scene.remove(activeMesh);
    activeMesh.geometry.dispose();
  }

  const geometry = new THREE.BufferGeometry();
  geometry.setAttribute('position', new THREE.BufferAttribute(new Float32Array(meshData.vertices), 3));
  geometry.setIndex(new THREE.BufferAttribute(new Uint32Array(meshData.indices), 1));
  geometry.computeVertexNormals();

  const material = new THREE.MeshStandardMaterial({
    color: 0x0088ff,
    roughness: 0.3,
    metalness: 0.1
  });

  activeMesh = new THREE.Mesh(geometry, material);
  scene.add(activeMesh);
}

// Filter Stub Implementation
function applyFilter(filterType) {
  if (!volumeInstance) return;

  switch (filterType) {
    case 'grow':
      console.log('Stub: Applying Grow Filter...');
      // TODO: Perform CSG Smooth Union or Sweep operation in Wasm
      break;
    case 'dig':
      console.log('Stub: Applying Dig Filter...');
      // TODO: Perform CSG Smooth Subtraction operation in Wasm
      break;
    case 'flatten':
      console.log('Stub: Applying Flatten Filter...');
      // TODO: Call volumeInstance.intersectPlane(...)
      break;
  }

  updateMeshFromWasm();
}

function onWindowResize() {
  camera.aspect = window.innerWidth / window.innerHeight;
  camera.updateProjectionMatrix();
  renderer.setSize(window.innerWidth, window.innerHeight);
}

function animate() {
  requestAnimationFrame(animate);
  controls.update();
  renderer.render(scene, camera);
}

init();