import * as THREE from 'three';
import { OrbitControls } from 'three/examples/jsm/controls/OrbitControls.js';
import { OBJLoader } from 'three/examples/jsm/loaders/OBJLoader.js';
import createModule from '../dist/marching_lib_wasm.js';
import { SphereCarveFilter } from './sphere-carve-filter.js';


let Module, scene, camera, renderer, controls;
let activeMesh = null;
let volumeInstance = null;
// Tool & control state
let currentTool = null; // 'dig' when carving tool is active
let digFilter = null;

async function init() {
	// 1. Initialize Emscripten Wasm Engine
	Module = await createModule();
	volumeInstance = new Module.Volume();

	// 2. Setup Three.js Scene
	const container = document.getElementById('canvas-container');
	scene = new THREE.Scene();
	scene.background = new THREE.Color(0x1e1e1e);

	const material = new THREE.MeshStandardMaterial({
		color: 0xffffff,
		roughness: 0.3,
		metalness: 0.1
	});

	activeMesh = new THREE.Mesh(new THREE.BufferGeometry(), material);
	scene.add(activeMesh);

	camera = new THREE.PerspectiveCamera(45, window.innerWidth / window.innerHeight, 0.1, 1000);
	camera.position.set(0, 2, 5);

	renderer = new THREE.WebGLRenderer({ antialias: true });
	renderer.setSize(window.innerWidth, window.innerHeight);
	renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
	container.appendChild(renderer.domElement);

	controls = new OrbitControls(camera, renderer.domElement);
	controls.enableDamping = true;
	digFilter = new SphereCarveFilter({
		canvas: renderer.domElement,
		camera,
		controls,
		mesh: activeMesh,
		volume: volumeInstance,
		wasmModule: Module,
		onMeshUpdated: updateMeshFromWasm
	});

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
	// Step factor control is provided statically in index.html


	// Sculpting Filter Buttons + dig-tool toggle
	const growBtn = document.getElementById('btn-filter-grow');
	const digBtn = document.getElementById('btn-filter-dig');
	const flattenBtn = document.getElementById('btn-filter-flatten');

	growBtn.addEventListener('click', () => {
		currentTool = null;
		digFilter.deactivate();
		digBtn.classList.remove('btn-primary');
	});

	digBtn.addEventListener('click', () => {
		if (currentTool === 'dig') {
			// deactivate tool
			currentTool = null;
			digFilter.deactivate();
			digBtn.classList.remove('btn-primary');
		} else {
			// activate dig tool
			currentTool = 'dig';
			digFilter.activate();
			digBtn.classList.add('btn-primary');
			growBtn.classList.remove('btn-primary');
			flattenBtn.classList.remove('btn-primary');
		}
	});

	flattenBtn.addEventListener('click', () => {
		currentTool = null;
		digFilter.deactivate();
		digBtn.classList.remove('btn-primary');
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

			const bbox = new THREE.Box3().setFromObject(activeMesh);
			const size = new THREE.Vector3();
			bbox.getSize(size);
			const maxSize = Math.max(size.x, size.y, size.z);
			if (maxSize <= 0) throw "Invalid box!";

			const center = new THREE.Vector3();
			bbox.getCenter(center);

			// Move mesh so its bbox center sits at world origin
			//activeMesh.position.set(-center.x, -center.y, -center.z);

			// Position camera farther back along +Z to fit the object
			const fov = (camera.fov || 45) * Math.PI / 180;
			const distance = (maxSize / 2) / Math.tan(fov / 2);
			camera.position.set(center.x, center.y, center.z + distance * 1.5 + 0.5);
			//camera.position.set(0, 0, distance * 1.5 + 0.5);
			camera.lookAt(0, 0, 0);
			controls.target.set(0, 0, 0);
			controls.update();
		}
	});
}


function updateMeshFromWasm() {
	const { vertices, indices } = volumeInstance.toMesh();

	const geometry = new THREE.BufferGeometry();
	geometry.setAttribute('position', new THREE.BufferAttribute(vertices, 3));
	geometry.setIndex(new THREE.BufferAttribute(indices, 1));
	geometry.computeVertexNormals();

	//if (activeMesh) {
		activeMesh.geometry.dispose();
		activeMesh.geometry = geometry;
	//	return;
	//}

	//activeMesh = new THREE.Mesh(geometry, material);
	//scene.add(activeMesh);
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
