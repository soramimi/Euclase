
#include "median.h"
#ifdef _WIN32
#define _USE_MATH_DEFINES
#endif
#include <math.h>
#include <vector>
#include <string.h>
#include <stdint.h>
#include "euclase.h"
#include "FilterStatus.h"

namespace {

using OctetRGBA = euclase::OctetRGBA;
using OctetGrayA = euclase::OctetGrayA;

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
	void insert(OctetRGBA const &p)
	{
		r.insert(p.r);
		g.insert(p.g);
		b.insert(p.b);
	}
	void remove(OctetRGBA const &p)
	{
		r.remove(p.r);
		g.remove(p.g);
		b.remove(p.b);
	}
	OctetRGBA get(uint8_t a)
	{
		return OctetRGBA(r.get(), g.get(), b.get(), a);
	}
};

struct median_filter_y_t {
	median_t l;
	void insert(OctetGrayA const &p)
	{
		l.insert(p.v);
	}
	void remove(OctetGrayA const &p)
	{
		l.remove(p.v);
	}
	OctetGrayA get(uint8_t a)
	{
		return OctetGrayA(l.get(), a);
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
	void insert(OctetRGBA const &p)
	{
		r.insert(p.r);
		g.insert(p.g);
		b.insert(p.b);
	}
	void remove(OctetRGBA const &p)
	{
		r.remove(p.r);
		g.remove(p.g);
		b.remove(p.b);
	}
	OctetRGBA get(uint8_t a)
	{
		return OctetRGBA(r.get(), g.get(), b.get(), a);
	}
};

struct minimize_filter_y_t {
	minimize_t l;
	void insert(OctetGrayA const &p)
	{
		l.insert(p.v);
	}
	void remove(OctetGrayA const &p)
	{
		l.remove(p.v);
	}
	OctetGrayA get(uint8_t a)
	{
		return OctetGrayA(l.get(), a);
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
	void insert(OctetRGBA const &p)
	{
		r.insert(p.r);
		g.insert(p.g);
		b.insert(p.b);
	}
	void remove(OctetRGBA const &p)
	{
		r.remove(p.r);
		g.remove(p.g);
		b.remove(p.b);
	}
	OctetRGBA get(uint8_t a)
	{
		return OctetRGBA(r.get(), g.get(), b.get(), a);
	}
};

struct maximize_filter_y_t {
	maximize_t l;
	void insert(OctetGrayA const &p)
	{
		l.insert(p.v);
	}
	void remove(OctetGrayA const &p)
	{
		l.remove(p.v);
	}
	OctetGrayA get(uint8_t a)
	{
		return OctetGrayA(l.get(), a);
	}
};



template <typename PIXEL, typename FILTER> euclase::Image Filter(euclase::Image image, int radius, FilterStatus *status)
{
	auto isInterrupted = [&](){
		return status && status->cancel && *status->cancel;
	};
	auto progress = [&](float v){
		if (status && status->progress) {
			*status->progress = v;
		}
	};
	int w = image.width();
	int h = image.height();
	euclase::Image newimage(w, h, image.format());
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
		PIXEL *dst = (PIXEL *)newimage.scanLine(0);

		for (int y = 0; y < h; y++) {
			if (isInterrupted()) return {};
			PIXEL *d = (PIXEL *)&src[(y + radius) * sw + radius];
			PIXEL *s = (PIXEL *)image.scanLine(y);
			memcpy(d, s, sizeof(PIXEL) * w);
		}

		std::atomic_int rows = 0;

#pragma omp parallel for schedule(static, 8)
		for (int y = 0; y < h; y++) {
			if (isInterrupted()) continue;

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
				if (isInterrupted()) break;

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
			progress((float)++rows / h);
		}
	}
	progress(1.0f);
	return newimage;
}

} // namespace

euclase::Image filter_median(euclase::Image const &image, int radius, FilterStatus *status)
{
	if (image.memtype() != euclase::Image::Host) {
		return filter_median(image.toHost(), radius, status);
	}

	auto format = image.format();

	if (format == euclase::Image::Format_F_RGBA) {
		euclase::Image tmpimg = image.convertToFormat(euclase::Image::Format_8_RGBA).toHost();
		tmpimg = filter_median(tmpimg, radius, status);
		return tmpimg.makeFPImage();
	}
	if (format == euclase::Image::Format_F_GrayscaleA) {
		euclase::Image tmpimg = image.convertToFormat(euclase::Image::Format_8_GrayscaleA).toHost();
		tmpimg = filter_median(tmpimg, radius, status);
		return tmpimg.convertToFormat(format);
	}

	if (format == euclase::Image::Format_8_RGB) {
		euclase::Image tmpimg = image.convertToFormat(euclase::Image::Format_8_RGBA).toHost();
		tmpimg = filter_median(tmpimg, radius, status);
		return tmpimg.convertToFormat(format);
	}
	if (format == euclase::Image::Format_8_Grayscale) {
		euclase::Image tmpimg = image.convertToFormat(euclase::Image::Format_8_GrayscaleA).toHost();
		tmpimg = filter_median(tmpimg, radius, status);
		return tmpimg.convertToFormat(format);
	}

	if (format == euclase::Image::Format_8_RGBA) {
		return Filter<OctetRGBA, median_filter_rgb_t>(image, radius, status);
	}
	if (format == euclase::Image::Format_8_GrayscaleA) {
		return Filter<OctetGrayA, median_filter_y_t>(image, radius, status);
	}
	return {};
}

euclase::Image filter_maximize(euclase::Image const &image, int radius, FilterStatus *status)
{
	if (image.memtype() != euclase::Image::Host) {
		return filter_maximize(image.toHost(), radius, status);
	}
	if (image.format() == euclase::Image::Format_F_RGBA) {
		euclase::Image tmpimg = image.convertToFormat(euclase::Image::Format_8_RGBA).toHost();
		tmpimg = filter_maximize(tmpimg, radius, status);
		return tmpimg.makeFPImage();
	}

	return Filter<OctetRGBA, maximize_filter_rgb_t>(image, radius, status);
}

euclase::Image filter_minimize(euclase::Image const &image, int radius, FilterStatus *status)
{
	if (image.memtype() != euclase::Image::Host) {
		return filter_minimize(image.toHost(), radius, status);
	}
	if (image.format() == euclase::Image::Format_F_RGBA) {
		euclase::Image tmpimg = image.convertToFormat(euclase::Image::Format_8_RGBA).toHost();
		tmpimg = filter_minimize(tmpimg, radius, status);
		return tmpimg.makeFPImage();
	}

	return Filter<OctetRGBA, minimize_filter_rgb_t>(image, radius, status);
}



