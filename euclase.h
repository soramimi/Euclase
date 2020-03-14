#ifndef EUCLASE_H
#define EUCLASE_H

#include <QImage>
#include <QPoint>
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace euclase {

template <typename T>
static inline T clamp(T a, T min, T max)
{
	return std::max(min, std::min(max, a));
}

static inline int gray(int r, int g, int b)
{
	return (306 * r + 601 * g + 117 * b) / 1024;
}

static inline float gamma(float v)
{
	return sqrt(v);
}

static inline float degamma(float v)
{
	return v * v;
}

struct PixelGrayA;

struct PixelRGBA {
	uint8_t r, g, b, a;
	PixelRGBA()
		: r(0)
		, g(0)
		, b(0)
		, a(0)
	{
	}
	PixelRGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
		: r(r)
		, g(g)
		, b(b)
		, a(a)
	{
	}
	inline PixelRGBA(PixelGrayA const &t);
	uint8_t gray() const
	{
		return euclase::gray(r, g, b);
	}
};

struct PixelGrayA {
	uint8_t l, a;
	PixelGrayA()
		: l(0)
		, a(0)
	{
	}
	PixelGrayA(uint8_t l, uint8_t a = 255)
		: l(l)
		, a(a)
	{
	}
	static PixelGrayA convert(PixelGrayA const &r, bool ignored)
	{
		(void)ignored;
		return {r.l, r.a};
	}
	inline PixelGrayA(PixelRGBA const &t);
	uint8_t gray() const
	{
		return l;
	}
};

inline PixelRGBA::PixelRGBA(PixelGrayA const &t)
	: r(t.l)
	, g(t.l)
	, b(t.l)
	, a(t.a)
{
}

inline PixelGrayA::PixelGrayA(PixelRGBA const &t)
	: l(euclase::gray(t.r, t.g, t.b))
	, a(t.a)
{
}

class FPixelRGB {
public:
	float r;
	float g;
	float b;
	FPixelRGB()
		: r(0)
		, g(0)
		, b(0)
	{
	}
	FPixelRGB(float r, float g, float b)
		: r(r)
		, g(g)
		, b(b)
	{
	}
	static FPixelRGB convert(PixelRGBA const &src, bool gamma_correction)
	{
		float r = src.r / 255.0;
		float g = src.g / 255.0;
		float b = src.b / 255.0;
		if (gamma_correction) {
			r *= r;
			g *= g;
			b *= b;
		}
		return {r, g, b};
	}
	FPixelRGB operator + (FPixelRGB const &right) const
	{
		return FPixelRGB(r + right.r, g + right.g, b + right.b);
	}
	FPixelRGB operator * (float t) const
	{
		return FPixelRGB(r * t, g * t, b * t);
	}
	void operator += (FPixelRGB const &o)
	{
		r += o.r;
		g += o.g;
		b += o.b;
	}
	void add(FPixelRGB const &p, float v)
	{
		r += p.r * v;
		g += p.g * v;
		b += p.b * v;
	}
	void sub(FPixelRGB const &p, float v)
	{
		r -= p.r * v;
		g -= p.g * v;
		b -= p.b * v;
	}

	void operator *= (float t)
	{
		r *= t;
		g *= t;
		b *= t;
	}
	uint8_t r8() const
	{
		if (r <= 0) return 0;
		if (r >= 1) return 255;
		return (uint8_t)floor(r * 255 + 0.5);
	}
	uint8_t g8() const
	{
		if (g <= 0) return 0;
		if (g >= 1) return 255;
		return (uint8_t)floor(g * 255 + 0.5);
	}
	uint8_t b8() const
	{
		if (b <= 0) return 0;
		if (b >= 1) return 255;
		return (uint8_t)floor(b * 255 + 0.5);
	}
	PixelRGBA color(float amount, bool gamma_correction) const
	{
		if (amount == 1) {
			FPixelRGB t = *this;
			if (gamma_correction) {
				t.r = sqrt(t.r);
				t.g = sqrt(t.g);
				t.b = sqrt(t.b);
			}
			return PixelRGBA(t.r8(), t.g8(), t.b8());
		} else if (amount == 0) {
			return PixelRGBA(0, 0, 0);
		}
		float m = 1 / amount;
		FPixelRGB t = *this * m;
		if (gamma_correction) {
			t.r = sqrt(t.r);
			t.g = sqrt(t.g);
			t.b = sqrt(t.b);
		}
		return PixelRGBA(t.r8(), t.g8(), t.b8());
	}
	operator PixelRGBA () const
	{
		return PixelRGBA(r8(), g8(), b8());
	}
};

class FPixelGray {
public:
	float l;
	FPixelGray()
		: l(0)
	{
	}
	FPixelGray(float y)
		: l(y)
	{
	}
	static FPixelGray convert(PixelGrayA const &src, bool gamma_correction)
	{
		(void)gamma_correction; // ignore
		return {src.l / 255.0F};
	}
	FPixelGray operator + (FPixelGray const &right) const
	{
		return FPixelGray(l + right.l);
	}
	FPixelGray operator * (float t) const
	{
		return FPixelGray(l * t);
	}
	void operator += (FPixelGray const &o)
	{
		l += o.l;
	}
	void add(FPixelGray const &p, float v)
	{
		v += p.l * v;
	}
	void sub(FPixelGray const &p, float v)
	{
		v -= p.l * v;
	}

	void operator *= (float t)
	{
		l *= t;
	}
	uint8_t y8() const
	{
		if (l <= 0) return 0;
		if (l >= 1) return 255;
		return (uint8_t)floor(l * 255 + 0.5);
	}
	PixelGrayA color(float amount, bool gamma_correction) const
	{
		if (amount == 1) {
			float l = y8();
			if (gamma_correction) {
				l = sqrt(l);
			}
			return PixelGrayA(l);
		} else if (amount == 0) {
			return PixelGrayA(0);
		}
		float m = 1 / amount;
		FPixelGray p = *this * m;
		if (gamma_correction) {
			p.l = sqrt(p.l);
		}
		return PixelGrayA(p.y8());
	}
	PixelGrayA toPixelGrayA() const
	{
		return PixelGrayA(y8());
	}
};

class FPixelRGBA {
public:
	float r;
	float g;
	float b;
	float a;
	FPixelRGBA()
		: r(0)
		, g(0)
		, b(0)
		, a(0)
	{
	}
	FPixelRGBA(float r, float g, float b, float a = 1)
		: r(r)
		, g(g)
		, b(b)
		, a(a)
	{
	}
	static FPixelRGBA convert(PixelRGBA const &src, bool gamma_correction)
	{
		float r = src.r / 255.0;
		float g = src.g / 255.0;
		float b = src.b / 255.0;
		if (gamma_correction) {
			r *= r;
			g *= g;
			b *= b;
		}
		return {r, g, b, float(src.a / 255.0)};
	}
	FPixelRGBA operator + (FPixelRGBA const &right) const
	{
		return FPixelRGBA(r + right.r, g + right.g, b + right.b);
	}
	FPixelRGBA operator * (float t) const
	{
		return FPixelRGBA(r * t, g * t, b * t);
	}
	void operator += (FPixelRGBA const &o)
	{
		r += o.r;
		g += o.g;
		b += o.b;
	}
	void operator *= (float t)
	{
		r *= t;
		g *= t;
		b *= t;
	}
	void add(FPixelRGBA const &p, float v)
	{
		v *= p.a;
		a += v;
		r += p.r * v;
		g += p.g * v;
		b += p.b * v;
	}
	void sub(FPixelRGBA const &p, float v)
	{
		v *= p.a;
		a -= v;
		r -= p.r * v;
		g -= p.g * v;
		b -= p.b * v;
	}
	uint8_t r8() const
	{
		if (r <= 0) return 0;
		if (r >= 1) return 255;
		return (uint8_t)floor(r * 255 + 0.5);
	}
	uint8_t g8() const
	{
		if (g <= 0) return 0;
		if (g >= 1) return 255;
		return (uint8_t)floor(g * 255 + 0.5);
	}
	uint8_t b8() const
	{
		if (b <= 0) return 0;
		if (b >= 1) return 255;
		return (uint8_t)floor(b * 255 + 0.5);
	}
	uint8_t a8() const
	{
		if (a <= 0) return 0;
		if (a >= 1) return 255;
		return (uint8_t)floor(a * 255 + 0.5);
	}
	PixelRGBA color(float amount, bool gamma_correction) const
	{
		if (amount == 0) {
			return PixelRGBA(0, 0, 0, 0);
		}
		FPixelRGBA t(*this);
		t *= (1.0f / t.a);
		t.a = t.a / amount;
		t.a = clamp(t.a, 0.0f, 1.0f);
		if (gamma_correction) {
			t.r = sqrt(t.r);
			t.g = sqrt(t.g);
			t.b = sqrt(t.b);
		}
		return PixelRGBA(t.r8(), t.g8(), t.b8(), t.a8());
	}
	operator PixelRGBA () const
	{
		return PixelRGBA(r8(), g8(), b8(), a8());
	}
};

class FPixelGrayA {
public:
	float l;
	float a;
	FPixelGrayA()
		: l(0)
		, a(0)
	{
	}
	FPixelGrayA(float v, float a = 1)
		: l(v)
		, a(a)
	{
	}
	static FPixelGrayA convert(PixelGrayA const &src, bool gamma_correction)
	{
		float l = src.l / 255.0;
		if (gamma_correction) {
			l *= l;
		}
		return {l, float(src.a / 255.0)};
	}
	FPixelGrayA operator + (FPixelGrayA const &right) const
	{
		return FPixelGrayA(l + right.l);
	}
	FPixelGrayA operator * (float t) const
	{
		return FPixelGrayA(l * t);
	}
	void operator += (FPixelGrayA const &o)
	{
		l += o.l;
	}
	void operator *= (float t)
	{
		l *= t;
	}
	void add(FPixelGrayA const &p, float v)
	{
		v *= p.a;
		a += v;
		v += p.l * v;
	}
	void sub(FPixelGrayA const &p, float v)
	{
		v *= p.a;
		a -= v;
		v -= p.l * v;
	}
	uint8_t v8() const
	{
		if (l <= 0) return 0;
		if (l >= 1) return 255;
		return (uint8_t)floor(l * 255 + 0.5);
	}
	uint8_t a8() const
	{
		if (a <= 0) return 0;
		if (a >= 1) return 255;
		return (uint8_t)floor(a * 255 + 0.5);
	}
	PixelGrayA color(float amount, bool gamma_correction) const
	{
		if (amount == 0) {
			return PixelGrayA(0, 0);
		}
		FPixelGrayA t(*this);
		t *= (1.0f / t.a);
		t.a = t.a / amount;
		t.a = clamp(t.a, 0.0f, 1.0f);
		if (gamma_correction) {
			t.l = sqrt(t.l);
		}
		return PixelGrayA(t.v8(), t.a8());
	}
};

static inline FPixelRGBA gamma(FPixelRGBA const &pix)
{
	return FPixelRGBA(gamma(pix.r), gamma(pix.g), gamma(pix.b), pix.a);
}

static inline FPixelRGBA degamma(FPixelRGBA const &pix)
{
	return FPixelRGBA(degamma(pix.r), degamma(pix.g), degamma(pix.b), pix.a);
}

// image

struct ImageHeader {
	unsigned int ref_ = 0;
	QPoint offset_;

	QPoint offset() const
	{
		return offset_;
	}
};

class Image {
public:
	ImageHeader header_;
	QImage image_;
	enum Format {
		RGB8,
		RGBA8,
		Grayscale8,
		GrayscaleA8,
		RGBF,
		RGBAF,
		GrayscaleF8,
		GrayscaleAF8,
	};
	Format format_ = Format::RGBA8;
	bool linear_ = false;
	std::shared_ptr<std::vector<uint8_t>> data_;

	Image() = default;
	Image(QImage const &image)
	{
		setImage(image);
	}

	bool isNull() const;

	void make2(int width, int height, QImage::Format format);

	void make(int width, int height, QImage::Format format);

	void make(QSize const &sz, QImage::Format format);

	uint8_t *scanLine2(int y);

	void fill(const QColor &color);

	void setImage(QImage const &image);

	QImage &getImage();

	QImage const &getImage() const;

	QImage copyImage() const;

	Image scaled(int w, int h) const;

	QImage::Format format() const;

	uint8_t *scanLine(int y);

	uint8_t const *scanLine(int y) const;

	QPoint offset() const;

	void setOffset(QPoint const &pt);

	void setOffset(int x, int y);

	int width() const;

	int height() const;

	QSize size() const;
	bool isRGBA8888() const;

	bool isGrayscale8() const;
};

// cubic bezier curve

double cubicBezierPoint(double p0, double p1, double p2, double p3, double t);
double cubicBezierGradient(double p0, double p1, double p2, double p3, double t);
QPointF cubicBezierPoint(QPointF &p0, QPointF &p1, QPointF &p2, QPointF &p3, double t);
void cubicBezierSplit(QPointF *p0, QPointF *p1, QPointF *p2, QPointF *p3, QPointF *q0, QPointF *q1, QPointF *q2, QPointF *q3, double t);

} // namespace euclase

#endif // EUCLASE_H
