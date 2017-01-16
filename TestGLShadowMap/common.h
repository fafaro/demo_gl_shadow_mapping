#pragma once

#include <cmath>

typedef unsigned int uint;
typedef double Real;

class Vec3 {
public:
	union {
		Real m[3];
		struct { Real x, y, z; };
	};
	Vec3() { x = y = z = 0; }
	Vec3(Real x, Real y, Real z) { this->x = x; this->y = y; this->z = z; }
	Vec3 operator+ (Vec3 const& rhs) const { return Vec3(x + rhs.x, y + rhs.y, z + rhs.z); }
	Vec3 operator- (Vec3 const& rhs) const { return Vec3(x - rhs.x, y - rhs.y, z - rhs.z); }
	Vec3 operator* (Real s) const { return Vec3(x * s, y * s, z * s); }
	Vec3 operator/ (Real s) const { Real inv = 1.0 / s; return *this * inv; }
	Vec3 operator+= (Vec3 const& rhs) { x += rhs.x; y += rhs.y; z += rhs.z; return *this; }
	Vec3 operator-= (Vec3 const& rhs) { x += rhs.x; y += rhs.y; z += rhs.z; return *this; }
	Real mag() const { return sqrt(x * x + y * y + z * z); }
};