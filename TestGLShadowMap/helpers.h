#pragma once

class Window {
public:
	int width, height;
	Window() { width = height = 1; }
} window;

class Camera {
public:
	Camera() {
		pos = Vec3(0, -1, 0.75);
		up = Vec3(0, 0, 1);
	}

	Vec3 pos;
	Vec3 lookAt;
	Vec3 up;
} camera;

class CameraController {
public:
	CameraController() {
		leftDrag = false;
	}

	void inputMotion(int x, int y) {
		Vec3 mouseCurrPos = Vec3(x, y, 0);
		if (leftDrag) {
			Vec3 mouseDelta = mouseCurrPos - dragStart.mousePos;
			Vec3 r = camera.lookAt - dragStart.cameraPos;
			double xrad = mouseDelta.x * 0.01;
			double yrad = mouseDelta.y * -0.01;
			Vec3 newPitchYaw = dragStart.cameraPitchYaw + Vec3(-yrad, -xrad, 0);
			camera.pos = camera.lookAt + fromPitchYaw(newPitchYaw);
		}
	}

	void inputMouse(int button, int state, int x, int y) {
		if (button == GLUT_LEFT_BUTTON) {
			leftDrag = state == GLUT_DOWN;
			dragStart.mousePos = Vec3(x, y, 0);
			dragStart.cameraPos = camera.pos;
			dragStart.cameraPitchYaw = toPitchYaw(camera.pos - camera.lookAt);
		}
	}

	void inputWheel(int wheel, int direction, int x, int y) {
		Vec3 r = camera.pos - camera.lookAt;
		if (direction < 0) r = r * 1.1;
		else if (direction > 0) r = r / 1.1;
		camera.pos = camera.lookAt + r;
	}

	bool leftDrag;
	struct {
		Vec3 mousePos;
		Vec3 cameraPos;
		Vec3 cameraPitchYaw;
	} dragStart;

private:
	static Vec3 toPitchYaw(Vec3 const& p) {
		Real dist = p.mag();
		if (dist == 0) return Vec3(0, 0, 1);
		Vec3 pn = p / dist;
		Real pitch = asin(pn.z);
		Real yaw = atan2(pn.y, pn.x);
		return Vec3(pitch, yaw, dist);
	}

	static Vec3 fromPitchYaw(Vec3 const& py) {
		Real pitch = py.x, yaw = py.y, dist = py.z;
		Vec3 r = Vec3(cos(pitch), 0, sin(pitch));
		Vec3 xaxis = Vec3(cos(yaw), sin(yaw), 0);
		Vec3 yaxis = Vec3(-xaxis.y, xaxis.x, 0);
		return (xaxis * r.x + yaxis * r.y + Vec3(0, 0, r.z)) * dist;
	}
} cameraController;

class Framerate {
public:
	static const int NSAMPLES = 10;
	LARGE_INTEGER t[NSAMPLES], f;
	int i;
	double fps;
	double cap = 100;

	Framerate() {
		i = 0;
		fps = 0;
		::memset(t, 0, sizeof(t));
		::QueryPerformanceFrequency(&f);
		sleept = 0;
	}

	void tick() {
		::QueryPerformanceCounter(&t[i]);
		double dur = (double)(t[i].QuadPart - t[(i + 1) % NSAMPLES].QuadPart) / f.QuadPart;
		dur /= NSAMPLES - 1;
		fps = 1.0 / dur;
		i = (i + 1) % NSAMPLES;
	}

	DWORD sleept;
	void sleep() {
		if (fps <= cap) {
			if (sleept > 0) sleept--;
			return;
		}
		//double currDur = 1.0 / fps;
		//double reqDur = 1.0 / cap;
		//double sleept = reqDur - currDur * 0.5;
		//if (sleept < 0) return;
		//sleept *= 1000.0;
		//printf("%f\n", sleept);
		sleept++;
		//else if (fps < cap) sleept = 0;
		::Sleep((DWORD)sleept);
	}

	double get() { return fps; }
} framerate;


