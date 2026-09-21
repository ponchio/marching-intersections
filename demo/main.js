import * as THREE from 'three';
import { OrbitControls } from 'three/examples/jsm/controls/OrbitControls.js';
import { OBJLoader } from 'three/examples/jsm/loaders/OBJLoader.js';
import createModule from '../dist/marching_lib_wasm.js';

let Module, scene, camera, renderer, controls;
let activeMesh = null;
let volumeInstance = null;
// Raycasting / carving state
const raycaster = new THREE.Raycaster();
const mouseNDC = new THREE.Vector2();
let isPointerDown = false;
let currentPickPoint = new THREE.Vector3();
let lastCarvePoint = new THREE.Vector3();
let carvingActive = false;
const CARVE_THRESHOLD_FACTOR = 0.5; // require movement >= 50% radius to trigger next carve
const CARVE_CHECK_INTERVAL_MS = 100; // loop check interval
// Tool & control state
let currentTool = null; // 'dig' when carving tool is active
let controlsWasEnabledBeforeCarve = true;
let controlsDisabledByCarve = false;

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

  // 5. Setup carving mouse handlers
  setupCarvingHandlers();

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

  // Sculpting Filter Buttons + dig-tool toggle
  const growBtn = document.getElementById('btn-filter-grow');
  const digBtn = document.getElementById('btn-filter-dig');
  const flattenBtn = document.getElementById('btn-filter-flatten');

  growBtn.addEventListener('click', () => {
    currentTool = null;
    digBtn.classList.remove('btn-primary');
    applyFilter('grow');
  });

  digBtn.addEventListener('click', () => {
    if (currentTool === 'dig') {
      // deactivate tool
      currentTool = null;
      digBtn.classList.remove('btn-primary');
      // restore controls if we left them disabled
      if (controlsDisabledByCarve && controls) {
        controls.enabled = controlsWasEnabledBeforeCarve;
        controlsDisabledByCarve = false;
      }
    } else {
      // activate dig tool
      currentTool = 'dig';
      digBtn.classList.add('btn-primary');
      growBtn.classList.remove('btn-primary');
      flattenBtn.classList.remove('btn-primary');
    }
  });

  flattenBtn.addEventListener('click', () => {
    currentTool = null;
    digBtn.classList.remove('btn-primary');
    applyFilter('flatten');
  });
}

// Setup pointer event handlers on the renderer canvas to perform sphere carving
function setupCarvingHandlers() {
  if (!renderer) return;
  const canvas = renderer.domElement;
  canvas.style.touchAction = 'none';

  function updateMouseNDC(e) {
    const rect = canvas.getBoundingClientRect();
    mouseNDC.x = ((e.clientX - rect.left) / rect.width) * 2 - 1;
    mouseNDC.y = -((e.clientY - rect.top) / rect.height) * 2 + 1;
  }

  function pickPoint() {
    if (!activeMesh) return null;
    raycaster.setFromCamera(mouseNDC, camera);
    const intersects = raycaster.intersectObject(activeMesh, true);

    if (intersects.length > 0) {
      console.log(intersects[0].point);
      return intersects[0].point.clone();
    }

    return null;
  }

  function computeRadiusWorld() {
    if (!activeMesh) return 0.1;
    const bbox = new THREE.Box3().setFromObject(activeMesh);
    const size = new THREE.Vector3();
    bbox.getSize(size);
    // Use diagonal fraction (2%) as requested
    return size.length() * 0.02;
  }

  async function applyCarveAtPoint(pt) {
    if (!volumeInstance || !Module || !activeMesh) return;
    const radius = computeRadiusWorld();
    if (typeof Module.carveSphere === 'function') {
      try {
        Module.carveSphere(volumeInstance, pt.x, pt.y, pt.z, radius);
        updateMeshFromWasm();
      } catch (err) {
        console.error('Error calling carveSphere:', err);
      }
    }
  }

  function carveLoop() {
    if (!carvingActive) return;
    const radius = computeRadiusWorld();
    const threshold = radius * CARVE_THRESHOLD_FACTOR;
    const dist = currentPickPoint.distanceTo(lastCarvePoint);
    if (dist >= threshold) {
      lastCarvePoint.copy(currentPickPoint);
      applyCarveAtPoint(currentPickPoint);
    }
    setTimeout(() => requestAnimationFrame(carveLoop), CARVE_CHECK_INTERVAL_MS);
  }

  // Intercept pointerdown in capture phase when dig tool active so OrbitControls doesn't start rotating
  canvas.addEventListener('pointerdown', (e) => {
    updateMouseNDC(e);
    if (currentTool !== 'dig') return; // not in dig mode; let OrbitControls handle it

    // We intend to carve: prevent OrbitControls from reacting
    e.preventDefault();
    e.stopPropagation();

    if (!activeMesh) return;
    isPointerDown = true;

    // Raycast against mesh to determine if click was on the mesh
    raycaster.setFromCamera(mouseNDC, camera);
    const intersects = raycaster.intersectObject(activeMesh, true);
    let p = null;
    if (intersects.length > 0) {
      p = intersects[0].point.clone();
      // Disable OrbitControls for the duration of the carve interaction
      if (controls) {
        controlsWasEnabledBeforeCarve = controls.enabled;
        controls.enabled = false;
        controlsDisabledByCarve = true;
      }
    } else {
      // fallback to horizontal plane at y=0 (do not disable controls)
      const plane = new THREE.Plane(new THREE.Vector3(0, 1, 0), 0);
      p = new THREE.Vector3();
      raycaster.ray.intersectPlane(plane, p);
    }

    if (!p) return;
    currentPickPoint.copy(p);
    lastCarvePoint.copy(p);
    carvingActive = true;
    applyCarveAtPoint(p);
    requestAnimationFrame(carveLoop);
  }, { capture: true });

  canvas.addEventListener('pointermove', (e) => {
    updateMouseNDC(e);
    const p = pickPoint();
    if (p) currentPickPoint.copy(p);
  });

  window.addEventListener('pointerup', () => {
    isPointerDown = false;
    carvingActive = false;
    // restore OrbitControls if we disabled them during carving
    if (controlsDisabledByCarve && controls) {
      controls.enabled = controlsWasEnabledBeforeCarve;
      controlsDisabledByCarve = false;
    }
  });
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
      console.log('Applying Sphere Carve Filter...');
      if (Module && typeof Module.carveSphere === 'function' && activeMesh) {
        // carve at the mesh bounding-box center with a 2% radius
        const bbox = new THREE.Box3().setFromObject(activeMesh);
        const center = new THREE.Vector3();
        bbox.getCenter(center);
        const size = new THREE.Vector3();
        bbox.getSize(size);
        const radius = size.length() * 0.02;
        Module.carveSphere(volumeInstance, center.x, center.y, center.z, radius);
      } else {
        console.warn('carveSphere not available on Module or no active mesh');
      }
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