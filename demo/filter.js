import * as THREE from 'three';

export class Filter {
	constructor({ canvas, camera, controls, volume, mesh, wasmModule }) {
		if (new.target === Filter) {
			throw new TypeError('Filter is an abstract base class');
		}

		this.canvas = canvas;
		this.camera = camera;
		this.controls = controls;
		this.volume = volume;
		this.mesh = mesh;
		this.wasmModule = wasmModule;

		this.raycaster = new THREE.Raycaster();
		this.mouseNDC = new THREE.Vector2();
		this.enabled = false;
		this.pointerDown = false;
		this.controlsWasEnabled = true;
		this.controlsDisabled = false;

		this.canvas.style.touchAction = 'none';
		this.canvas.addEventListener('pointerdown', this.onCanvasPointerDown, { capture: true });
		this.canvas.addEventListener('pointermove', this.onCanvasPointerMove);
		window.addEventListener('pointerup', this.onWindowPointerUp);
	}

	activate() {
		this.enabled = true;
	}

	deactivate() {
		this.enabled = false;
		this.finishInteraction();
	}

	dispose() {
		this.deactivate();
		this.canvas.removeEventListener('pointerdown', this.onCanvasPointerDown, { capture: true });
		this.canvas.removeEventListener('pointermove', this.onCanvasPointerMove);
		window.removeEventListener('pointerup', this.onWindowPointerUp);
	}

	onPointerDown() {}

	onPointerMove() {}

	onPointerUp() {}

	updateMouseNDC(event) {
		const rect = this.canvas.getBoundingClientRect();
		this.mouseNDC.x = ((event.clientX - rect.left) / rect.width) * 2 - 1;
		this.mouseNDC.y = -((event.clientY - rect.top) / rect.height) * 2 + 1;
	}

	pickPoint() {
		this.raycaster.setFromCamera(this.mouseNDC, this.camera);
		const intersections = this.raycaster.intersectObject(this.mesh, true);
		return intersections.length ? intersections[0].point.clone() : null;
	}

	onCanvasPointerDown = (event) => {
		this.updateMouseNDC(event);
		if (!this.enabled) return;

		event.preventDefault();
		event.stopPropagation();

		let point = this.pickPoint();
		const hitMesh = point !== null;
		if (hitMesh) {
			this.controlsWasEnabled = this.controls.enabled;
			this.controls.enabled = false;
			this.controlsDisabled = true;
		}

		if (point) {
			this.pointerDown = true;
			this.onPointerDown(point, hitMesh);
		}
	};

	onCanvasPointerMove = (event) => {
		if (!this.enabled || !this.pointerDown) return;
		this.updateMouseNDC(event);
		const point = this.pickPoint();
		if (point) this.onPointerMove(point);
	};

	onWindowPointerUp = () => {
		this.finishInteraction();
	};

	finishInteraction() {
		this.pointerDown = false;
		this.onPointerUp();
		if (this.controlsDisabled) {
			this.controls.enabled = this.controlsWasEnabled;
			this.controlsDisabled = false;
		}
	}
}
