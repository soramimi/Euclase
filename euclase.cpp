
#include <QDebug>

#if !defined(_WIN32) && !defined(__APPLE__)
#include <QDebug>
#include <x86intrin.h>
#endif

#ifdef USE_CUDA
#include "ApplicationGlobal.h"
#endif

#include "euclase.h"

using namespace euclase;

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

PointF euclase::cubicBezierPoint(PointF const &p0, PointF const &p1, PointF const &p2, PointF const &p3, double t)
{
	double x = cubicBezierPoint(p0.x(), p1.x(), p2.x(), p3.x(), t);
	double y = cubicBezierPoint(p0.y(), p1.y(), p2.y(), p3.y(), t);
	return PointF(x, y);
}

void euclase::cubicBezierSplit(PointF *p0, PointF *p1, PointF *p2, PointF *p3, PointF *q0, PointF *q1, PointF *q2, PointF *q3, double t)
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
#ifdef USE_CUDA
			if (ptr_->memtype_ == CUDA && ptr_->cudamem_) {
				global->cuda->free(ptr_->cudamem_);
			}
#endif
			delete[] ptr_;
		}
	}
	ptr_ = p;
}

void euclase::Image::fill(const Color &color)
{
	int w = width();
	int h = height();
	switch (format()) {
	case Image::Format_U8_RGB:
		for (int y = 0; y < h; y++) {
			uint8_t *p = scanLine(y);
			for (int x = 0; x < w; x++) {
				p[x * 3 + 0] = color.red();
				p[x * 3 + 1] = color.green();
				p[x * 3 + 2] = color.blue();
			}
		}
		return;
	case Image::Format_U8_RGBA:
#ifdef USE_CUDA
		if (memtype() == CUDA) {
			uint8_t r = color.red();
			uint8_t g = color.green();
			uint8_t b = color.blue();
			uint8_t a = color.alpha();
			global->cuda->fill_uint8_rgba(w, h, r, g, b, a, data(), width(), height(), 0, 0);
			return;
		}
#endif
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
	case Image::Format_U8_Grayscale:
		{
			uint8_t k = euclase::gray(color.red(), color.green(), color.blue());
#ifdef USE_CUDA
			if (memtype() == CUDA) {
				global->cuda->memset(data(), k, w * h);
				return;
			}
#endif
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
	case Image::Format_F32_RGBA:
		{
			euclase::OctetRGBA icolor(color.red(), color.green(), color.blue(), color.alpha());
			euclase::Float32RGBA fcolor = euclase::Float32RGBA::convert(icolor);
#ifdef USE_CUDA
			if (memtype() == CUDA) {
				global->cuda->fill_fp32_rgba(w, h, fcolor.r, fcolor.g, fcolor.b, fcolor.a, data(), width(), height(), 0, 0);
				return;
			}
#endif
			if (memtype() == Host) {
				for (int y = 0; y < h; y++) {
					euclase::Float32RGBA *p = (euclase::Float32RGBA *)scanLine(y);
					for (int x = 0; x < w; x++) {
						p[x] = fcolor;
					}
				}
				return;
			}
		}
		break;
	case Image::Format_F16_RGBA:
		{
			euclase::OctetRGBA icolor(color.red(), color.green(), color.blue(), color.alpha());
			euclase::Float16RGBA fcolor = euclase::Float16RGBA::convert(icolor);
#ifdef USE_CUDA
			if (memtype() == CUDA) {
				global->cuda->fill_fp16_rgba(w, h, fcolor.r, fcolor.g, fcolor.b, fcolor.a, data(), width(), height(), 0, 0);
				return;
			}
#endif
			if (memtype() == Host) {
				for (int y = 0; y < h; y++) {
					euclase::Float16RGBA *p = (euclase::Float16RGBA *)scanLine(y);
					for (int x = 0; x < w; x++) {
						p[x] = fcolor;
					}
				}
				return;
			}
		}
		break;
	}
#ifdef USE_CUDA
	if (memtype() == CUDA) {
		auto img = toHost();
		img.fill(color);
		img = img.toCUDA();
		assign(img.ptr_);
	}
#endif
}

#ifdef USE_QT
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
		f = Image::Format_U8_Grayscale;
		break;
	case QImage::Format_RGB888:
	case QImage::Format_RGB32:
	case QImage::Format_RGBA8888:
	case QImage::Format_ARGB32:
		srcimage = image.convertToFormat(QImage::Format_RGBA8888);
		f = Image::Format_U8_RGBA;
		break;
	}

	make(w, h, f);

	if (srcimage.format() == QImage::Format_RGBA8888 && format() == Image::Format_U8_RGBA) {
		for (int y = 0; y < h; y++) {
			uint8_t const *s = srcimage.scanLine(y);
			uint8_t *d = scanLine(y);
			memcpy(d, s, w * 4);
		}
	} else if (srcimage.format() == QImage::Format_RGB888 && format() == Image::Format_U8_RGB) {
		for (int y = 0; y < h; y++) {
			uint8_t const *s = srcimage.scanLine(y);
			uint8_t *d = scanLine(y);
			memcpy(d, s, w * 3);
		}
	} else if (srcimage.format() == QImage::Format_Grayscale8 && format() == Image::Format_U8_Grayscale) {
		for (int y = 0; y < h; y++) {
			uint8_t const *s = srcimage.scanLine(y);
			uint8_t *d = scanLine(y);
			memcpy(d, s, w);
		}
	}
}

QImage euclase::Image::qimage() const
{
	int w = width();
	int h = height();
	if (format() == Image::Format_U8_RGB) {
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
	if (format() == Image::Format_F16_RGBA) {
		QImage newimage(w, h, QImage::Format_RGBA8888);
#ifdef USE_CUDA
		if (memtype() == CUDA) {
			euclase::Image tmpimg(w, h, Format_U8_RGBA, CUDA);
			global->cuda->scale_fp16_to_uint8_rgba(w, h, w, tmpimg.data(), w, h, data());
			return tmpimg.qimage();
		}
#endif
		if (memtype() == Host) {
			for (int y = 0; y < h; y++) {
				euclase::Float16RGBA const *s = (euclase::Float16RGBA const *)scanLine(y);
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
	if (format() == Image::Format_F32_RGBA) {
		QImage newimage(w, h, QImage::Format_RGBA8888);
#ifdef USE_CUDA
		if (memtype() == CUDA) {
			euclase::Image tmpimg(w, h, Format_U8_RGBA, CUDA);
			global->cuda->scale_fp32_to_uint8_rgba(w, h, w, tmpimg.data(), w, h, data());
			return tmpimg.qimage();
		}
#endif
		if (memtype() == Host) {
			for (int y = 0; y < h; y++) {
				euclase::Float32RGBA const *s = (euclase::Float32RGBA const *)scanLine(y);
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
	if (format() == Image::Format_U8_RGBA) {
		QImage newimage(w, h, QImage::Format_RGBA8888);
#ifdef USE_CUDA
		if (memtype() == CUDA) {
			global->cuda->copy_uint8_rgba(w, h, data(), w, h, 0, 0, newimage.bits(), w, h, 0, 0);
			return newimage;
		}
#endif
		if (memtype() == Host) {
			for (int y = 0; y < h; y++) {
				uint8_t const *s = scanLine(y);
				uint8_t *d = newimage.scanLine(y);
				memcpy(d, s, 4 * w);
			}
			return newimage;
		}
	}
	if (format() == Image::Format_U8_Grayscale) {
		QImage newimage(w, h, QImage::Format_Grayscale8);
#ifdef USE_CUDA
		if (memtype() == CUDA) {
			for (int y = 0; y < h; y++) {
				global->cuda->memcpy_dtoh(newimage.scanLine(y), scanLine(y), w);
			}
			return newimage;
		}
#endif
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

#endif

euclase::Image euclase::Image::convertToFormat(Image::Format newformat) const
{
	if (format() == newformat) {
		return *this;
	}

#ifdef USE_CUDA
	if (memtype() == CUDA) {
		return toHost().convertToFormat(newformat).toCUDA();
	}
#endif

	int w = width();
	int h = height();
	euclase::Image newimg;
	if (newformat == Image::Format_U8_RGBA) {
		switch (format()) {
		case Format_F32_RGBA:
			newimg.make(w, h, newformat);
			for (int y = 0; y < h; y++) {
				euclase::Float32RGBA const *src = (euclase::Float32RGBA const *)scanLine(y);
				euclase::OctetRGBA *dst = (euclase::OctetRGBA *)newimg.scanLine(y);
				for (int x = 0; x < w; x++) {
					dst[x] = euclase::OctetRGBA::convert(src[x].limit());
				}
			}
			break;
		}
	} else if (newformat == Image::Format_U8_RGB) {
		switch (format()) {
		case Format_U8_Grayscale:
			newimg.make(w, h, newformat);
			for (int y = 0; y < h; y++) {
				euclase::OctetGray const *src = (euclase::OctetGray const *)scanLine(y);
				euclase::OctetRGB *dst = (euclase::OctetRGB *)newimg.scanLine(y);
				for (int x = 0; x < w; x++) {
					dst[x] = euclase::OctetRGB::convert(src[x]);
				}
			}
			break;
		}
	} else if (newformat == Image::Format_F32_RGBA) {
		switch (format()) {
		case Format_U8_RGBA:
			newimg.make(w, h, newformat);
			for (int y = 0; y < h; y++) {
				euclase::OctetRGBA const *src = (euclase::OctetRGBA const *)scanLine(y);
				euclase::Float32RGBA *dst = (euclase::Float32RGBA *)newimg.scanLine(y);
				for (int x = 0; x < w; x++) {
					dst[x] = euclase::Float32RGBA::convert(src[x]);
				}
			}
			break;
		case Format_F16_RGBA:
			newimg.make(w, h, newformat);
			for (int y = 0; y < h; y++) {
				euclase::Float16RGBA const *src = (euclase::Float16RGBA const *)scanLine(y);
				euclase::Float32RGBA *dst = (euclase::Float32RGBA *)newimg.scanLine(y);
				for (int x = 0; x < w; x++) {
					dst[x] = euclase::Float32RGBA::convert(src[x]);
				}
			}
			break;
		}
	} else if (newformat == Image::Format_F32_RGB) {
		switch (format()) {
		case Format_U8_RGB:
			newimg.make(w, h, newformat);
			for (int y = 0; y < h; y++) {
				uint8_t const *s = scanLine(y);
				Float32RGB *d = (Float32RGB *)newimg.scanLine(y);
				for (int x = 0; x < w; x++) {
					d[x] = Float32RGB::convert(OctetRGBA(s[3 * x + 0], s[3 * x + 1], s[3 * x + 2], 255));
				}
			}
			break;
		}
	} else if (newformat == Image::Format_F16_RGBA) {
		switch (format()) {
		case Format_U8_RGB:
			newimg.make(w, h, newformat);
			for (int y = 0; y < h; y++) {
				euclase::OctetRGB const *src = (euclase::OctetRGB const *)scanLine(y);
				euclase::Float16RGBA *dst = (euclase::Float16RGBA *)newimg.scanLine(y);
				for (int x = 0; x < w; x++) {
					dst[x] = euclase::Float16RGBA::convert(src[x]);
				}
			}
			break;
		case Format_U8_RGBA:
			newimg.make(w, h, newformat);
			for (int y = 0; y < h; y++) {
				euclase::OctetRGBA const *src = (euclase::OctetRGBA const *)scanLine(y);
				euclase::Float16RGBA *dst = (euclase::Float16RGBA *)newimg.scanLine(y);
				for (int x = 0; x < w; x++) {
					dst[x] = euclase::Float16RGBA::convert(src[x]);
				}
			}
			break;
		case Format_F32_RGBA:
			newimg.make(w, h, newformat);
			for (int y = 0; y < h; y++) {
				euclase::Float32RGBA const *src = (euclase::Float32RGBA const *)scanLine(y);
				euclase::Float16RGBA *dst = (euclase::Float16RGBA *)newimg.scanLine(y);
				for (int x = 0; x < w; x++) {
					dst[x] = euclase::Float16RGBA::convert(src[x]);
				}
			}
			break;
		}
	} else if (newformat == Image::Format_F16_RGB) {
		switch (format()) {
		case Format_U8_RGB:
			newimg.make(w, h, newformat);
			for (int y = 0; y < h; y++) {
				uint8_t const *s = scanLine(y);
				Float16RGB *d = (Float16RGB *)newimg.scanLine(y);
				for (int x = 0; x < w; x++) {
					d[x] = Float16RGB::convert(OctetRGBA(s[3 * x + 0], s[3 * x + 1], s[3 * x + 2], 255));
				}
			}
			break;
		}
	} else if (newformat == Image::Format_U8_GrayscaleA) {
		switch (format()) {
		case Format_F32_RGBA:
			newimg.make(w, h, newformat);
			for (int y = 0; y < h; y++) {
				Float32RGBA const *s = (Float32RGBA const *)scanLine(y);
				OctetGrayA *d = (OctetGrayA *)newimg.scanLine(y);
				for (int x = 0; x < w; x++) {
					d[x] = OctetGrayA::convert(s[x].limit());
				}
			}
			break;
		case Format_U8_RGBA:
			newimg.make(w, h, newformat);
			for (int y = 0; y < h; y++) {
				OctetRGBA const *s = (OctetRGBA const *)scanLine(y);
				OctetGrayA *d = (OctetGrayA *)newimg.scanLine(y);
				for (int x = 0; x < w; x++) {
					d[x] = OctetGrayA::convert(s[x]);
				}
			}
			break;
		case Format_U8_RGB:
			newimg.make(w, h, newformat);
			for (int y = 0; y < h; y++) {
				OctetRGB const *s = (OctetRGB const *)scanLine(y);
				OctetGrayA *d = (OctetGrayA *)newimg.scanLine(y);
				for (int x = 0; x < w; x++) {
					d[x] = OctetGrayA::convert(s[x]);
				}
			}
			break;
		case Format_U8_Grayscale:
			newimg.make(w, h, newformat);
			for (int y = 0; y < h; y++) {
				OctetGray const *s = (OctetGray const *)scanLine(y);
				OctetGrayA *d = (OctetGrayA *)newimg.scanLine(y);
				for (int x = 0; x < w; x++) {
					d[x] = OctetGrayA::convert(s[x]);
				}
			}
			break;
		}
	} else if (newformat == Image::Format_U8_Grayscale) {
		switch (format()) {
		case Format_F32_RGBA:
			newimg.make(w, h, newformat);
			for (int y = 0; y < h; y++) {
				Float32RGBA const *s = (Float32RGBA const *)scanLine(y);
				OctetGray *d = (OctetGray *)newimg.scanLine(y);
				for (int x = 0; x < w; x++) {
					d[x] = OctetGray::convert(s[x].limit());
				}
			}
			break;
		case Format_U8_RGBA:
			newimg.make(w, h, newformat);
			for (int y = 0; y < h; y++) {
				OctetRGBA const *s = (OctetRGBA const *)scanLine(y);
				OctetGray *d = (OctetGray *)newimg.scanLine(y);
				for (int x = 0; x < w; x++) {
					d[x] = OctetGray::convert(s[x]);
				}
			}
			break;
		case Format_U8_RGB:
			newimg.make(w, h, newformat);
			for (int y = 0; y < h; y++) {
				OctetRGB const *s = (OctetRGB const *)scanLine(y);
				OctetGray *d = (OctetGray *)newimg.scanLine(y);
				for (int x = 0; x < w; x++) {
					d[x] = OctetGray::convert(s[x]);
				}
			}
			break;
		case Format_U8_GrayscaleA:
			newimg.make(w, h, newformat);
			for (int y = 0; y < h; y++) {
				OctetGrayA const *s = (OctetGrayA const *)scanLine(y);
				OctetGray *d = (OctetGray *)newimg.scanLine(y);
				for (int x = 0; x < w; x++) {
					d[x] = OctetGray::convert(s[x]);
				}
			}
			break;
		}
	} else if (newformat == Image::Format_F32_Grayscale) {
		switch (format()) {
		case Format_U8_Grayscale:
			newimg.make(w, h, newformat);
			for (int y = 0; y < h; y++) {
				Float32RGBA const *s = (Float32RGBA const *)scanLine(y);
				Float32Gray *d = (Float32Gray *)newimg.scanLine(y);
				for (int x = 0; x < w; x++) {
					d[x] = Float32Gray::convert(s[x]);
				}
			}
			break;
		case Format_F32_RGBA:
			newimg.make(w, h, newformat);
			for (int y = 0; y < h; y++) {
				Float32RGBA const *s = (Float32RGBA const *)scanLine(y);
				Float32Gray *d = (Float32Gray *)newimg.scanLine(y);
				for (int x = 0; x < w; x++) {
					d[x] = Float32Gray::convert(s[x]);
				}
			}
			break;
		case Format_U8_RGBA:
			newimg.make(w, h, newformat);
			for (int y = 0; y < h; y++) {
				OctetRGBA const *s = (OctetRGBA const *)scanLine(y);
				Float32Gray *d = (Float32Gray *)newimg.scanLine(y);
				for (int x = 0; x < w; x++) {
					d[x] = Float32Gray::convert(s[x]);
				}
			}
			break;
		}
	} else if (newformat == Image::Format_F32_GrayscaleA) {
		switch (format()) {
		case Format_U8_GrayscaleA:
			newimg.make(w, h, newformat);
			for (int y = 0; y < h; y++) {
				OctetGrayA const *s = (OctetGrayA const *)scanLine(y);
				Float32GrayA *d = (Float32GrayA *)newimg.scanLine(y);
				for (int x = 0; x < w; x++) {
					d[x] = Float32GrayA::convert(s[x]);
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
	case euclase::Image::Format_F32_RGB:
	case euclase::Image::Format_F32_RGBA:
	case euclase::Image::Format_F32_Grayscale:
	case euclase::Image::Format_F32_GrayscaleA:
		return *this;
	case Format_U8_RGB:
		return convertToFormat(Format_F32_RGB);
	case Format_U8_RGBA:
		return convertToFormat(Format_F32_RGBA);
	case Format_U8_Grayscale:
		return convertToFormat(Format_F32_Grayscale);
	case Format_U8_GrayscaleA:
		return convertToFormat(Format_F32_GrayscaleA);
	}
	return {};
}

void euclase::Image::swap(Image &other)
{
	std::swap(ptr_, other.ptr_);
}

euclase::Image euclase::Image::scaled(int w, int h, bool smooth) const
{
#if 1
	return euclase::resizeImage(*this, w, h, smooth ? EnlargeMethod::Bicubic : EnlargeMethod::NearestNeighbor);
#else
	Image newimage;
	QImage img = image_.scaled(w, h);
	newimage.setImage(img);
	return std::move(newimage);
#endif
}

Size euclase::Image::size() const
{
	return {width(), height()};
}

#ifdef USE_QT
QImage::Format euclase::qimageformat(Image::Format format)
{
	switch (format) {
	case euclase::Image::Format_U8_RGB:
		return QImage::Format_RGB888;
	case euclase::Image::Format_U8_RGBA:
		return QImage::Format_RGBA8888;
	case euclase::Image::Format_U8_Grayscale:
		return QImage::Format_Grayscale8;
	}
	return QImage::Format_Invalid;
}
#endif

int euclase::bytesPerPixel(Image::Format format)
{
	switch (format) {
	case Image::Format_U8_RGB:
		return 3;
	case Image::Format_U8_RGBA:
		return 4;
	case Image::Format_U8_Grayscale:
		return 1;
	case Image::Format_U8_GrayscaleA:
		return 2;
	case Image::Format_F32_RGB:
		return sizeof(float) * 3;
	case Image::Format_F32_RGBA:
		return sizeof(float) * 4;
	case Image::Format_F32_Grayscale:
		return sizeof(float);
	case Image::Format_F32_GrayscaleA:
		return sizeof(float) * 2;
	case Image::Format_F16_RGB:
		return sizeof(_float16_t) * 3;
	case Image::Format_F16_RGBA:
		return sizeof(_float16_t) * 4;
	case Image::Format_F16_Grayscale:
		return sizeof(_float16_t);
	case Image::Format_F16_GrayscaleA:
		return sizeof(_float16_t) * 2;
	}
	return 0;
}

bool euclase::Image::init(int w, int h, Image::Format format, MemoryType memtype, const Color &color)
{
	if (w < 1 || h < 1) {
		qDebug() << "euclase::Image::init: invalid size" << w << h;
		w = h = 0;
		return false;
	}
	const size_t datasize = (size_t)w * (size_t)h * euclase::bytesPerPixel(format);
	Data *p;
	try {
		size_t n = sizeof(Data) + (memtype == Host ? datasize : 0);
		p = (Data *)new char[n];
	} catch (std::bad_alloc &e) {
		qDebug() << "euclase::Image::init: bad_alloc" << e.what();
		return false;
	}
	if (!p) {
		assert(p);
	}
	new(p) Data();
	assign(p);
	p->memtype_ = memtype;
	ptr_->format_ = format;
	ptr_->width_ = w;
	ptr_->height_ = h;
#ifdef USE_CUDA
	if (memtype == CUDA) {
		Q_ASSERT(global && global->cuda);
		ptr_->cudamem_ = global->cuda->malloc(datasize);
	}
#endif
	fill(color);
	return true;
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
		const int datasize = w * h * euclase::bytesPerPixel(f);
		switch (dst_memtype) {
		case Host:
			switch (src_memtype) {
			case Host:
				memcpy(newimg.ptr_->data(), ptr_->data(), datasize);
				break;
#ifdef USE_CUDA
			case CUDA:
				global->cuda->memcpy_dtoh(newimg.ptr_->data(), ptr_->data(), datasize);
				break;
#endif
			}
			break;
#ifdef USE_CUDA
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
#endif
		}
	}
	return newimg;
}

euclase::Image &euclase::Image::memconvert(MemoryType memtype)
{
	if (ptr_ && ptr_->memtype_ != memtype) {
		Image newimg = copy(memtype);
		assign(newimg.ptr_);
	}
	return *this;
}

euclase::FloatHSV euclase::rgb_to_hsv(const Float32RGB &rgb)
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

euclase::Float32RGB euclase::hsv_to_rgb(const FloatHSV &hsv)
{
	Float32RGB rgb;
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

// resize/blur

using Float32RGB = euclase::Float32RGB;
using Float32Gray = euclase::Float32Gray;
using Float32RGBA = euclase::Float32RGBA;
using Float32GrayA = euclase::Float32GrayA;

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

	euclase::Image newimg(dst_w, dst_h, FORMAT);
#pragma omp parallel for
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
#pragma omp parallel for
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
#pragma omp parallel for
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
#pragma omp parallel for
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
#pragma omp parallel for
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
	if (!newimg.make(dst_w, dst_h, (euclase::Image::Format)FORMAT)) {
		return {}; // bad alloc?
	}

	std::vector<double> bicubic_lut_x;
	std::vector<double> bicubic_lut_y;
	bicubic_lut_t bicubic_lut_x_p = makeBicubicLookupTable(src_w, dst_w, &bicubic_lut_x);
	bicubic_lut_t bicubic_lut_y_p = makeBicubicLookupTable(src_h, dst_h, &bicubic_lut_y);

#pragma omp parallel for
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

#pragma omp parallel for
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

#pragma omp parallel for
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

#pragma omp parallel for
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
					newimage = resizeNearestNeighbor<euclase::Image::Format_F32_RGBA, Float32RGBA>(newimage, dst_w, dst_h);
				} else {
					if (dst_w < w && dst_h < h) {
						if (alphachannel) {
							newimage = resizeAveragingT<euclase::Image::Format_F32_RGBA, Float32RGBA, Float32RGBA>(image, dst_w, dst_h);
						} else {
							newimage = resizeAveragingT<euclase::Image::Format_F32_RGBA, Float32RGB, Float32RGB>(image, dst_w, dst_h);
						}
					} else if (dst_w < w) {
						if (alphachannel) {
							newimage = resizeAveragingHT<euclase::Image::Format_F32_RGBA, Float32RGBA, Float32RGBA>(image, dst_w);
						} else {
							newimage = resizeAveragingHT<euclase::Image::Format_F32_RGBA, Float32RGB, Float32RGB>(image, dst_w);
						}
					} else if (dst_h < h) {
						if (alphachannel) {
							newimage = resizeAveragingVT<euclase::Image::Format_F32_RGBA, Float32RGBA, Float32RGBA>(image, dst_h);
						} else {
							newimage = resizeAveragingVT<euclase::Image::Format_F32_RGBA, Float32RGB, Float32RGB>(image, dst_h);
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
							newimage = resizeBilinearT<euclase::Image::Format_F32_RGBA, Float32RGBA, Float32RGBA>(newimage, dst_w, dst_h);
						} else {
							newimage = resizeBilinearT<euclase::Image::Format_F32_RGBA, Float32RGB, Float32RGB>(newimage, dst_w, dst_h);
						}
					} else if (dst_w > w) {
						if (alphachannel) {
							newimage = resizeBilinearHT<euclase::Image::Format_F32_RGBA, Float32RGBA, Float32RGBA>(newimage, dst_w);
						} else {
							newimage = resizeBilinearHT<euclase::Image::Format_F32_RGBA, Float32RGB, Float32RGB>(newimage, dst_w);
						}
					} else if (dst_h > h) {
						if (alphachannel) {
							newimage = resizeBilinearVT<euclase::Image::Format_F32_RGBA, Float32RGBA, Float32RGBA>(newimage, dst_h);
						} else {
							newimage = resizeBilinearVT<euclase::Image::Format_F32_RGBA, Float32RGB, Float32RGB>(newimage, dst_h);
						}
					}
				} else if (method == EnlargeMethod::Bicubic) {
					if (dst_w > w && dst_h > h) {
						if (alphachannel) {
							newimage = resizeBicubicT<euclase::Image::Format_F32_RGBA, Float32RGBA, Float32RGBA>(newimage, dst_w, dst_h);
						} else {
							newimage = resizeBicubicT<euclase::Image::Format_F32_RGBA, Float32RGB, Float32RGB>(newimage, dst_w, dst_h);
						}
					} else if (dst_w > w) {
						if (alphachannel) {
							newimage = resizeBicubicHT<euclase::Image::Format_F32_RGBA, Float32RGBA, Float32RGBA>(newimage, dst_w);
						} else {
							newimage = resizeBicubicHT<euclase::Image::Format_F32_RGBA, Float32RGB, Float32RGB>(newimage, dst_w);
						}
					} else if (dst_h > h) {
						if (alphachannel) {
							newimage = resizeBicubicVT<euclase::Image::Format_F32_RGBA, Float32RGBA, Float32RGBA>(newimage, dst_h);
						} else {
							newimage = resizeBicubicVT<euclase::Image::Format_F32_RGBA, Float32RGB, Float32RGB>(newimage, dst_h);
						}
					}
				} else {
					newimage = resizeNearestNeighbor<euclase::Image::Format_F32_RGBA, Float32RGBA>(newimage, dst_w, dst_h);
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
					newimage = resizeNearestNeighbor<euclase::Image::Format_F32_Grayscale, Float32Gray>(newimage, dst_w, dst_h);
				} else {
					if (dst_w < w && dst_h < h) {
						if (alphachannel) {
							newimage = resizeAveragingT<euclase::Image::Format_F32_Grayscale, Float32GrayA, Float32GrayA>(newimage, dst_w, dst_h);
						} else {
							newimage = resizeAveragingT<euclase::Image::Format_F32_Grayscale, Float32Gray, Float32Gray>(newimage, dst_w, dst_h);
						}
					} else if (dst_w < w) {
						if (alphachannel) {
							newimage = resizeAveragingHT<euclase::Image::Format_F32_Grayscale, Float32GrayA, Float32GrayA>(newimage, dst_w);
						} else {
							newimage = resizeAveragingHT<euclase::Image::Format_F32_Grayscale, Float32Gray, Float32Gray>(newimage, dst_w);
						}
					} else if (dst_h < h) {
						if (alphachannel) {
							newimage = resizeAveragingVT<euclase::Image::Format_F32_Grayscale, Float32GrayA, Float32GrayA>(newimage, dst_h);
						} else {
							newimage = resizeAveragingVT<euclase::Image::Format_F32_Grayscale, Float32Gray, Float32Gray>(newimage, dst_h);
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
							newimage = resizeBilinearT<euclase::Image::Format_F32_Grayscale, Float32GrayA, Float32GrayA>(newimage, dst_w, dst_h);
						} else {
							newimage = resizeBilinearT<euclase::Image::Format_F32_Grayscale, Float32Gray, Float32Gray>(newimage, dst_w, dst_h);
						}
					} else if (dst_w > w) {
						if (alphachannel) {
							newimage = resizeBilinearHT<euclase::Image::Format_F32_Grayscale, Float32GrayA, Float32GrayA>(newimage, dst_w);
						} else {
							newimage = resizeBilinearHT<euclase::Image::Format_F32_Grayscale, Float32Gray, Float32Gray>(newimage, dst_w);
						}
					} else if (dst_h > h) {
						if (alphachannel) {
							newimage = resizeBilinearVT<euclase::Image::Format_F32_Grayscale, Float32GrayA, Float32GrayA>(newimage, dst_h);
						} else {
							newimage = resizeBilinearVT<euclase::Image::Format_F32_Grayscale, Float32Gray, Float32Gray>(newimage, dst_h);
						}
					}
				} else if (method == EnlargeMethod::Bicubic) {
					if (dst_w > w && dst_h > h) {
						if (alphachannel) {
							newimage = resizeBicubicT<euclase::Image::Format_F32_Grayscale, Float32GrayA, Float32GrayA>(newimage, dst_w, dst_h);
						} else {
							newimage = resizeBicubicT<euclase::Image::Format_F32_Grayscale, Float32Gray, Float32Gray>(newimage, dst_w, dst_h);
						}
					} else if (dst_w > w) {
						if (alphachannel) {
							newimage = resizeBicubicHT<euclase::Image::Format_F32_Grayscale, Float32GrayA, Float32GrayA>(newimage, dst_w);
						} else {
							newimage = resizeBicubicHT<euclase::Image::Format_F32_Grayscale, Float32Gray, Float32Gray>(newimage, dst_w);
						}
					} else if (dst_h > h) {
						if (alphachannel) {
							newimage = resizeBicubicVT<euclase::Image::Format_F32_Grayscale, Float32GrayA, Float32GrayA>(newimage, dst_h);
						} else {
							newimage = resizeBicubicVT<euclase::Image::Format_F32_Grayscale, Float32Gray, Float32Gray>(newimage, dst_h);
						}
					}
				} else {
					newimage = resizeNearestNeighbor<euclase::Image::Format_F32_Grayscale, Float32Gray>(newimage, dst_w, dst_h);
				}
			}
		}
	}
	return newimage;
}

euclase::Image euclase::resizeImage(euclase::Image const &image, int dst_w, int dst_h, EnlargeMethod method)
{
	if (image.width() == dst_w && image.height() == dst_h) return image;

	auto memtype = image.memtype();
	if (memtype != euclase::Image::Host) {
		auto newimg = resizeImage(image.toHost(), dst_w, dst_h, method);
		return newimg.memconvert(memtype);
	}

	bool alphachannel = false;
	switch (image.format()) {
	case euclase::Image::Format_U8_RGBA:
	case euclase::Image::Format_F32_RGBA:
	case euclase::Image::Format_U8_GrayscaleA:
	case euclase::Image::Format_F32_GrayscaleA:
		alphachannel = true;
		break;
	}

	switch (image.format()) {
	case euclase::Image::Format_U8_Grayscale:
		return resizeNearestNeighbor<euclase::Image::Format_U8_Grayscale, euclase::OctetGray>(image, dst_w, dst_h);
	case euclase::Image::Format_U8_GrayscaleA:
		return resizeNearestNeighbor<euclase::Image::Format_U8_GrayscaleA, euclase::OctetGrayA>(image, dst_w, dst_h);
	case euclase::Image::Format_F32_RGB:
	case euclase::Image::Format_F32_RGBA:
		return resizeColorImage(image, dst_w, dst_h, method, alphachannel);
	case euclase::Image::Format_F32_Grayscale:
	case euclase::Image::Format_F32_GrayscaleA:
		return resizeGrayscaleImage(image, dst_w, dst_h, method, alphachannel);
	}
	return {};
}

euclase::Image euclase::filter_blur(euclase::Image image, int radius, bool *cancel, std::function<void (float)> progress)
{
	if (image.format() == Image::Format_F32_RGBA) {
		return BlurFilter<Float32RGBA, Float32RGBA>(image, radius, cancel, progress);
	}

	if (image.format() == Image::Format_U8_Grayscale) {
		return BlurFilter<Float32GrayA, Float32GrayA>(image, radius, cancel, progress);
	}

	auto format = image.format();
	switch (format) {
	case euclase::Image::Format_U8_RGBA:
	case euclase::Image::Format_U8_GrayscaleA:
		auto img = filter_blur(image.convertToFormat(Image::Format_F32_RGBA), radius, cancel, progress);
		return img.convertToFormat(format);
	}
	return {};
}

#ifdef USE_EUCLASE_IMAGE_READ_WRITE
// image load/save

#include "png.cpph"
// #include "jpeg.cpph"

#if 0
std::optional<euclase::Image> euclase::load_jpeg(char const *path)
{
	Image image;
	if (_load_jpeg(&image, path)) {
		return image;
	}
	return std::nullopt;
}

bool euclase::save_jpeg(Image const &image, char const *path)
{
	return write_jpeg(image, path);
}
#endif

std::optional<euclase::Image> euclase::load_png(char const *path)
{
	Image image;
	if (load_png(&image, path)) {
		return image;
	}
	return std::nullopt;
}

bool euclase::save_png(Image const &image, char const *path)
{
	return write_png(image, path);
}

#endif


