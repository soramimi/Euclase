
#include "median.h"
#ifdef _WIN32
#define _USE_MATH_DEFINES
#endif
#include <math.h>
#include <vector>
#include <string.h>
#include <stdint.h>
#include "euclase.h"

namespace {

using PixelRGBA = euclase::PixelRGBA;
using PixelGrayA = euclase::PixelGrayA;

//

class median_t {
private:
	int map256_[256];
	int map16_[16];
public:
	median_t()
	{
		clear();
	}
	void clear()
	{
		for (int i = 0; i < 256; i++) {
			map256_[i] = 0;
		}
		for (int i = 0; i < 16; i++) {
			map16_[i] = 0;
		}
	}
	void insert(uint8_t n)
	{
		map256_[n]++;
		map16_[n >> 4]++;
	}
	void remove(uint8_t n)
	{
		map256_[n]--;
		map16_[n >> 4]--;
	}
	uint8_t get()
	{
		int left, right;
		int lower, upper;
		lower = 0;
		upper = 0;
		left = 0;
		right = 15;
		while (left < right) {
			if (lower + map16_[left] < upper + map16_[right]) {
				lower += map16_[left];
				left++;
			} else {
				upper += map16_[right];
				right--;
			}
		}
		left *= 16;
		right = left + 15;
		while (left < right) {
			if (lower + map256_[left] < upper + map256_[right]) {
				lower += map256_[left];
				left++;
			} else {
				upper += map256_[right];
				right--;
			}
		}
		return left;
	}

};

struct median_filter_rgb_t {
	median_t r;
	median_t g;
	median_t b;
	void insert(PixelRGBA const &p)
	{
		r.insert(p.r);
		g.insert(p.g);
		b.insert(p.b);
	}
	void remove(PixelRGBA const &p)
	{
		r.remove(p.r);
		g.remove(p.g);
		b.remove(p.b);
	}
	PixelRGBA get(uint8_t a)
	{
		return PixelRGBA(r.get(), g.get(), b.get(), a);
	}
};

struct median_filter_y_t {
	median_t l;
	void insert(PixelGrayA const &p)
	{
		l.insert(p.l);
	}
	void remove(PixelGrayA const &p)
	{
		l.remove(p.l);
	}
	PixelGrayA get(uint8_t a)
	{
		return PixelGrayA(l.get(), a);
	}
};

class minimize_t {
private:
	int map256_[256];
	int map16_[16];
public:
	minimize_t()
	{
		clear();
	}
	void clear()
	{
		for (int i = 0; i < 256; i++) {
			map256_[i] = 0;
		}
		for (int i = 0; i < 16; i++) {
			map16_[i] = 0;
		}
	}
	void insert(uint8_t n)
	{
		map256_[n]++;
		map16_[n >> 4]++;
	}
	void remove(uint8_t n)
	{
		map256_[n]--;
		map16_[n >> 4]--;
	}
	uint8_t get()
	{
		int left, right;
		for (left = 0; left < 16; left++) {
			if (map16_[left] != 0) {
				left *= 16;
				right = left + 16;
				while (left < right) {
					if (map256_[left] != 0) {
						return left;
					}
					left++;
				}
				break;
			}
		}
		return 0;
	}

};

struct minimize_filter_rgb_t {
	minimize_t r;
	minimize_t g;
	minimize_t b;
	void insert(PixelRGBA const &p)
	{
		r.insert(p.r);
		g.insert(p.g);
		b.insert(p.b);
	}
	void remove(PixelRGBA const &p)
	{
		r.remove(p.r);
		g.remove(p.g);
		b.remove(p.b);
	}
	PixelRGBA get(uint8_t a)
	{
		return PixelRGBA(r.get(), g.get(), b.get(), a);
	}
};

struct minimize_filter_y_t {
	minimize_t l;
	void insert(PixelGrayA const &p)
	{
		l.insert(p.l);
	}
	void remove(PixelGrayA const &p)
	{
		l.remove(p.l);
	}
	PixelGrayA get(uint8_t a)
	{
		return PixelGrayA(l.get(), a);
	}
};


class maximize_t {
private:
	int map256_[256];
	int map16_[16];
public:
	maximize_t()
	{
		clear();
	}
	void clear()
	{
		for (int i = 0; i < 256; i++) {
			map256_[i] = 0;
		}
		for (int i = 0; i < 16; i++) {
			map16_[i] = 0;
		}
	}
	void insert(uint8_t n)
	{
		map256_[n]++;
		map16_[n >> 4]++;
	}
	void remove(uint8_t n)
	{
		map256_[n]--;
		map16_[n >> 4]--;
	}
	uint8_t get()
	{
		int left, right;
		right = 16;
		while (right > 0) {
			right--;
			if (map16_[right] != 0) {
				left = right * 16;
				right = left + 16;
				while (left < right) {
					right--;
					if (map256_[right] != 0) {
						return right;
					}
				}
				break;
			}
		}
		return 0;
	}

};

struct maximize_filter_rgb_t {
	maximize_t r;
	maximize_t g;
	maximize_t b;
	void insert(PixelRGBA const &p)
	{
		r.insert(p.r);
		g.insert(p.g);
		b.insert(p.b);
	}
	void remove(PixelRGBA const &p)
	{
		r.remove(p.r);
		g.remove(p.g);
		b.remove(p.b);
	}
	PixelRGBA get(uint8_t a)
	{
		return PixelRGBA(r.get(), g.get(), b.get(), a);
	}
};

struct maximize_filter_y_t {
	maximize_t l;
	void insert(PixelGrayA const &p)
	{
		l.insert(p.l);
	}
	void remove(PixelGrayA const &p)
	{
		l.remove(p.l);
	}
	PixelGrayA get(uint8_t a)
	{
		return PixelGrayA(l.get(), a);
	}
};



template <typename PIXEL, typename FILTER> euclase::Image Filter(euclase::Image image, int radius)
{
	int w = image.width();
	int h = image.height();
	if (w > 0 && h > 0) {
		std::vector<int> shape(radius * 2 + 1);
		{
			for (int y = 0; y < radius; y++) {
				double t = asin((radius - (y + 0.5)) / radius);
				double x = floor(cos(t) * radius + 0.5);
				shape[y] = x;
				shape[radius * 2 - y] = x;
			}
			shape[radius] = radius;
		}

		int sw = w + radius * 2;
		int sh = h + radius * 2;
		std::vector<PIXEL> src(sw * sh);
		PIXEL *dst = (PIXEL *)image.scanLine(0);

		for (int y = 0; y < h; y++) {
			PIXEL *d = (PIXEL *)&src[(y + radius) * sw + radius];
			PIXEL *s = (PIXEL *)image.scanLine(y);
			memcpy(d, s, sizeof(PIXEL) * w);
		}

#pragma omp parallel for
		for (int y = 0; y < h; y++) {
			FILTER filter;
			for (int i = 0; i < radius * 2 + 1; i++) {
				for (int x = 0; x < shape[i]; x++) {
					PIXEL rgb = src[(y + i) * sw + radius + x];
					if (rgb.a > 0) {
						filter.insert(rgb);
					}
				}
			}
			for (int x = 0; x < w; x++) {
				for (int i = 0; i < radius * 2 + 1; i++) {
					PIXEL pix = src[(y + i) * sw + x + radius + shape[i]];
					if (pix.a > 0) {
						filter.insert(pix);
					}
				}

				PIXEL pix = src[(radius + y) * sw + radius + x];
				if (pix.a > 0) {
					pix = filter.get(pix.a);
				}
				dst[y * w + x] = pix;

				for (int i = 0; i < radius * 2 + 1; i++) {
					PIXEL pix = src[(y + i) * sw + x + radius - shape[i]];
					if (pix.a > 0) {
						filter.remove(pix);
					}
				}
			}
		}
	}
	return image;
}

} // namespace


euclase::Image filter_median(euclase::Image image, int radius)
{
//	image = image.convertToFormat(QImage::Format_RGBA8888);
	return Filter<PixelRGBA, median_filter_rgb_t>(image, radius);
}

euclase::Image filter_maximize(euclase::Image image, int radius)
{
//	image = image.convertToFormat(QImage::Format_RGBA8888);
	return Filter<PixelRGBA, maximize_filter_rgb_t>(image, radius);
}

euclase::Image filter_minimize(euclase::Image image, int radius)
{
//	image = image.convertToFormat(QImage::Format_RGBA8888);
	return Filter<PixelRGBA, minimize_filter_rgb_t>(image, radius);
}



