import { Filter } from './filter.js';
import * as THREE from 'three';

const CARVE_THRESHOLD_FACTOR = 0.5;
const CARVE_CHECK_INTERVAL_MS = 100;

export class SphereCarveFilter extends Filter {
	constructor(options) {
		super(options);
		this.onMeshUpdated = options.onMeshUpdated;
		this.currentPickPoint = new THREE.Vector3();
		this.lastCarvePoint = new THREE.Vector3();
		this.carvingActive = false;
	}

	onPointerDown(point) {
		this.currentPickPoint.copy(point);
		this.lastCarvePoint.copy(point);
		this.carvingActive = true;

	}

	onPointerMove(point) {
		this.currentPickPoint.copy(point);
		this.carveAtPoint(point);
		requestAnimationFrame(this.carveLoop);
	}

	onPointerUp() {
		this.carvingActive = false;
	}

	computeRadiusWorld() {
		const bounds = new THREE.Box3().setFromObject(this.mesh);
		const size = new THREE.Vector3();
		bounds.getSize(size);
		return size.length() * 0.02;
	}

	carveLoop = () => {
		if (!this.carvingActive) return;

		const threshold = this.computeRadiusWorld() * CARVE_THRESHOLD_FACTOR;
		if (this.currentPickPoint.distanceTo(this.lastCarvePoint) >= threshold) {
			this.lastCarvePoint.copy(this.currentPickPoint);
			this.carveAtPoint(this.currentPickPoint);
		}

		setTimeout(() => requestAnimationFrame(this.carveLoop), CARVE_CHECK_INTERVAL_MS);
	};

	carveAtPoint(point) {
		if (typeof this.wasmModule?.carveSphere !== 'function') {
			throw new Error('carveSphere is not available on the WebAssembly module');
		}
		try {
			this.wasmModule.carveSphereAt(this.volume, point.x, point.y, point.z, this.computeRadiusWorld());
			this.onMeshUpdated();
		} catch (err) {
			console.error('Error calling carveSphere:', err);
		}
	}
}
