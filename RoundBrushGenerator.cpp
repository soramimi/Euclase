#include "RoundBrushGenerator.h"
#include <math.h>
#include <algorithm>

// Moler-Morrison Algorithm
static inline double mm_hypot(double a, double b)
{
	a = fabs(a);
	b = fabs(b);
	if (a < b) std::swap(a, b);
	if (b == 0) return a;
	double s;

	s = (b / a) * (b / a);
	s /= 4 + s;
	a += 2 * a * s;
	b *= s;

	s = (b / a) * (b / a);
	s /= 4 + s;
	a += 2 * a * s;
	b *= s;

	s = (b / a) * (b / a);
	s /= 4 + s;
	a += 2 * a * s;
//	b *= s;

	return a;
}

//#define HYPOT(A, B) hypot(A, B)
#define HYPOT(A, B) sqrt((A) * (A) + (B) * (B))
//#define HYPOT(A, B) mm_hypot(A, B)


RoundBrushGenerator::RoundBrushGenerator(double size, double softness)
{
	radius = float(size / 2 + 0.25);
	blur = float(radius - radius * softness);
	mul = radius - blur;
	if (mul > 0) {
		mul = 1 / mul;
	} else {
		mul = -1;
	}
}

double RoundBrushGenerator::level(double x, double y)
{
	double value = 0;
	double d = HYPOT(x, y);
	if (d > radius) {
		value = 0;
	} else if (d > blur && mul > 0) {
		double t = (d - blur) * mul;
		if (t < 1) {
			double u = 1 - t;
			value = u * u * (u + t * 3);
		}
	} else {
		value = 1;
	}
	return value;
}
