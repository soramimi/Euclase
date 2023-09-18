#include "resize.h"
#include <QImage>
#include <cmath>
#include <cstdint>
#include "euclase.h"
#include <omp.h>
#include "ApplicationGlobal.h"

using FloatRGB = euclase::FloatRGB;
using FloatGray = euclase::FloatGray;
using FloatRGBA = euclase::FloatRGBA;
using FloatGrayA = euclase::FloatGrayA;

namespace {

static double bicubic(double t)
{
	if (t < 0) t = -t;
	double tt = t * t;
	double ttt = t * t * t;
	const double a = -0.5;
	if (t < 1) return (a + 2) * ttt - (a + 3) * tt + 1;
	if (t < 2) return a * ttt - 5 * a * tt + 8 * a * t - 4 * a;
	return 0;
}

template <euclase::Image::Format FORMAT, typename PIXEL>
euclase::Image resizeNearestNeighbor(euclase::Image const &image, int dst_w, int dst_h)
{
	const int src_w = image.width();
	const int src_h = image.height();

	if (image.memtype() == euclase::Image::CUDA) {
		euclase::Image newimg(dst_w, dst_h, FORMAT, image.memtype());
		int psize = euclase::bytesPerPixel(FORMAT);
		global->cuda->scale(dst_w, dst_h, psize * dst_w, newimg.data(), src_w, src_h, psize * src_w, image.data(), psize);
		return newimg;
	}

	euclase::Image newimg(dst_w, dst_h, FORMAT);
#pragma omp parallel for schedule(static, 8)
	for (int y = 0; y < dst_h; y++) {
		double fy = (double)y * src_h / dst_h;
		PIXEL *dst = (PIXEL *)newimg.scanLine(y);
		PIXEL const *src = (PIXEL const *)image.scanLine((int)fy);
		double mul = (double)src_w / dst_w;
		for (int x = 0; x < dst_w; x++) {
			double fx = (double)x * mul;
			dst[x] = src[(int)fx];
		}
	}
	return newimg;
}

template <euclase::Image::Format FORMAT, typename PIXEL, typename FPIXEL>
euclase::Image resizeAveragingT(euclase::Image const &image, int dst_w, int dst_h)
{
	const int src_w = image.width();
	const int src_h = image.height();
	euclase::Image newimg(dst_w, dst_h, (euclase::Image::Format)FORMAT);
#pragma omp parallel for schedule(static, 8)
	for (int y = 0; y < dst_h; y++) {
		double lo_y = (double)y * src_h / dst_h;
		double hi_y = (double)(y + 1) * src_h / dst_h;
		PIXEL *dst = (PIXEL *)newimg.scanLine(y);
		double mul = (double)src_w / dst_w;
		for (int x = 0; x < dst_w; x++) {
			double lo_x = (double)x * mul;
			double hi_x = (double)(x + 1) * mul;
			int lo_iy = (int)lo_y;
			int hi_iy = (int)hi_y;
			int lo_ix = (int)lo_x;
			int hi_ix = (int)hi_x;
			FPIXEL pixel;
			double volume = 0;
			for (int sy = lo_iy; sy <= hi_iy; sy++) {
				double vy = 1;
				if (sy < src_h) {
					if (lo_iy == hi_iy) {
						vy = hi_y - lo_y;
					} else if (sy == lo_iy) {
						vy = 1 - (lo_y - sy);
					} else if (sy == hi_iy) {
						vy = hi_y - sy;
					}
				}
				PIXEL const *src = (PIXEL const *)image.scanLine(sy < src_h ? sy : (src_h - 1));
				for (int sx = lo_ix; sx <= hi_ix; sx++) {
					FPIXEL p = euclase::pixel_cast<FPIXEL>(src[sx < src_w ? sx : (src_w - 1)]);
					double vx = 1;
					if (sx < src_w) {
						if (lo_ix == hi_ix) {
							vx = hi_x - lo_x;
						} else if (sx == lo_ix) {
							vx = 1 - (lo_x - sx);
						} else if (sx == hi_ix) {
							vx = hi_x - sx;
						}
					}
					double v = vy * vx;
					pixel.add(p, v);
					volume += v;
				}
			}
			dst[x] = pixel.color(volume);
		}
	}
	return newimg;
}

template <euclase::Image::Format FORMAT, typename PIXEL, typename FPIXEL>
euclase::Image resizeAveragingHT(euclase::Image const &image, int dst_w)
{
	const int src_w = image.width();
	const int src_h = image.height();
	euclase::Image newimg(dst_w, src_h, (euclase::Image::Format)FORMAT);
#pragma omp parallel for schedule(static, 8)
	for (int y = 0; y < src_h; y++) {
		PIXEL *dst = (PIXEL *)newimg.scanLine(y);
		for (int x = 0; x < dst_w; x++) {
			double lo_x = (double)x * src_w / dst_w;
			double hi_x = (double)(x + 1) * src_w / dst_w;
			int lo_ix = (int)lo_x;
			int hi_ix = (int)hi_x;
			FPIXEL pixel;
			double volume = 0;
			PIXEL const *src = (PIXEL const *)image.scanLine(y < src_h ? y : (src_h - 1));
			for (int sx = lo_ix; sx <= hi_ix; sx++) {
				FPIXEL p = euclase::pixel_cast<FPIXEL>(src[sx < src_w ? sx : (src_w - 1)]);
				double v = 1;
				if (sx < src_w) {
					if (lo_ix == hi_ix) {
						v = hi_x - lo_x;
					} else if (sx == lo_ix) {
						v = 1 - (lo_x - sx);
					} else if (sx == hi_ix) {
						v = hi_x - sx;
					}
				}
				pixel.add(p, v);
				volume += v;
			}
			dst[x] = pixel.color(volume);
		}
	}
	return newimg;
}

template <euclase::Image::Format FORMAT, typename PIXEL, typename FPIXEL>
euclase::Image resizeAveragingVT(euclase::Image const &image, int dst_h)
{
	const int src_w = image.width();
	const int src_h = image.height();
	euclase::Image newimg(src_w, dst_h, FORMAT);
#pragma omp parallel for schedule(static, 8)
	for (int y = 0; y < dst_h; y++) {
		double lo_y = (double)y * src_h / dst_h;
		double hi_y = (double)(y + 1) * src_h / dst_h;
		PIXEL *dst = (PIXEL *)newimg.scanLine(y);
		for (int x = 0; x < src_w; x++) {
			int lo_iy = (int)lo_y;
			int hi_iy = (int)hi_y;
			FPIXEL pixel;
			double volume = 0;
			for (int sy = lo_iy; sy <= hi_iy; sy++) {
				double v = 1;
				if (sy < src_h) {
					if (lo_iy == hi_iy) {
						v = hi_y - lo_y;
					} else if (sy == lo_iy) {
						v = 1 - (lo_y - sy);
					} else if (sy == hi_iy) {
						v = hi_y - sy;
					}
				}
				PIXEL const *src = (PIXEL const *)image.scanLine(sy < src_h ? sy : (src_h - 1));
				FPIXEL p = euclase::pixel_cast<FPIXEL>(src[x]);
				pixel.add(p, v);
				volume += v;
			}
			dst[x] = pixel.color(volume);
		}
	}
	return newimg;
}

struct bilinear_t {
	int i0, i1;
	double v0, v1;
};

template <euclase::Image::Format FORMAT, typename PIXEL, typename FPIXEL>
euclase::Image resizeBilinearT(euclase::Image const &image, int dst_w, int dst_h)
{
	const int src_w = image.width();
	const int src_h = image.height();
	euclase::Image newimg(dst_w, dst_h, (euclase::Image::Format)FORMAT);

	std::vector<bilinear_t> lut(dst_w);
	bilinear_t *lut_p = &lut[0];
	for (int x = 0; x < dst_w; x++) {
		double tx = (double)x * src_w / dst_w - 0.5;
		int x0, x1;
		if (tx < 0) {
			x0 = x1 = 0;
			tx = 0;
		} else {
			x0 = x1 = (int)tx;
			if (x0 + 1 < src_w) {
				x1 = x0 + 1;
				tx -= x0;
			} else {
				x0 = x1 = src_w - 1;
				tx = 0;
			}
		}
		lut_p[x].i0 = x0;
		lut_p[x].i1 = x1;
		lut_p[x].v1 = tx;
		lut_p[x].v0 = 1 - tx;
	}

#pragma omp parallel for schedule(static, 8)
	for (int y = 0; y < dst_h; y++) {
		double yt = (double)y * src_h / dst_h - 0.5;
		int y0, y1;
		if (yt < 0) {
			y0 = y1 = 0;
			yt = 0;
		} else {
			y0 = y1 = (int)yt;
			if (y0 + 1 < src_h) {
				y1 = y0 + 1;
				yt -= y0;
			} else {
				y0 = y1 = src_h - 1;
				yt = 0;
			}
		}
		double ys = 1 - yt;
		PIXEL *dst = (PIXEL *)newimg.scanLine(y);
		PIXEL const *src1 = (PIXEL const *)image.scanLine(y0);
		PIXEL const *src2 = (PIXEL const *)image.scanLine(y1);
		for (int x = 0; x < dst_w; x++) {
			double a11 = lut_p[x].v0 * ys;
			double a12 = lut_p[x].v1 * ys;
			double a21 = lut_p[x].v0 * yt;
			double a22 = lut_p[x].v1 * yt;
			FPIXEL pixel;
			pixel.add(euclase::pixel_cast<FPIXEL>(src1[lut_p[x].i0]), a11);
			pixel.add(euclase::pixel_cast<FPIXEL>(src1[lut_p[x].i1]), a12);
			pixel.add(euclase::pixel_cast<FPIXEL>(src2[lut_p[x].i0]), a21);
			pixel.add(euclase::pixel_cast<FPIXEL>(src2[lut_p[x].i1]), a22);
			dst[x] = pixel.color(a11 + a12 + a21 + a22);
		}
	}
	return newimg;
}

template <euclase::Image::Format FORMAT, typename PIXEL, typename FPIXEL>
euclase::Image resizeBilinearHT(euclase::Image const &image, int dst_w)
{
	const int src_w = image.width();
	const int src_h = image.height();
	euclase::Image newimg(dst_w, src_h, (euclase::Image::Format)FORMAT);
#pragma omp parallel for schedule(static, 8)
	for (int y = 0; y < src_h; y++) {
		PIXEL *dst = (PIXEL *)newimg.scanLine(y);
		PIXEL const *src = (PIXEL const *)image.scanLine(y);
		double mul = (double)src_w / dst_w;
		for (int x = 0; x < dst_w; x++) {
			double xt = (double)x * mul - 0.5;
			int x0, x1;
			if (xt < 0) {
				x0 = x1 = 0;
				xt = 0;
			} else {
				x0 = x1 = (int)xt;
				if (x0 + 1 < src_w) {
					x1 = x0 + 1;
					xt -= x0;
				} else {
					x0 = x1 = src_w - 1;
					xt = 0;
				}
			}
			double xs = 1 - xt;
			FPIXEL p1(euclase::pixel_cast<FPIXEL>(src[x0]));
			FPIXEL p2(euclase::pixel_cast<FPIXEL>(src[x1]));
			FPIXEL p;
			p.add(p1, xs);
			p.add(p2, xt);
			dst[x] = p.color(1);
		}
	}
	return newimg;
}

template <euclase::Image::Format FORMAT, typename PIXEL, typename FPIXEL>
euclase::Image resizeBilinearVT(euclase::Image const &image, int dst_h)
{
	const int src_w = image.width();
	const int src_h = image.height();
	euclase::Image newimg(src_w, dst_h, FORMAT);
#pragma omp parallel for schedule(static, 8)
	for (int y = 0; y < dst_h; y++) {
		double yt = (double)y * src_h / dst_h - 0.5;
		int y0, y1;
		if (yt < 0) {
			y0 = y1 = 0;
			yt = 0;
		} else {
			y0 = y1 = (int)yt;
			if (y0 + 1 < src_h) {
				y1 = y0 + 1;
				yt -= y0;
			} else {
				y0 = y1 = src_h - 1;
				yt = 0;
			}
		}
		double ys = 1 - yt;
		PIXEL *dst = (PIXEL *)newimg.scanLine(y);
		PIXEL const *src1 = (PIXEL const *)image.scanLine(y0);
		PIXEL const *src2 = (PIXEL const *)image.scanLine(y1);
		for (int x = 0; x < src_w; x++) {
			FPIXEL p1(euclase::pixel_cast<FPIXEL>(src1[x]));
			FPIXEL p2(euclase::pixel_cast<FPIXEL>(src2[x]));
			FPIXEL p;
			p.add(p1, ys);
			p.add(p2, yt);
			dst[x] = p.color(1);
		}
	}
	return newimg;
}

typedef double (*bicubic_lut_t)[4];

static bicubic_lut_t makeBicubicLookupTable(int src, int dst, std::vector<double> *out)
{
	out->resize(dst * 4);
	double (*lut)[4] = (double (*)[4])&(*out)[0];
	for (int x = 0; x < dst; x++) {
		double sx = (double)x * src / dst - 0.5;
		int ix = (int)floor(sx);
		double tx = sx - ix;
		for (int x2 = -1; x2 <= 2; x2++) {
			int x3 = ix + x2;
			if (x3 >= 0 && x3 < src) {
				lut[x][x2 + 1] = bicubic(x2 - tx);
			}
		}
	}
	return lut;
}

template <euclase::Image::Format FORMAT, typename PIXEL, typename FPIXEL>
euclase::Image resizeBicubicT(euclase::Image const &image, int dst_w, int dst_h)
{
	const int src_w = image.width();
	const int src_h = image.height();
	euclase::Image newimg;
	newimg.make(dst_w, dst_h, (euclase::Image::Format)FORMAT);

	std::vector<double> bicubic_lut_x;
	std::vector<double> bicubic_lut_y;
	bicubic_lut_t bicubic_lut_x_p = makeBicubicLookupTable(src_w, dst_w, &bicubic_lut_x);
	bicubic_lut_t bicubic_lut_y_p = makeBicubicLookupTable(src_h, dst_h, &bicubic_lut_y);

#pragma omp parallel for schedule(static, 8)
	for (int y = 0; y < dst_h; y++) {
		PIXEL *dst = (PIXEL *)newimg.scanLine(y);
		double sy = (double)y * src_h / dst_h - 0.5;
		int iy = (int)floor(sy);
		for (int x = 0; x < dst_w; x++) {
			double sx = (double)x * src_w / dst_w - 0.5;
			int ix = (int)floor(sx);
			FPIXEL pixel;
			double amount = 0;
			for (int y2 = -1; y2 <= 2; y2++) {
				int y3 = iy + y2;
				if (y3 >= 0 && y3 < src_h) {
					double vy = bicubic_lut_y_p[y][y2 + 1];
					PIXEL const *src = (PIXEL const *)image.scanLine(y3);
					for (int x2 = -1; x2 <= 2; x2++) {
						int x3 = ix + x2;
						if (x3 >= 0 && x3 < src_w) {
							double vx = bicubic_lut_x_p[x][x2 + 1];
							FPIXEL p(euclase::pixel_cast<FPIXEL>(src[x3]));
							double v = vx * vy;
							pixel.add(p, v);
							amount += v;
						}
					}
				}
			}
			dst[x] = pixel.color(amount);
		}
	}
	return newimg;
}

template <euclase::Image::Format FORMAT, typename PIXEL, typename FPIXEL>
euclase::Image resizeBicubicHT(euclase::Image const &image, int dst_w)
{
	const int src_w = image.width();
	const int src_h = image.height();
	euclase::Image newimg(dst_w, src_h, (euclase::Image::Format)FORMAT);

	std::vector<double> bicubic_lut_x;
	bicubic_lut_t bicubic_lut_x_p = makeBicubicLookupTable(src_w, dst_w, &bicubic_lut_x);

#pragma omp parallel for schedule(static, 8)
	for (int y = 0; y < src_h; y++) {
		PIXEL *dst = (PIXEL *)newimg.scanLine(y);
		PIXEL const *src = (PIXEL const *)image.scanLine(y);
		for (int x = 0; x < dst_w; x++) {
			FPIXEL pixel;
			double volume = 0;
			double sx = (double)x * src_w / dst_w - 0.5;
			int ix = (int)floor(sx);
			for (int x2 = -1; x2 <= 2; x2++) {
				int x3 = ix + x2;
				if (x3 >= 0 && x3 < src_w) {
					double v = bicubic_lut_x_p[x][x2 + 1];
					FPIXEL p(euclase::pixel_cast<FPIXEL>(src[x3]));
					pixel.add(p, v);
					volume += v;
				}
			}
			dst[x] = pixel.color(volume);
		}
	}
	return newimg;
}

template <euclase::Image::Format FORMAT, typename PIXEL, typename FPIXEL>
euclase::Image resizeBicubicVT(euclase::Image const &image, int dst_h)
{
	const int src_w = image.width();
	const int src_h = image.height();
	euclase::Image newimg(src_w, dst_h, FORMAT);

	std::vector<double> bicubic_lut_y;
	bicubic_lut_t bicubic_lut_y_p = makeBicubicLookupTable(src_h, dst_h, &bicubic_lut_y);

#pragma omp parallel for schedule(static, 8)
	for (int x = 0; x < src_w; x++) {
		for (int y = 0; y < dst_h; y++) {
			PIXEL *dst = (PIXEL *)newimg.scanLine(y) + x;
			FPIXEL pixel;
			double volume = 0;
			double sy = (double)y * src_h / dst_h - 0.5;
			int iy = (int)floor(sy);
			for (int y2 = -1; y2 <= 2; y2++) {
				int y3 = iy + y2;
				if (y3 >= 0 && y3 < src_h) {
					double v = bicubic_lut_y_p[y][y2 + 1];
					FPIXEL p(euclase::pixel_cast<FPIXEL>(((PIXEL const *)image.scanLine(y3))[x]));
					pixel.add(p, v);
					volume += v;
				}
			}
			*dst = pixel.color(volume);
		}
	}
	return newimg;
}

//

template <typename PIXEL, typename FPIXEL> euclase::Image BlurFilter(euclase::Image const &image, int radius, bool *cancel, std::function<void (float)> &progress)
{
	auto isInterrupted = [&](){
		return cancel && *cancel;
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

		std::vector<PIXEL> buffer(w * h);

		std::atomic_int rows = 0;

#pragma omp parallel for schedule(static, 8)
		for (int y = 0; y < h; y++) {
			if (isInterrupted()) continue;

			FPIXEL pixel;
			float amount = 0;

			for (int i = 0; i < radius * 2 + 1; i++) {
				int y2 = y + i - radius;
				if (y2 >= 0 && y2 < h) {
					PIXEL const *s = (PIXEL const *)image.scanLine(y2);
					for (int x = 0; x < shape[i]; x++) {
						if (x < w) {
							PIXEL pix(s[x]);
							pixel.add(euclase::pixel_cast<FPIXEL>(pix), 1);
							amount++;
						}
					}
				}
			}
			for (int x = 0; x < w; x++) {
				for (int i = 0; i < radius * 2 + 1; i++) {
					int y2 = y + i - radius;
					if (y2 >= 0 && y2 < h) {
						PIXEL const *s = (PIXEL const *)image.scanLine(y2);
						int x2 = x + shape[i];
						if (x2 < w) {
							PIXEL pix(s[x2]);
							pixel.add(euclase::pixel_cast<FPIXEL>(pix), 1);
							amount++;
						}
					}
				}

				{
					buffer[y * w + x] = pixel.color(amount);
				}

				for (int i = 0; i < radius * 2 + 1; i++) {
					int y2 = y + i - radius;
					if (y2 >= 0 && y2 < h) {
						PIXEL const *s = (PIXEL const *)image.scanLine(y2);
						int x2 = x - shape[i];
						if (x2 >= 0) {
							PIXEL pix(s[x2]);
							pixel.sub(euclase::pixel_cast<FPIXEL>(pix), 1);
							amount--;
						}
					}
				}
			}

			progress((float)++rows / h);
		}

		if (isInterrupted()) return {};

		for (int y = 0; y < h; y++) {
			PIXEL *s = &buffer[y * w];
			PIXEL *d = (PIXEL *)newimage.scanLine(y);
			memcpy(d, s, sizeof(PIXEL) * w);
		}
	}
	return newimage;
}

}

static euclase::Image resizeColorImage(euclase::Image const &image, int dst_w, int dst_h, EnlargeMethod method, bool alphachannel)
{
	euclase::Image newimage;
	int w, h;
	w = image.width();
	h = image.height();
	if (w > 0 && h > 0 && dst_w > 0 && dst_h > 0) {
		newimage = image;
		if (w != dst_w || h != dst_h) {
			if (dst_w < w || dst_h < h) {
				if (method == EnlargeMethod::NearestNeighbor) {
					newimage = resizeNearestNeighbor<euclase::Image::Format_F_RGBA, FloatRGBA>(newimage, dst_w, dst_h);
				} else {
					if (dst_w < w && dst_h < h) {
						if (alphachannel) {
							newimage = resizeAveragingT<euclase::Image::Format_F_RGBA, FloatRGBA, FloatRGBA>(image, dst_w, dst_h);
						} else {
							newimage = resizeAveragingT<euclase::Image::Format_F_RGBA, FloatRGB, FloatRGB>(image, dst_w, dst_h);
						}
					} else if (dst_w < w) {
						if (alphachannel) {
							newimage = resizeAveragingHT<euclase::Image::Format_F_RGBA, FloatRGBA, FloatRGBA>(image, dst_w);
						} else {
							newimage = resizeAveragingHT<euclase::Image::Format_F_RGBA, FloatRGB, FloatRGB>(image, dst_w);
						}
					} else if (dst_h < h) {
						if (alphachannel) {
							newimage = resizeAveragingVT<euclase::Image::Format_F_RGBA, FloatRGBA, FloatRGBA>(image, dst_h);
						} else {
							newimage = resizeAveragingVT<euclase::Image::Format_F_RGBA, FloatRGB, FloatRGB>(image, dst_h);
						}
					}
				}
			}
			w = newimage.width();
			h = newimage.height();
			if (dst_w > w || dst_h > h) {
				if (method == EnlargeMethod::Bilinear) {
					if (dst_w > w && dst_h > h) {
						if (alphachannel) {
							newimage = resizeBilinearT<euclase::Image::Format_F_RGBA, FloatRGBA, FloatRGBA>(newimage, dst_w, dst_h);
						} else {
							newimage = resizeBilinearT<euclase::Image::Format_F_RGBA, FloatRGB, FloatRGB>(newimage, dst_w, dst_h);
						}
					} else if (dst_w > w) {
						if (alphachannel) {
							newimage = resizeBilinearHT<euclase::Image::Format_F_RGBA, FloatRGBA, FloatRGBA>(newimage, dst_w);
						} else {
							newimage = resizeBilinearHT<euclase::Image::Format_F_RGBA, FloatRGB, FloatRGB>(newimage, dst_w);
						}
					} else if (dst_h > h) {
						if (alphachannel) {
							newimage = resizeBilinearVT<euclase::Image::Format_F_RGBA, FloatRGBA, FloatRGBA>(newimage, dst_h);
						} else {
							newimage = resizeBilinearVT<euclase::Image::Format_F_RGBA, FloatRGB, FloatRGB>(newimage, dst_h);
						}
					}
				} else if (method == EnlargeMethod::Bicubic) {
					if (dst_w > w && dst_h > h) {
						if (alphachannel) {
							newimage = resizeBicubicT<euclase::Image::Format_F_RGBA, FloatRGBA, FloatRGBA>(newimage, dst_w, dst_h);
						} else {
							newimage = resizeBicubicT<euclase::Image::Format_F_RGBA, FloatRGB, FloatRGB>(newimage, dst_w, dst_h);
						}
					} else if (dst_w > w) {
						if (alphachannel) {
							newimage = resizeBicubicHT<euclase::Image::Format_F_RGBA, FloatRGBA, FloatRGBA>(newimage, dst_w);
						} else {
							newimage = resizeBicubicHT<euclase::Image::Format_F_RGBA, FloatRGB, FloatRGB>(newimage, dst_w);
						}
					} else if (dst_h > h) {
						if (alphachannel) {
							newimage = resizeBicubicVT<euclase::Image::Format_F_RGBA, FloatRGBA, FloatRGBA>(newimage, dst_h);
						} else {
							newimage = resizeBicubicVT<euclase::Image::Format_F_RGBA, FloatRGB, FloatRGB>(newimage, dst_h);
						}
					}
				} else {
					newimage = resizeNearestNeighbor<euclase::Image::Format_F_RGBA, FloatRGBA>(newimage, dst_w, dst_h);
				}
			}
		}
	}
	return newimage;
}

static euclase::Image resizeGrayscaleImage(euclase::Image const &image, int dst_w, int dst_h, EnlargeMethod method, bool alphachannel)
{
	euclase::Image newimage;
	int w, h;
	w = image.width();
	h = image.height();
	if (w > 0 && h > 0 && dst_w > 0 && dst_h > 0) {
		if (w != dst_w || h != dst_h) {
			newimage = image;
			if (dst_w < w || dst_h < h) {
				if (method == EnlargeMethod::NearestNeighbor) {
					newimage = resizeNearestNeighbor<euclase::Image::Format_F_Grayscale, FloatGray>(newimage, dst_w, dst_h);
				} else {
					if (dst_w < w && dst_h < h) {
						if (alphachannel) {
							newimage = resizeAveragingT<euclase::Image::Format_F_Grayscale, FloatGrayA, FloatGrayA>(newimage, dst_w, dst_h);
						} else {
							newimage = resizeAveragingT<euclase::Image::Format_F_Grayscale, FloatGray, FloatGray>(newimage, dst_w, dst_h);
						}
					} else if (dst_w < w) {
						if (alphachannel) {
							newimage = resizeAveragingHT<euclase::Image::Format_F_Grayscale, FloatGrayA, FloatGrayA>(newimage, dst_w);
						} else {
							newimage = resizeAveragingHT<euclase::Image::Format_F_Grayscale, FloatGray, FloatGray>(newimage, dst_w);
						}
					} else if (dst_h < h) {
						if (alphachannel) {
							newimage = resizeAveragingVT<euclase::Image::Format_F_Grayscale, FloatGrayA, FloatGrayA>(newimage, dst_h);
						} else {
							newimage = resizeAveragingVT<euclase::Image::Format_F_Grayscale, FloatGray, FloatGray>(newimage, dst_h);
						}
					}
				}
			}
			w = newimage.width();
			h = newimage.height();
			if (dst_w > w || dst_h > h) {
				if (method == EnlargeMethod::Bilinear) {
					if (dst_w > w && dst_h > h) {
						if (alphachannel) {
							newimage = resizeBilinearT<euclase::Image::Format_F_Grayscale, FloatGrayA, FloatGrayA>(newimage, dst_w, dst_h);
						} else {
							newimage = resizeBilinearT<euclase::Image::Format_F_Grayscale, FloatGray, FloatGray>(newimage, dst_w, dst_h);
						}
					} else if (dst_w > w) {
						if (alphachannel) {
							newimage = resizeBilinearHT<euclase::Image::Format_F_Grayscale, FloatGrayA, FloatGrayA>(newimage, dst_w);
						} else {
							newimage = resizeBilinearHT<euclase::Image::Format_F_Grayscale, FloatGray, FloatGray>(newimage, dst_w);
						}
					} else if (dst_h > h) {
						if (alphachannel) {
							newimage = resizeBilinearVT<euclase::Image::Format_F_Grayscale, FloatGrayA, FloatGrayA>(newimage, dst_h);
						} else {
							newimage = resizeBilinearVT<euclase::Image::Format_F_Grayscale, FloatGray, FloatGray>(newimage, dst_h);
						}
					}
				} else if (method == EnlargeMethod::Bicubic) {
					if (dst_w > w && dst_h > h) {
						if (alphachannel) {
							newimage = resizeBicubicT<euclase::Image::Format_F_Grayscale, FloatGrayA, FloatGrayA>(newimage, dst_w, dst_h);
						} else {
							newimage = resizeBicubicT<euclase::Image::Format_F_Grayscale, FloatGray, FloatGray>(newimage, dst_w, dst_h);
						}
					} else if (dst_w > w) {
						if (alphachannel) {
							newimage = resizeBicubicHT<euclase::Image::Format_F_Grayscale, FloatGrayA, FloatGrayA>(newimage, dst_w);
						} else {
							newimage = resizeBicubicHT<euclase::Image::Format_F_Grayscale, FloatGray, FloatGray>(newimage, dst_w);
						}
					} else if (dst_h > h) {
						if (alphachannel) {
							newimage = resizeBicubicVT<euclase::Image::Format_F_Grayscale, FloatGrayA, FloatGrayA>(newimage, dst_h);
						} else {
							newimage = resizeBicubicVT<euclase::Image::Format_F_Grayscale, FloatGray, FloatGray>(newimage, dst_h);
						}
					}
				} else {
					newimage = resizeNearestNeighbor<euclase::Image::Format_F_Grayscale, FloatGray>(newimage, dst_w, dst_h);
				}
			}
		}
	}
	return newimage;
}

euclase::Image resizeImage(euclase::Image const &image, int dst_w, int dst_h, EnlargeMethod method)
{
	if (image.width() == dst_w && image.height() == dst_h) return image;

	auto memtype = image.memtype();
	if (memtype != euclase::Image::Host) {
		auto newimg = resizeImage(image.toHost(), dst_w, dst_h, method);
		return newimg.memconvert(memtype);
	}

	bool alphachannel = false;
	switch (image.format()) {
	case euclase::Image::Format_8_RGBA:
	case euclase::Image::Format_F_RGBA:
	case euclase::Image::Format_8_GrayscaleA:
	case euclase::Image::Format_F_GrayscaleA:
		alphachannel = true;
		break;
	}

	switch (image.format()) {
	case euclase::Image::Format_8_Grayscale:
		return resizeNearestNeighbor<euclase::Image::Format_8_Grayscale, euclase::OctetGray>(image, dst_w, dst_h);
	case euclase::Image::Format_8_GrayscaleA:
		return resizeNearestNeighbor<euclase::Image::Format_8_GrayscaleA, euclase::OctetGrayA>(image, dst_w, dst_h);
	case euclase::Image::Format_F_RGB:
	case euclase::Image::Format_F_RGBA:
		return resizeColorImage(image, dst_w, dst_h, method, alphachannel);
	case euclase::Image::Format_F_Grayscale:
	case euclase::Image::Format_F_GrayscaleA:
		return resizeGrayscaleImage(image, dst_w, dst_h, method, alphachannel);
	}
	Q_ASSERT(0);
	return {};
}

euclase::Image filter_blur(euclase::Image image, int radius, bool *cancel, std::function<void (float)> progress)
{
	if (image.format() == euclase::Image::Format_8_Grayscale) {
		return BlurFilter<FloatGrayA, FloatGrayA>(image, radius, cancel, progress);
	}
	return BlurFilter<FloatRGBA, FloatRGBA>(image, radius, cancel, progress);
}
