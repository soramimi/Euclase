#include "RoundBrushGenerator.h"
#include "ApplicationGlobal.h"
#include <math.h>
#include <algorithm>

// Moler-Morrison Algorithm
static inline float mm_hypot(float a, float b)
{
	a = fabs(a);
	b = fabs(b);
	if (a < b) std::swap(a, b);
	if (b == 0) return a;
	float s;

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


RoundBrushGenerator::RoundBrushGenerator(float size, float softness)
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

float RoundBrushGenerator::level(float x, float y)
{
	float value = 0;
	float d = HYPOT(x, y);
	if (d > radius) {
		value = 0;
	} else if (d > blur && mul > 0) {
		float t = (d - blur) * mul;
		if (t < 1) {
			float u = 1 - t;
			value = u * u * (u + t * 3);
		}
	} else {
		value = 1;
	}
	return value;
}

euclase::Image RoundBrushGenerator::image(int w, int h, float cx, float cy, QColor const &color) const
{
#ifdef USE_CUDA
	if (global->cuda) {
		euclase::Image image(w, h, euclase::Image::Format_F32_RGBA, euclase::Image::CUDA);
		image.fill(color);
		global->cuda->round_brush(w, h, cx, cy, radius, blur, mul, image.data());
		return image;
	}
#endif

	euclase::Float32RGBA c = euclase::Float32RGBA::convert(euclase::OctetRGBA(
		color.red(),
		color.green(),
		color.blue()
		));

	euclase::Image image(w, h, euclase::Image::Format_F32_RGBA);
	for (int i = 0; i < h; i++) {
		for (int j = 0; j < w; j++) {
			euclase::Float32RGBA *dst = (euclase::Float32RGBA *)image.scanLine(i);
			float tx = j + 0.5;
			float ty = i + 0.5;
			float x = tx - cx;
			float y = ty - cy;
			float value = 0;
			float d = hypot(x, y);
			if (d > radius) {
				value = 0;
			} else if (d > blur && mul > 0) {
				float t = (d - blur) * mul;
				if (t < 1) {
					float u = 1 - t;
					value = u * u * (u + t * 3);
				}
			} else {
				value = 1;
			}
			dst[j] = c;
			dst[j].a = value;
		}
	}
	return image;
}
