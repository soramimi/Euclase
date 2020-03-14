#include "resize.h"
#include <QImage>
#include <math.h>
#include <stdint.h>
#include "euclase.h"
#include <omp.h>

using PixelRGBA = euclase::PixelRGBA;
using PixelGrayA = euclase::PixelGrayA;
using FPixelRGB = euclase::FPixelRGB;
using FPixelGray = euclase::FPixelGray;
using FPixelRGBA = euclase::FPixelRGBA;
using FPixelGrayA = euclase::FPixelGrayA;

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

QImage resizeNearestNeighbor(QImage const &image, int dst_w, int dst_h)
{
	const int src_w = image.width();
	const int src_h = image.height();
	QImage newimg(dst_w, dst_h, QImage::Format_RGBA8888);
#pragma omp parallel for
	for (int y = 0; y < dst_h; y++) {
		double fy = (double)y * src_h / dst_h;
		PixelRGBA *dst = (PixelRGBA *)newimg.scanLine(y);
		PixelRGBA const *src = (PixelRGBA const *)image.scanLine((int)fy);
		double mul = (double)src_w / dst_w;
		for (int x = 0; x < dst_w; x++) {
			double fx = (double)x * mul;
			dst[x] = src[(int)fx];
		}
	}
	return std::move(newimg);
}

template <typename PIXEL>
QImage resizeAveragingT(QImage const &image, int dst_w, int dst_h, bool gamma_correction)
{
//	bool gamma_correction = true;
	const int src_w = image.width();
	const int src_h = image.height();
	QImage newimg(dst_w, dst_h, QImage::Format_RGBA8888);
#pragma omp parallel for
	for (int y = 0; y < dst_h; y++) {
		double lo_y = (double)y * src_h / dst_h;
		double hi_y = (double)(y + 1) * src_h / dst_h;
		PixelRGBA *dst = (PixelRGBA *)newimg.scanLine(y);
		double mul = (double)src_w / dst_w;
		for (int x = 0; x < dst_w; x++) {
			double lo_x = (double)x * mul;
			double hi_x = (double)(x + 1) * mul;
			int lo_iy = (int)lo_y;
			int hi_iy = (int)hi_y;
			int lo_ix = (int)lo_x;
			int hi_ix = (int)hi_x;
			PIXEL pixel;
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
				PixelRGBA const *src = (PixelRGBA const *)image.scanLine(sy < src_h ? sy : (src_h - 1));
				for (int sx = lo_ix; sx <= hi_ix; sx++) {
					PIXEL p = PIXEL::convert(src[sx < src_w ? sx : (src_w - 1)], gamma_correction);
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
			dst[x] = pixel.color(volume, gamma_correction);
		}
	}
	return std::move(newimg);
}

template <typename PIXEL>
QImage resizeAveragingHT(QImage const &image, int dst_w, bool gamma_correction)
{
//	bool gamma_correction = true;
	const int src_w = image.width();
	const int src_h = image.height();
	QImage newimg(dst_w, src_h, QImage::Format_RGBA8888);
#pragma omp parallel for
	for (int y = 0; y < src_h; y++) {
		PixelRGBA *dst = (PixelRGBA *)newimg.scanLine(y);
		for (int x = 0; x < dst_w; x++) {
			double lo_x = (double)x * src_w / dst_w;
			double hi_x = (double)(x + 1) * src_w / dst_w;
			int lo_ix = (int)lo_x;
			int hi_ix = (int)hi_x;
			PIXEL pixel;
			double volume = 0;
			PixelRGBA const *src = (PixelRGBA const *)image.scanLine(y < src_h ? y : (src_h - 1));
			for (int sx = lo_ix; sx <= hi_ix; sx++) {
				PIXEL p = PIXEL::convert(src[sx < src_w ? sx : (src_w - 1)], gamma_correction);
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
			dst[x] = pixel.color(volume, gamma_correction);
		}
	}
	return std::move(newimg);
}

template <typename PIXEL>
QImage resizeAveragingVT(QImage const &image, int dst_h, bool gamma_correction)
{
//	bool gamma_correction = true;
	const int src_w = image.width();
	const int src_h = image.height();
	QImage newimg(src_w, dst_h, QImage::Format_RGBA8888);
#pragma omp parallel for
	for (int y = 0; y < dst_h; y++) {
		double lo_y = (double)y * src_h / dst_h;
		double hi_y = (double)(y + 1) * src_h / dst_h;
		PixelRGBA *dst = (PixelRGBA *)newimg.scanLine(y);
		for (int x = 0; x < src_w; x++) {
			int lo_iy = (int)lo_y;
			int hi_iy = (int)hi_y;
			PIXEL pixel;
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
				PixelRGBA const *src = (PixelRGBA const *)image.scanLine(sy < src_h ? sy : (src_h - 1));
				PIXEL p = PIXEL::convert(src[x], gamma_correction);
				pixel.add(p, v);
				volume += v;
			}
			dst[x] = pixel.color(volume, gamma_correction);
		}
	}
	return std::move(newimg);
}

struct bilinear_t {
	int i0, i1;
	double v0, v1;
};

template <typename PIXEL>
QImage resizeBilinearT(QImage const &image, int dst_w, int dst_h, bool gamma_correction)
{
//	bool gamma_correction = true;
	const int src_w = image.width();
	const int src_h = image.height();
	QImage newimg(dst_w, dst_h, QImage::Format_RGBA8888);

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

#pragma omp parallel for
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
		PixelRGBA *dst = (PixelRGBA *)newimg.scanLine(y);
		PixelRGBA const *src1 = (PixelRGBA const *)image.scanLine(y0);
		PixelRGBA const *src2 = (PixelRGBA const *)image.scanLine(y1);
		for (int x = 0; x < dst_w; x++) {
			double a11 = lut_p[x].v0 * ys;
			double a12 = lut_p[x].v1 * ys;
			double a21 = lut_p[x].v0 * yt;
			double a22 = lut_p[x].v1 * yt;
			PIXEL pixel;
			pixel.add(PIXEL::convert(src1[lut_p[x].i0], gamma_correction), a11);
			pixel.add(PIXEL::convert(src1[lut_p[x].i1], gamma_correction), a12);
			pixel.add(PIXEL::convert(src2[lut_p[x].i0], gamma_correction), a21);
			pixel.add(PIXEL::convert(src2[lut_p[x].i1], gamma_correction), a22);
			dst[x] = pixel.color(a11 + a12 + a21 + a22, gamma_correction);
		}
	}
	return std::move(newimg);
}

template <typename PIXEL>
QImage resizeBilinearHT(QImage const &image, int dst_w, bool gamma_correction)
{
//	bool gamma_correction = true;
	const int src_w = image.width();
	const int src_h = image.height();
	QImage newimg(dst_w, src_h, QImage::Format_RGBA8888);
#pragma omp parallel for
	for (int y = 0; y < src_h; y++) {
		PixelRGBA *dst = (PixelRGBA *)newimg.scanLine(y);
		PixelRGBA const *src = (PixelRGBA const *)image.scanLine(y);
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
			PIXEL p1(PIXEL::convert(src[x0], gamma_correction));
			PIXEL p2(PIXEL::convert(src[x1], gamma_correction));
			PIXEL p;
			p.add(p1, xs);
			p.add(p2, xt);
			dst[x] = p.color(1, gamma_correction);
		}
	}
	return std::move(newimg);
}

template <typename PIXEL>
QImage resizeBilinearVT(QImage const &image, int dst_h, bool gamma_correction)
{
//	bool gamma_correction = true;
	const int src_w = image.width();
	const int src_h = image.height();
	QImage newimg(src_w, dst_h, QImage::Format_RGBA8888);
#pragma omp parallel for
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
		PixelRGBA *dst = (PixelRGBA *)newimg.scanLine(y);
		PixelRGBA const *src1 = (PixelRGBA const *)image.scanLine(y0);
		PixelRGBA const *src2 = (PixelRGBA const *)image.scanLine(y1);
		for (int x = 0; x < src_w; x++) {
			PIXEL p1(PIXEL::convert(src1[x], gamma_correction));
			PIXEL p2(PIXEL::convert(src2[x], gamma_correction));
			PIXEL p;
			p.add(p1, ys);
			p.add(p2, yt);
			dst[x] = p.color(1, gamma_correction);
		}
	}
	return std::move(newimg);
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

template <typename PIXEL>
QImage resizeBicubicT(QImage const &image, int dst_w, int dst_h, bool gamma_correction)
{
//	bool gamma_correction = true;
	const int src_w = image.width();
	const int src_h = image.height();
	QImage newimg(dst_w, dst_h, QImage::Format_RGBA8888);

	std::vector<double> bicubic_lut_x;
	std::vector<double> bicubic_lut_y;
	bicubic_lut_t bicubic_lut_x_p = makeBicubicLookupTable(src_w, dst_w, &bicubic_lut_x);
	bicubic_lut_t bicubic_lut_y_p = makeBicubicLookupTable(src_h, dst_h, &bicubic_lut_y);

#pragma omp parallel for
	for (int y = 0; y < dst_h; y++) {
		PixelRGBA *dst = (PixelRGBA *)newimg.scanLine(y);
		double sy = (double)y * src_h / dst_h - 0.5;
		int iy = (int)floor(sy);
		for (int x = 0; x < dst_w; x++) {
			double sx = (double)x * src_w / dst_w - 0.5;
			int ix = (int)floor(sx);
			PIXEL pixel;
			double amount = 0;
			for (int y2 = -1; y2 <= 2; y2++) {
				int y3 = iy + y2;
				if (y3 >= 0 && y3 < src_h) {
					double vy = bicubic_lut_y_p[y][y2 + 1];
					PixelRGBA const *src = (PixelRGBA const *)image.scanLine(y3);
					for (int x2 = -1; x2 <= 2; x2++) {
						int x3 = ix + x2;
						if (x3 >= 0 && x3 < src_w) {
							double vx = bicubic_lut_x_p[x][x2 + 1];
							PIXEL p(PIXEL::convert(src[x3], gamma_correction));
							double v = vx * vy;
							pixel.add(p, v);
							amount += v;
						}
					}
				}
			}
			dst[x] = pixel.color(amount, gamma_correction);
		}
	}
	return std::move(newimg);
}

template <typename PIXEL>
QImage resizeBicubicHT(QImage const &image, int dst_w, bool gamma_correction)
{
//	bool gamma_correction = true;
	const int src_w = image.width();
	const int src_h = image.height();
	QImage newimg(dst_w, src_h, QImage::Format_RGBA8888);

	std::vector<double> bicubic_lut_x;
	bicubic_lut_t bicubic_lut_x_p = makeBicubicLookupTable(src_w, dst_w, &bicubic_lut_x);

#pragma omp parallel for
	for (int y = 0; y < src_h; y++) {
		PixelRGBA *dst = (PixelRGBA *)newimg.scanLine(y);
		PixelRGBA const *src = (PixelRGBA const *)image.scanLine(y);
		for (int x = 0; x < dst_w; x++) {
			PIXEL pixel;
			double volume = 0;
			double sx = (double)x * src_w / dst_w - 0.5;
			int ix = (int)floor(sx);
			for (int x2 = -1; x2 <= 2; x2++) {
				int x3 = ix + x2;
				if (x3 >= 0 && x3 < src_w) {
					double v = bicubic_lut_x_p[x][x2 + 1];
					PIXEL p(PIXEL::convert(src[x3], gamma_correction));
					pixel.add(p, v);
					volume += v;
				}
			}
			dst[x] = pixel.color(volume, gamma_correction);
		}
	}
	return std::move(newimg);
}

template <typename PIXEL>
QImage resizeBicubicVT(QImage const &image, int dst_h, bool gamma_correction)
{
//	bool gamma_correction = true;
	const int src_w = image.width();
	const int src_h = image.height();
	QImage newimg(src_w, dst_h, QImage::Format_RGBA8888);

	std::vector<double> bicubic_lut_y;
	bicubic_lut_t bicubic_lut_y_p = makeBicubicLookupTable(src_h, dst_h, &bicubic_lut_y);

#pragma omp parallel for
	for (int x = 0; x < src_w; x++) {
		for (int y = 0; y < dst_h; y++) {
			PixelRGBA *dst = (PixelRGBA *)newimg.scanLine(y) + x;
			PIXEL pixel;
			double volume = 0;
			double sy = (double)y * src_h / dst_h - 0.5;
			int iy = (int)floor(sy);
			for (int y2 = -1; y2 <= 2; y2++) {
				int y3 = iy + y2;
				if (y3 >= 0 && y3 < src_h) {
					double v = bicubic_lut_y_p[y][y2 + 1];
					PIXEL p(PIXEL::convert(((PixelRGBA const *)image.scanLine(y3))[x], gamma_correction));
					pixel.add(p, v);
					volume += v;
				}
			}
			*dst = pixel.color(volume, gamma_correction);
		}
	}
	return std::move(newimg);
}

//

template <typename PIXEL, typename FPIXEL> QImage BlurFilter(QImage const &image, int radius, bool gamma_correction)
{
//	bool gamma_correction = true;
	int w = image.width();
	int h = image.height();
	QImage newimage(w, h, image.format());
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

#pragma omp parallel for
		for (int y = 0; y < h; y++) {
			FPIXEL pixel;
			for (int i = 0; i < radius * 2 + 1; i++) {
				int y2 = y + i - radius;
				if (y2 >= 0 && y2 < h) {
					PIXEL const *s = (PIXEL const *)image.scanLine(y2);
					for (int x = 0; x < shape[i]; x++) {
						if (x < w) {
							PIXEL pix(s[x]);
							pixel.add(FPIXEL::convert(pix, gamma_correction), 1);
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
							pixel.add(FPIXEL::convert(pix, gamma_correction), 1);
						}
					}
				}

				{
					PIXEL const *s = (PIXEL const *)image.scanLine(y);
					PIXEL pix = s[x];
					pix = pixel.color(1, gamma_correction);
					buffer[y * w + x] = pix;
				}

				for (int i = 0; i < radius * 2 + 1; i++) {
					int y2 = y + i - radius;
					if (y2 >= 0 && y2 < h) {
						PIXEL const *s = (PIXEL const *)image.scanLine(y2);
						int x2 = x - shape[i];
						if (x2 >= 0) {
							PIXEL pix(s[x2]);
							pixel.sub(FPIXEL::convert(pix, gamma_correction), 1);
						}
					}
				}
			}
		}

		for (int y = 0; y < h; y++) {
			PIXEL *s = &buffer[y * w];
			PIXEL *d = (PIXEL *)newimage.scanLine(y);
			memcpy(d, s, sizeof(PIXEL) * w);
		}
	}
	return newimage;
}

}

QImage resizeImage(QImage const &image, int dst_w, int dst_h, EnlargeMethod method, bool alphachannel, bool gamma_correction)
{
	QImage newimage = image;
	if (dst_w > 0 && dst_h > 0) {
		int w, h;
		w = newimage.width();
		h = newimage.height();
		if (w != dst_w || h != dst_h) {
			if (dst_w < w || dst_h < h) {
				if (dst_w < w && dst_h < h) {
					if (alphachannel) {
						newimage = resizeAveragingT<FPixelRGBA>(newimage, dst_w, dst_h, gamma_correction);
					} else {
						newimage = resizeAveragingT<FPixelRGB>(newimage, dst_w, dst_h, gamma_correction);
					}
				} else if (dst_w < w) {
					if (alphachannel) {
						newimage = resizeAveragingHT<FPixelRGBA>(newimage, dst_w, gamma_correction);
					} else {
						newimage = resizeAveragingHT<FPixelRGB>(newimage, dst_w, gamma_correction);
					}
				} else if (dst_h < h) {
					if (alphachannel) {
						newimage = resizeAveragingVT<FPixelRGBA>(newimage, dst_h, gamma_correction);
					} else {
						newimage = resizeAveragingVT<FPixelRGB>(newimage, dst_h, gamma_correction);
					}
				}
			}
			w = newimage.width();
			h = newimage.height();
			if (dst_w > w || dst_h > h) {
				if (method == EnlargeMethod::Bilinear) {
					if (dst_w > w && dst_h > h) {
						if (alphachannel) {
							newimage = resizeBilinearT<FPixelRGBA>(newimage, dst_w, dst_h, gamma_correction);
						} else {
							newimage = resizeBilinearT<FPixelRGB>(newimage, dst_w, dst_h, gamma_correction);
						}
					} else if (dst_w > w) {
						if (alphachannel) {
							newimage = resizeBilinearHT<FPixelRGBA>(newimage, dst_w, gamma_correction);
						} else {
							newimage = resizeBilinearHT<FPixelRGB>(newimage, dst_w, gamma_correction);
						}
					} else if (dst_h > h) {
						if (alphachannel) {
							newimage = resizeBilinearVT<FPixelRGBA>(newimage, dst_h, gamma_correction);
						} else {
							newimage = resizeBilinearVT<FPixelRGB>(newimage, dst_h, gamma_correction);
						}
					}
				} else if (method == EnlargeMethod::Bicubic) {
					if (dst_w > w && dst_h > h) {
						if (alphachannel) {
							newimage = resizeBicubicT<FPixelRGBA>(newimage, dst_w, dst_h, gamma_correction);
						} else {
							newimage = resizeBicubicT<FPixelRGB>(newimage, dst_w, dst_h, gamma_correction);
						}
					} else if (dst_w > w) {
						if (alphachannel) {
							newimage = resizeBicubicHT<FPixelRGBA>(newimage, dst_w, gamma_correction);
						} else {
							newimage = resizeBicubicHT<FPixelRGB>(newimage, dst_w, gamma_correction);
						}
					} else if (dst_h > h) {
						if (alphachannel) {
							newimage = resizeBicubicVT<FPixelRGBA>(newimage, dst_h, gamma_correction);
						} else {
							newimage = resizeBicubicVT<FPixelRGB>(newimage, dst_h, gamma_correction);
						}
					}
				} else {
					newimage = resizeNearestNeighbor(newimage, dst_w, dst_h);
				}
			}
		}
		return std::move(newimage);
	}
	return QImage();
}

QImage filter_blur(QImage image, int radius, bool gamma_correction)
{
	if (image.format() == QImage::Format_Grayscale8) {
		return BlurFilter<PixelGrayA, FPixelGrayA>(image, radius, gamma_correction);
	}
	image = image.convertToFormat(QImage::Format_RGBA8888);
	return BlurFilter<PixelRGBA, FPixelRGBA>(image, radius, gamma_correction);
}
