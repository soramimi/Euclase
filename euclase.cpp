#include "euclase.h"
#include "resize.h"
#include "ApplicationGlobal.h"

#ifndef _WIN32
#include <x86intrin.h>
#endif

double euclase::cubicBezierPoint(double p0, double p1, double p2, double p3, double t)
{
	double u = 1 - t;
	return p0 * u * u * u + p1 * u * u * t * 3 + p2 * u * t * t * 3 + p3 * t * t * t;
}

double euclase::cubicBezierGradient(double p0, double p1, double p2, double p3, double t)
{
	return 0 - p0 * (t * t - t * 2 + 1) + p1 * (t * t * 3 - t * 4 + 1) - p2 * (t * t * 3 - t * 2) + p3 * t * t;
}

static void cubicBezierSplit(double *p0, double *p1, double *p2, double *p3, double *p4, double *p5, double t)
{
	double p = euclase::cubicBezierPoint(*p0, *p1, *p2, *p3, t);
	double q = euclase::cubicBezierGradient(*p0, *p1, *p2, *p3, t);
	double u = 1 - t;
	*p4 = p + q * u;
	*p5 = *p3 + (*p2 - *p3) * u;
	*p1 = *p0 + (*p1 - *p0) * t;
	*p2 = p - q * t;
	*p3 = p;
}

QPointF euclase::cubicBezierPoint(QPointF &p0, QPointF &p1, QPointF &p2, QPointF &p3, double t)
{
	double x = cubicBezierPoint(p0.x(), p1.x(), p2.x(), p3.x(), t);
	double y = cubicBezierPoint(p0.y(), p1.y(), p2.y(), p3.y(), t);
	return QPointF(x, y);
}

void euclase::cubicBezierSplit(QPointF *p0, QPointF *p1, QPointF *p2, QPointF *p3, QPointF *q0, QPointF *q1, QPointF *q2, QPointF *q3, double t)
{
	double p4, p5;
	*q3 = *p3;
	::cubicBezierSplit(&p0->rx(), &p1->rx(), &p2->rx(), &p3->rx(), &p4, &p5, t);
	q1->rx() = p4;
	q2->rx() = p5;
	::cubicBezierSplit(&p0->ry(), &p1->ry(), &p2->ry(), &p3->ry(), &p4, &p5, t);
	q1->ry() = p4;
	q2->ry() = p5;
	*q0 = *p3;
}

// Image

euclase::Image::Image(int width, int height, Format format, MemoryType memtype)
{
	init(width, height, format, memtype);
}

void euclase::Image::assign(Data *p)
{
	if (p) {
		p->ref++;
	}
	if (ptr_) {
		if (ptr_->ref > 1) {
			ptr_->ref--;
		} else {
			ptr_->~Data();
			if (ptr_->memtype_ == CUDA && ptr_->cudamem_) {
				global->cuda->free(ptr_->cudamem_);
			}
			free(ptr_);
		}
	}
	ptr_ = p;
}

void euclase::Image::fill(const QColor &color)
{
	int w = width();
	int h = height();
	switch (format()) {
	case Image::Format_8_RGB:
		for (int y = 0; y < h; y++) {
			uint8_t *p = scanLine(y);
			for (int x = 0; x < w; x++) {
				p[x * 3 + 0] = color.red();
				p[x * 3 + 1] = color.green();
				p[x * 3 + 2] = color.blue();
			}
		}
		return;
	case Image::Format_8_RGBA:
		if (memtype() == CUDA) {
			uint8_t r = color.red();
			uint8_t g = color.green();
			uint8_t b = color.blue();
			uint8_t a = color.alpha();
			global->cuda->fill_uint8_rgba(w, h, r, g, b, a, data(), width(), height(), 0, 0);
			return;
		}
		if (memtype() == Host) {
			if (color.alpha() == 0) {
				for (int y = 0; y < h; y++) {
					uint8_t *p = scanLine(y);
					memset(p, 0, w * 4);
				}
			} else {
				for (int y = 0; y < h; y++) {
					uint8_t *p = scanLine(y);
					for (int x = 0; x < w; x++) {
						p[x * 4 + 0] = color.red();
						p[x * 4 + 1] = color.green();
						p[x * 4 + 2] = color.blue();
						p[x * 4 + 3] = color.alpha();
					}
				}
			}
			return;
		}
		break;
	case Image::Format_8_Grayscale:
		{
			uint8_t k = euclase::gray(color.red(), color.green(), color.blue());
			if (memtype() == CUDA) {
				global->cuda->memset(data(), k, w * h);
				return;
			}
			if (memtype() == Host) {
				for (int y = 0; y < h; y++) {
					uint8_t *p = scanLine(y);
					for (int x = 0; x < w; x++) {
						p[x] = k;
					}
				}
				return;
			}
		}
		break;
	case Image::Format_F_RGBA:
		{
			euclase::OctetRGBA icolor(color.red(), color.green(), color.blue(), color.alpha());
			euclase::FloatRGBA fcolor = euclase::FloatRGBA::convert(icolor);
			if (memtype() == CUDA) {
				global->cuda->fill_float_rgba(w, h, fcolor.r, fcolor.g, fcolor.b, fcolor.a, data(), width(), height(), 0, 0);
				return;
			}
			if (memtype() == Host) {
				for (int y = 0; y < h; y++) {
					euclase::FloatRGBA *p = (euclase::FloatRGBA *)scanLine(y);
					for (int x = 0; x < w; x++) {
						p[x] = fcolor;
					}
				}
				return;
			}
		}
		break;
	}
	if (memtype() == CUDA) {
		auto img = toHost();
		img.fill(color);
		img = img.toCUDA();
		assign(img.ptr_);
	}
}

void euclase::Image::setImage(const QImage &image)
{
	Image::Format f = Image::Format_Invalid;
	int w = image.width();
	int h = image.height();

	QImage srcimage;

	switch (image.format()) {
	case QImage::Format_Grayscale8:
	case QImage::Format_Grayscale16:
		srcimage = image.convertToFormat(QImage::Format_Grayscale8);
		f = Image::Format_8_Grayscale;
		break;
	case QImage::Format_RGB888:
	case QImage::Format_RGB32:
	case QImage::Format_RGBA8888:
	case QImage::Format_ARGB32:
		srcimage = image.convertToFormat(QImage::Format_RGBA8888);
		f = Image::Format_8_RGBA;
		break;
	}

	make(w, h, f);

	if (srcimage.format() == QImage::Format_RGBA8888 && format() == Image::Format_8_RGBA) {
		for (int y = 0; y < h; y++) {
			uint8_t const *s = srcimage.scanLine(y);
			uint8_t *d = scanLine(y);
			memcpy(d, s, w * 4);
		}
	} else if (srcimage.format() == QImage::Format_RGB888 && format() == Image::Format_8_RGB) {
		for (int y = 0; y < h; y++) {
			uint8_t const *s = srcimage.scanLine(y);
			uint8_t *d = scanLine(y);
			memcpy(d, s, w * 3);
		}
	} else if (srcimage.format() == QImage::Format_Grayscale8 && format() == Image::Format_8_Grayscale) {
		for (int y = 0; y < h; y++) {
			uint8_t const *s = srcimage.scanLine(y);
			uint8_t *d = scanLine(y);
			memcpy(d, s, w);
		}
	}
}

euclase::Image euclase::Image::convert(Image::Format newformat) const
{
	if (format() == newformat) {
		return *this;
	}

	if (memtype() == CUDA) {
		return toHost().convert(newformat).toCUDA();
	}

	int w = width();
	int h = height();
	euclase::Image newimg;
	if (newformat == Image::Format_8_RGBA) {
		switch (format()) {
		case Format_F_RGBA:
			newimg.make(w, h, newformat);
			for (int y = 0; y < h; y++) {
				euclase::FloatRGBA const *src = (euclase::FloatRGBA const *)scanLine(y);
				euclase::OctetRGBA *dst = (euclase::OctetRGBA *)newimg.scanLine(y);
				for (int x = 0; x < w; x++) {
					dst[x] = euclase::OctetRGBA::convert(src[x].limit());
				}
			}
			break;
		}
	} else if (newformat == Image::Format_F_RGBA) {
		switch (format()) {
		case Format_8_RGBA:
			newimg.make(w, h, newformat);
			for (int y = 0; y < h; y++) {
				euclase::OctetRGBA const *src = (euclase::OctetRGBA const *)scanLine(y);
				euclase::FloatRGBA *dst = (euclase::FloatRGBA *)newimg.scanLine(y);
				for (int x = 0; x < w; x++) {
					dst[x] = euclase::FloatRGBA::convert(src[x]);
				}
			}
			break;
		}
	} else if (newformat == Image::Format_F_RGB) {
		switch (format()) {
		case Format_8_RGB:
			newimg.make(w, h, newformat);
			for (int y = 0; y < h; y++) {
				uint8_t const *s = scanLine(y);
				FloatRGB *d = (FloatRGB *)newimg.scanLine(y);
				for (int x = 0; x < w; x++) {
					d[x] = FloatRGB::convert(OctetRGBA(s[3 * x + 0], s[3 * x + 1], s[3 * x + 2], 255));
				}
			}
			break;
		}
	} else if (newformat == Image::Format_8_Grayscale) {
		switch (format()) {
		case Format_F_RGBA:
			newimg.make(w, h, newformat);
			for (int y = 0; y < h; y++) {
				FloatRGBA const *s = (FloatRGBA const *)scanLine(y);
				OctetGray *d = (OctetGray *)newimg.scanLine(y);
				for (int x = 0; x < w; x++) {
					d[x] = OctetGray::convert(s[x].limit());
				}
			}
			break;
		case Format_8_RGBA:
			newimg.make(w, h, newformat);
			for (int y = 0; y < h; y++) {
				OctetRGBA const *s = (OctetRGBA const *)scanLine(y);
				OctetGray *d = (OctetGray *)newimg.scanLine(y);
				for (int x = 0; x < w; x++) {
					d[x] = OctetGray::convert(s[x]);
				}
			}
			break;
		}
	} else if (newformat == Image::Format_F_Grayscale) {
		switch (format()) {
		case Format_8_Grayscale:
			newimg.make(w, h, newformat);
			for (int y = 0; y < h; y++) {
				FloatRGBA const *s = (FloatRGBA const *)scanLine(y);
				FloatGray *d = (FloatGray *)newimg.scanLine(y);
				for (int x = 0; x < w; x++) {
					d[x] = FloatGray::convert(s[x]);
				}
			}
			break;
		case Format_F_RGBA:
			newimg.make(w, h, newformat);
			for (int y = 0; y < h; y++) {
				FloatRGBA const *s = (FloatRGBA const *)scanLine(y);
				FloatGray *d = (FloatGray *)newimg.scanLine(y);
				for (int x = 0; x < w; x++) {
					d[x] = FloatGray::convert(s[x]);
				}
			}
			break;
		case Format_8_RGBA:
			newimg.make(w, h, newformat);
			for (int y = 0; y < h; y++) {
				OctetRGBA const *s = (OctetRGBA const *)scanLine(y);
				FloatGray *d = (FloatGray *)newimg.scanLine(y);
				for (int x = 0; x < w; x++) {
					d[x] = FloatGray::convert(s[x]);
				}
			}
			break;
		}
	} else if (newformat == Image::Format_F_GrayscaleA) {
		switch (format()) {
		case Format_8_GrayscaleA:
			newimg.make(w, h, newformat);
			for (int y = 0; y < h; y++) {
				OctetGrayA const *s = (OctetGrayA const *)scanLine(y);
				FloatGrayA *d = (FloatGrayA *)newimg.scanLine(y);
				for (int x = 0; x < w; x++) {
					d[x] = FloatGrayA::convert(s[x]);
				}
			}
			break;
		}
	}
	return newimg;
}

euclase::Image euclase::Image::makeFPImage() const
{
	switch (format()) {
	case euclase::Image::Format_F_RGB:
	case euclase::Image::Format_F_RGBA:
	case euclase::Image::Format_F_Grayscale:
	case euclase::Image::Format_F_GrayscaleA:
		return *this;
	case Format_8_RGB:
		return convert(Format_F_RGB);
	case Format_8_RGBA:
		return convert(Format_F_RGBA);
	case Format_8_Grayscale:
		return convert(Format_F_Grayscale);
	case Format_8_GrayscaleA:
		return convert(Format_F_GrayscaleA);
	}
	return {};
}

void euclase::Image::swap(Image &other)
{
	std::swap(ptr_, other.ptr_);
}

QImage euclase::Image::qimage() const
{
	int w = width();
	int h = height();
	if (format() == Image::Format_8_RGB) {
		QImage newimage(w, h, QImage::Format_RGB888);
		if (memtype() == Host) {
			for (int y = 0; y < h; y++) {
				uint8_t const *s = scanLine(y);
				uint8_t *d = newimage.scanLine(y);
				memcpy(d, s, 3 * w);
			}
			return newimage;
		}
	}
	if (format() == Image::Format_F_RGBA) {
		QImage newimage(w, h, QImage::Format_RGBA8888);
		if (memtype() == CUDA) {
			euclase::Image tmpimg(w, h, Format_8_RGBA, CUDA);
			global->cuda->copy_float_to_uint8_rgba(w, h, data(), w, h, 0, 0, tmpimg.data(), w, h, 0, 0);
			return tmpimg.qimage();
		}
		if (memtype() == Host) {
			for (int y = 0; y < h; y++) {
				euclase::FloatRGBA const *s = (euclase::FloatRGBA const *)scanLine(y);
				uint8_t *d = newimage.scanLine(y);
				for (int x = 0; x < w; x++) {
					auto t = euclase::gamma(s[x]).limit();
					d[4 * x + 0] = t.r8();
					d[4 * x + 1] = t.g8();
					d[4 * x + 2] = t.b8();
					d[4 * x + 3] = t.a8();
				}
			}
			return newimage;
		}
	}
	if (format() == Image::Format_8_RGBA) {
		QImage newimage(w, h, QImage::Format_RGBA8888);
		if (memtype() == CUDA) {
			global->cuda->copy_uint8_rgba(w, h, data(), w, h, 0, 0, newimage.bits(), w, h, 0, 0);
			return newimage;
		}
		if (memtype() == Host) {
			for (int y = 0; y < h; y++) {
				uint8_t const *s = scanLine(y);
				uint8_t *d = newimage.scanLine(y);
				memcpy(d, s, 4 * w);
			}
			return newimage;
		}
	}
	if (format() == Image::Format_8_Grayscale) {
		QImage newimage(w, h, QImage::Format_Grayscale8);
		if (memtype() == CUDA) {
			for (int y = 0; y < h; y++) {
				global->cuda->memcpy_dtoh(newimage.scanLine(y), scanLine(y), w);
			}
			return newimage;
		}
		if (memtype() == Host) {
			for (int y = 0; y < h; y++) {
				uint8_t const *s = scanLine(y);
				uint8_t *d = newimage.scanLine(y);
				memcpy(d, s, w);
			}
			return newimage;
		}
	}
	return {};
}

euclase::Image euclase::Image::scaled(int w, int h, bool smooth) const
{
#if 1
	return resizeImage(*this, w, h, smooth ? EnlargeMethod::Bicubic : EnlargeMethod::NearestNeighbor);
#else
	Image newimage;
	QImage img = image_.scaled(w, h);
	newimage.setImage(img);
	return std::move(newimage);
#endif
}

QSize euclase::Image::size() const
{
	return {width(), height()};
}

QImage::Format euclase::qimageformat(Image::Format format)
{
	switch (format) {
	case euclase::Image::Format_8_RGB:
		return QImage::Format_RGB888;
	case euclase::Image::Format_8_RGBA:
		return QImage::Format_RGBA8888;
	case euclase::Image::Format_8_Grayscale:
		return QImage::Format_Grayscale8;
	}
	return QImage::Format_Invalid;
}

int euclase::bytesPerPixel(Image::Format format)
{
	switch (format) {
	case Image::Format_8_RGB:
		return 3;
	case Image::Format_8_RGBA:
		return 4;
	case Image::Format_8_Grayscale:
		return 1;
	case Image::Format_8_GrayscaleA:
		return 2;
	case Image::Format_F_RGB:
		return sizeof(float) * 3;
	case Image::Format_F_RGBA:
		return sizeof(float) * 4;
	case Image::Format_F_Grayscale:
		return sizeof(float);
	case Image::Format_F_GrayscaleA:
		return sizeof(float) * 2;
	}
	return 0;
}

void euclase::Image::init(int w, int h, Image::Format format, MemoryType memtype)
{
	const int datasize = w * h * bytesPerPixel(format);
	Data *p = (Data *)malloc(sizeof(Data) + (memtype == Host ? datasize : 0));
	Q_ASSERT(p);
	new(p) Data();
	assign(p);
	p->memtype_ = memtype;
	ptr_->format_ = format;
	ptr_->width_ = w;
	ptr_->height_ = h;
	if (memtype == CUDA) {
		Q_ASSERT(global && global->cuda);
		ptr_->cudamem_ = global->cuda->malloc(datasize);
	}
}

euclase::Image euclase::Image::copy(MemoryType memtype) const
{
	const auto src_memtype = this->memtype();
	const auto dst_memtype = (memtype == (MemoryType)-1) ? src_memtype : memtype;
	int w = width();
	int h = height();
	auto f = format();
	Image newimg(w, h, f, dst_memtype);
	if (ptr_) {
		const int datasize = w * h * bytesPerPixel(f);
		switch (dst_memtype) {
		case Host:
			switch (src_memtype) {
			case Host:
				memcpy(newimg.ptr_->data(), ptr_->data(), datasize);
				break;
			case CUDA:
				global->cuda->memcpy_dtoh(newimg.ptr_->data(), ptr_->data(), datasize);
				break;
			}
			break;
		case CUDA:
			switch (src_memtype) {
			case Host:
				global->cuda->memcpy_htod(newimg.ptr_->data(), ptr_->data(), datasize);
				break;
			case CUDA:
				global->cuda->memcpy_dtod(newimg.ptr_->data(), ptr_->data(), datasize);
				break;
			}
			break;
		}
	}
	return newimg;
}

euclase::Image &euclase::Image::memconvert(MemoryType memtype)
{
	if (ptr_->memtype_ != memtype) {
		Image newimg = copy(memtype);
		assign(newimg.ptr_);
	}
	return *this;
}

euclase::FloatHSV euclase::rgb_to_hsv(const FloatRGB &rgb)
{
	FloatHSV hsv;
	float max = rgb.r > rgb.g ? rgb.r : rgb.g;
	max = max > rgb.b ? max : rgb.b;
	float min = rgb.r < rgb.g ? rgb.r : rgb.g;
	min = min < rgb.b ? min : rgb.b;
	hsv.h = max - min;
	if (hsv.h > 0.0f) {
		if (max == rgb.r) {
			hsv.h = (rgb.g - rgb.b) / hsv.h;
			if (hsv.h < 0.0f) {
				hsv.h += 6.0f;
			}
		} else if (max == rgb.g) {
			hsv.h = 2.0f + (rgb.b - rgb.r) / hsv.h;
		} else {
			hsv.h = 4.0f + (rgb.r - rgb.g) / hsv.h;
		}
	}
	hsv.h /= 6.0f;
	hsv.s = (max - min);
	if (max != 0.0f) {
		hsv.s /= max;
	}
	hsv.v = max;
	return hsv;
}

euclase::FloatRGB euclase::hsv_to_rgb(const FloatHSV &hsv)
{
	FloatRGB rgb;
	rgb.r = hsv.v;
	rgb.g = hsv.v;
	rgb.b = hsv.v;
	if (hsv.s > 0.0f) {
		float h = fmod(hsv.h * 6, 6);
		if (h < 0) h += 6;
		const float MAX = hsv.v;
		const float MIN = MAX - ((hsv.s) * MAX);
		const int i = (int)h;
		const float f = h - (float)i;
		switch (i) {
		default:
		case 0: // red to yellow
			rgb.r = MAX;
			rgb.g = f * (MAX - MIN) + MIN;
			rgb.b = MIN;
			break;
		case 1: // yellow to green
			rgb.r = (1.0f - f) * (MAX - MIN) + MIN;
			rgb.g = MAX;
			rgb.b = MIN;
			break;
		case 2: // green to cyan
			rgb.r = MIN;
			rgb.g = MAX;
			rgb.b = f * (MAX - MIN) + MIN;
			break;
		case 3: // cyan to blue
			rgb.r = MIN;
			rgb.g = (1.0f - f) * (MAX - MIN) + MIN;
			rgb.b = MAX;
			break;
		case 4: // blue to magenta
			rgb.r = f * (MAX - MIN) + MIN;
			rgb.g = MIN;
			rgb.b = MAX;
			break;
		case 5: // magenta to red
			rgb.r = MAX;
			rgb.g = MIN;
			rgb.b = (1.0f - f) * (MAX - MIN) + MIN;
			break;
		}
	}
	return rgb;
}
