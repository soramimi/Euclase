#include "euclase.h"

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
	double u = 1 - u;
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

bool euclase::Image::isNull() const
{
	return image_.isNull();
}

void euclase::Image::make2(int width, int height, QImage::Format format)
{
	switch (format) {
	case QImage::Format_RGB32:
		format_ = Format::RGB8;
		data_ = std::make_shared<std::vector<uint8_t>>(width * height * 3);
		break;
	case QImage::Format_RGB888:
		format_ = Format::RGB8;
		data_ = std::make_shared<std::vector<uint8_t>>(width * height * 3);
		break;
	case QImage::Format_RGBA8888:
		format_ = Format::RGBA8;
		data_ = std::make_shared<std::vector<uint8_t>>(width * height * 4);
		break;
	case QImage::Format_Grayscale8:
		format_ = Format::Grayscale8;
		data_ = std::make_shared<std::vector<uint8_t>>(width * height);
		break;
	default:
		data_.reset();
	}
}

void euclase::Image::make(int width, int height, QImage::Format format)
{
	image_ = QImage(width, height, format);
	make2(width, height, format);
}

void euclase::Image::make(const QSize &sz, QImage::Format format)
{
	make(sz.width(), sz.height(), format);
}

uint8_t *euclase::Image::scanLine2(int y)
{
	uint8_t *p = data_->data();
	int w = width();
	switch (format_) {
	case Format::RGB8:
		return p + 3 * w * y;
	case Format::RGBA8:
		return p + 4 * w * y;
	case Format::Grayscale8:
		return p + w * y;
	}
	return nullptr;
}

void euclase::Image::fill(const QColor &color)
{
	image_.fill(color);
	int w = width();
	int h = height();
	switch (format_) {
	case Format::RGB8:
		for (int y = 0; y < h; y++) {
			uint8_t *p =scanLine2(y);
			for (int x = 0; x < w; x++) {
				p[x * 3 + 0] = color.red();
				p[x * 3 + 1] = color.green();
				p[x * 3 + 2] = color.blue();
			}
		}
		break;
	case Format::RGBA8:
		for (int y = 0; y < h; y++) {
			uint8_t *p =scanLine2(y);
			for (int x = 0; x < w; x++) {
				p[x * 4 + 0] = color.red();
				p[x * 4 + 1] = color.green();
				p[x * 4 + 2] = color.blue();
				p[x * 4 + 4] = color.alpha();
			}
		}
		break;
	case Format::Grayscale8:
		for (int y = 0; y < h; y++) {
			uint8_t *p =scanLine2(y);
			for (int x = 0; x < w; x++) {
				p[x] = euclase::gray(color.red(), color.green(), color.blue());
			}
		}
		break;
	}
}

void euclase::Image::setImage(const QImage &image)
{
	image_ = image;
	int w = width();
	int h = height();
	make2(w, h, image.format());
	switch (format_) {
	case Format::RGB8:
		for (int y = 0; y < h; y++) {
			uint8_t const *s = image.scanLine(y);
			uint8_t *d = scanLine2(y);
			memcpy(d, s, w * 3);
		}
		return;
	case Format::RGBA8:
		for (int y = 0; y < h; y++) {
			uint8_t const *s = image.scanLine(y);
			uint8_t *d = scanLine2(y);
			memcpy(d, s, w * 4);
		}
		return;
	case Format::Grayscale8:
		for (int y = 0; y < h; y++) {
			uint8_t const *s = image.scanLine(y);
			uint8_t *d = scanLine2(y);
			memcpy(d, s, w);
		}
		return;
	}
}

QImage &euclase::Image::getImage()
{
	return image_;
}

const QImage &euclase::Image::getImage() const
{
	return image_;
}

QImage euclase::Image::copyImage() const
{
	return image_.copy();
}

euclase::Image euclase::Image::scaled(int w, int h) const
{
	Image newimage;
	QImage img = image_.scaled(w, h);
	newimage.setImage(img);
	return newimage;
}

QImage::Format euclase::Image::format() const
{
	return image_.format();
}

uint8_t *euclase::Image::scanLine(int y)
{
	return image_.scanLine(y);
}

const uint8_t *euclase::Image::scanLine(int y) const
{
	return image_.scanLine(y);
}

QPoint euclase::Image::offset() const
{
	return header_.offset();
}

void euclase::Image::setOffset(const QPoint &pt)
{
	header_.offset_ = pt;
}

void euclase::Image::setOffset(int x, int y)
{
	setOffset(QPoint(x, y));
}

int euclase::Image::width() const
{
	return image_.width();
}

int euclase::Image::height() const
{
	return image_.height();
}

QSize euclase::Image::size() const
{
	return image_.size();
}

bool euclase::Image::isRGBA8888() const
{
	return image_.format() == QImage::Format_RGBA8888;
}

bool euclase::Image::isGrayscale8() const
{
	return image_.format() == QImage::Format_Grayscale8;
}
