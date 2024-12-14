#ifndef EUCLASE_H
#define EUCLASE_H

#ifdef USE_QT
#include <QImage>
#endif

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <atomic>
#include <utility>
#include <cassert>
#include <functional>
#include "fp/fp.h"
#include "fp/fp.h"

namespace euclase {

class _float16_t {
public:
	uint16_t v_;
	_float16_t() = default;
	_float16_t(float f)
	{
		v_ = fp32_to_fp16(f);
	}
	_float16_t operator = (float f)
	{
		v_ = fp32_to_fp16(f);
		return *this;
	}
	operator float () const
	{
		return fp16_to_fp32(v_);
	}
	_float16_t sqrt() const
	{
		_float16_t r;
		r.v_ = fp16_sqrt(v_);
		return r;
	}

	_float16_t pow2() const
	{
		_float16_t r;
		r.v_ = fp16_pow2(v_);
		return r;
	}
};

static inline _float16_t operator + (const _float16_t &a, const _float16_t &b)
{
	return (float)a + (float)b;
}

static inline _float16_t operator - (const _float16_t &a, const _float16_t &b)
{
	return (float)a - (float)b;
}

static inline _float16_t operator * (const _float16_t &a, const _float16_t &b)
{
	return (float)a * (float)b;
}

static inline _float16_t operator / (const _float16_t &a, const _float16_t &b)
{
	return (float)a / (float)b;
}

static inline float operator + (const _float16_t &a, float b)
{
	return (float)a + b;
}

static inline float operator - (const _float16_t &a, float b)
{
	return (float)a - b;
}

static inline float operator * (const _float16_t &a, float b)
{
	return (float)a * b;
}

static inline float operator / (const _float16_t &a, float b)
{
	return (float)a / b;
}

static inline float operator + (float a, const _float16_t &b)
{
	return a + (float)b;
}

static inline float operator - (float a, const _float16_t &b)
{
	return a - (float)b;
}

static inline float operator * (float a, const _float16_t &b)
{
	return a * (float)b;
}

static inline float operator / (float a, const _float16_t &b)
{
	return a / (float)b;
}

class Size {
private:
	int w_ = 0;
	int h_ = 0;
public:
	Size() = default;
	Size(int w, int h)
		: w_(w)
		, h_(h)
	{
	}
#ifdef USE_QT
	Size(QSize const &sz)
		: w_(sz.width())
		, h_(sz.height())
	{
	}
	operator QSize () const
	{
		return {w_, h_};
	}
#endif
	int width() const
	{
		return w_;
	}
	int height() const
	{
		return h_;
	}
};

class SizeF {
private:
	double w_ = 0;
	double h_ = 0;
public:
	SizeF() = default;
	SizeF(double w, double h)
		: w_(w)
		, h_(h)
	{
	}
#ifdef USE_QT
	SizeF(QSizeF const &sz)
		: w_(sz.width())
		, h_(sz.height())
	{
	}
	operator QSizeF () const
	{
		return {w_, h_};
	}
#endif
	double width() const
	{
		return w_;
	}
	double height() const
	{
		return h_;
	}
};

class Point {
private:
	int x_ = 0;
	int y_ = 0;
public:
	Point() = default;
	Point(int x, int y)
		: x_(x)
		, y_(y)
	{
	}
#ifdef USE_QT
	Point(QPoint const &pt)
		: x_(pt.x())
		, y_(pt.y())
	{
	}
	operator QPoint () const
	{
		return {x_, y_};
	}
#endif
	int x() const
	{
		return x_;
	}
	int y() const
	{
		return y_;
	}
	int &rx()
	{
		return x_;
	}
	int &ry()
	{
		return y_;
	}
};

class PointF {
private:
	double x_ = 0;
	double y_ = 0;
public:
	PointF() = default;
	PointF(double x, double y)
		: x_(x)
		, y_(y)
	{
	}
#ifdef USE_QT
	PointF(QPointF const &pt)
		: x_(pt.x())
		, y_(pt.y())
	{
	}
	operator QPointF () const
	{
		return {x_, y_};
	}
#endif
	double x() const
	{
		return x_;
	}
	double y() const
	{
		return y_;
	}
	double &rx()
	{
		return x_;
	}
	double &ry()
	{
		return y_;
	}
};

enum class k {
	transparent,
	black,
	white,
};

class Color {
private:
	uint8_t r = 0;
	uint8_t g = 0;
	uint8_t b = 0;
	uint8_t a = 255;
public:
	Color(euclase::k s)
	{
		switch (s) {
		case k::transparent:
			a = 0;
			return;
		case k::black:
			r = 0;
			g = 0;
			b = 0;
			a = 255;
			return;
		case k::white:
			r = 255;
			g = 255;
			b = 255;
			a = 255;
			return;
		}
	}
	Color(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
		: r(r)
		, g(g)
		, b(b)
		, a(a)
	{
	}
#ifdef USE_QT
	Color(QColor const &color)
		: r(color.red())
		, g(color.green())
		, b(color.blue())
	{
	}
#endif
	uint8_t red() const
	{
		return r;
	}
	uint8_t green() const
	{
		return g;
	}
	uint8_t blue() const
	{
		return b;
	}
	uint8_t alpha() const
	{
		return a;
	}
};

class RefCounter {
private:
	std::atomic_uint32_t ref = {};
public:
	RefCounter() = default;
	RefCounter(RefCounter const &r)
	{
		ref.store(r.ref.load());
	}
	void operator = (RefCounter const &r)
	{
		ref.store(r.ref.load());
	}
	operator unsigned int () const
	{
		return ref;
	}
	void operator ++ (int)
	{
		ref++;
	}
	void operator -- (int)
	{
		ref--;
	}
};

template <typename T>
static inline T clamp(T a, T min, T max)
{
	return std::max(min, std::min(max, a));
}

inline float clamp_f32(float a)
{
	a = floorf(a * 1000000.0f + 0.5f) / 1000000.0f;
	return std::max(0.0f, std::min(1.0f, a));
}

inline float clamp_f16(_float16_t a)
{
	return clamp_f32((float)a);
}

static inline int gray(int r, int g, int b)
{
	return (306 * r + 601 * g + 117 * b) / 1024;
}

static inline float grayf(float r, float g, float b)
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

static inline _float16_t gamma(_float16_t v)
{
	return v.sqrt();
}

static inline _float16_t degamma(_float16_t v)
{
	return v.pow2();
}

class Float32RGB;
class Float32RGBA;
class Float32Gray;
class Float32GrayA;
class Float16RGB;
class Float16RGBA;
class Float16Gray;
class Float16GrayA;
class OctetRGB;
class OctetRGBA;
class OctetGray;
class OctetGrayA;

class OctetRGB {
public:
	uint8_t r, g, b;
	OctetRGB()
		: r(0)
		, g(0)
		, b(0)
	{
	}
	OctetRGB(uint8_t r, uint8_t g, uint8_t b)
		: r(r)
		, g(g)
		, b(b)
	{
	}
	inline OctetRGB(OctetGrayA const &t);
	uint8_t gray() const
	{
		return euclase::gray(r, g, b);
	}
	static OctetRGB convert(OctetGray const &t);
	static OctetRGB convert(Float32RGB const &t);
};

class OctetRGBA {
public:
	uint8_t r, g, b, a;
	OctetRGBA()
		: r(0)
		, g(0)
		, b(0)
		, a(0)
	{
	}
	OctetRGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
		: r(r)
		, g(g)
		, b(b)
		, a(a)
	{
	}
	inline OctetRGBA(OctetGrayA const &t);
	uint8_t gray() const
	{
		return euclase::gray(r, g, b);
	}
	static OctetRGBA convert(Float32RGBA const &t);
};

class OctetGray {
public:
	uint8_t v;
	OctetGray()
		: v(0)
	{
	}
	explicit OctetGray(uint8_t l)
		: v(l)
	{
	}
	static OctetGray convert(OctetGray const &r, bool ignored)
	{
		(void)ignored;
		return r;
	}
	explicit inline OctetGray(OctetRGBA const &t);
	static inline OctetGray convert(OctetRGB const &t);
	static inline OctetGray convert(Float32RGBA const &t);
	static inline OctetGray convert(OctetRGBA const &r)
	{
		auto y = ::euclase::gray(r.r, r.g, r.b);
		return OctetGray(uint8_t((y * r.a + 128) / 255));
	}
	static OctetGray convert(OctetGrayA const &r);

	uint8_t gray() const
	{
		return v;
	}
};

class OctetGrayA {
public:
	uint8_t v, a;
	OctetGrayA()
		: v(0)
		, a(0)
	{
	}
	explicit OctetGrayA(uint8_t v, uint8_t a = 255)
		: v(v)
		, a(a)
	{
	}
	static OctetGrayA convert(OctetGrayA const &r, bool ignored)
	{
		(void)ignored;
		return r;
	}
	inline OctetGrayA(OctetRGBA const &t);
	uint8_t gray() const
	{
		return v;
	}
	static inline OctetGrayA convert(Float32RGBA const &t);
	static inline OctetGrayA convert(Float32RGB const &t);
	static inline OctetGrayA convert(Float32GrayA const &t);
	static inline OctetGrayA convert(Float32Gray const &t);
	static inline OctetGrayA convert(OctetRGBA const &t);
	static inline OctetGrayA convert(OctetRGB const &t);
	static inline OctetGrayA convert(OctetGray const &t);
};

inline OctetRGBA::OctetRGBA(OctetGrayA const &t)
	: r(t.v)
	, g(t.v)
	, b(t.v)
	, a(t.a)
{
}

inline OctetGrayA::OctetGrayA(OctetRGBA const &t)
	: v(euclase::gray(t.r, t.g, t.b))
	, a(t.a)
{
}

class Float32RGB {
public:
	float r;
	float g;
	float b;
	Float32RGB()
		: r(0)
		, g(0)
		, b(0)
	{
	}
	Float32RGB(float r, float g, float b)
		: r(r)
		, g(g)
		, b(b)
	{
	}
	static Float32RGB convert(OctetRGBA const &src)
	{
		float r = degamma(src.r / 255.0f);
		float g = degamma(src.g / 255.0f);
		float b = degamma(src.b / 255.0f);
		return {r, g, b};
	}
	static Float32RGB convert(OctetGrayA const &src)
	{
		float v = degamma(src.v / 255.0f);
		return {v, v, v};
	}
	Float32RGB operator + (Float32RGB const &right) const
	{
		return Float32RGB(r + right.r, g + right.g, b + right.b);
	}
	Float32RGB operator * (float t) const
	{
		return Float32RGB(r * t, g * t, b * t);
	}
	void operator += (Float32RGB const &o)
	{
		r += o.r;
		g += o.g;
		b += o.b;
	}
	void add(Float32RGB const &p, float n)
	{
		r += p.r * n;
		g += p.g * n;
		b += p.b * n;
	}
	void sub(Float32RGB const &p, float n)
	{
		r -= p.r * n;
		g -= p.g * n;
		b -= p.b * n;
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
		return (uint8_t)floor(r * 255 + 0.5f);
	}
	uint8_t g8() const
	{
		if (g <= 0) return 0;
		if (g >= 1) return 255;
		return (uint8_t)floor(g * 255 + 0.5f);
	}
	uint8_t b8() const
	{
		if (b <= 0) return 0;
		if (b >= 1) return 255;
		return (uint8_t)floor(b * 255 + 0.5f);
	}
	Float32RGB limit() const
	{
		return Float32RGB(clamp_f32(r), clamp_f32(g), clamp_f32(b));
	}
	Float32RGB color(float amount) const
	{
		if (amount == 1) {
			return *this;
		} else if (amount == 0) {
			return Float32RGB(0, 0, 0);
		}
		float m = 1.0f / amount;
		Float32RGB t = *this;
		t.r *= m;
		t.g *= m;
		t.b *= m;
		return t.limit();
	}
	operator OctetRGBA () const
	{
		return OctetRGBA(r8(), g8(), b8());
	}
};

class Float16RGB {
public:
	_float16_t r;
	_float16_t g;
	_float16_t b;
	Float16RGB()
		: r(0)
		, g(0)
		, b(0)
	{
	}
	Float16RGB(_float16_t r, _float16_t g, _float16_t b)
		: r(r)
		, g(g)
		, b(b)
	{
	}
	static Float16RGB convert(OctetRGBA const &src)
	{
		_float16_t r = degamma(src.r / 255.0f);
		_float16_t g = degamma(src.g / 255.0f);
		_float16_t b = degamma(src.b / 255.0f);
		return {r, g, b};
	}
	static Float16RGB convert(OctetGrayA const &src)
	{
		_float16_t v = degamma(src.v / 255.0f);
		return {v, v, v};
	}
	Float16RGB operator + (Float16RGB const &right) const
	{
		return Float16RGB(r + right.r, g + right.g, b + right.b);
	}
	Float16RGB operator * (_float16_t t) const
	{
		return Float16RGB(r * t, g * t, b * t);
	}
	void operator += (Float16RGB const &o)
	{
		r = (float)r + (float)o.r;
		g = (float)g + (float)o.g;
		b = (float)b + (float)o.b;
	}
	void add(Float16RGB const &p, _float16_t n)
	{
		r = (float)r + (float)p.r * (float)n;
		g = (float)g + (float)p.g * (float)n;
		b = (float)b + (float)p.b * (float)n;
	}
	void sub(Float16RGB const &p, _float16_t n)
	{
		r = (float)r - (float)p.r * (float)n;
		g = (float)g - (float)p.g * (float)n;
		b = (float)b - (float)p.b * (float)n;
	}

	void operator *= (_float16_t t)
	{
		r = (float)r * (float)t;
		g = (float)g * (float)t;
		b = (float)b * (float)t;
	}
	uint8_t r8() const
	{
		if ((float)r <= 0) return 0;
		if ((float)r >= 1) return 255;
		return (uint8_t)floorf((float)r * 255 + 0.5f);
	}
	uint8_t g8() const
	{
		if ((float)g <= 0) return 0;
		if ((float)g >= 1) return 255;
		return (uint8_t)floorf((float)g * 255 + 0.5f);
	}
	uint8_t b8() const
	{
		if ((float)b <= 0) return 0;
		if ((float)b >= 1) return 255;
		return (uint8_t)floorf((float)b * 255 + 0.5f);
	}
	Float16RGB limit() const
	{
		return Float16RGB(clamp_f16(r), clamp_f16(g), clamp_f16(b));
	}
	Float16RGB color(_float16_t amount) const
	{
		if (amount == 1) {
			return *this;
		} else if (amount == 0) {
			return Float16RGB(0, 0, 0);
		}
		float m = 1.0f / (float)amount;
		Float16RGB t = *this;
		t.r = (float)t.r * m;
		t.g = (float)t.g * m;
		t.b = (float)t.b * m;
		return t.limit();
	}
	operator OctetRGBA () const
	{
		return OctetRGBA(r8(), g8(), b8());
	}
};

class Float32Gray {
public:
	float v;
	Float32Gray()
		: v(0)
	{
	}
	Float32Gray(float y)
		: v(y)
	{
	}
	static Float32Gray convert(OctetGrayA const &src)
	{
		return {src.v / 255.0f};
	}
	static Float32Gray convert(OctetGray const &src)
	{
		return {src.v / 255.0f};
	}
	static inline Float32Gray convert(OctetRGBA const &r);
	Float32Gray operator + (Float32Gray const &right) const
	{
		return Float32Gray(v + right.v);
	}
	Float32Gray operator * (float t) const
	{
		return Float32Gray(v * t);
	}
	void operator += (Float32Gray const &o)
	{
		v += o.v;
	}
	void add(Float32Gray const &p, float n)
	{
		v += p.v * n;
	}
	void sub(Float32Gray const &p, float n)
	{
		v -= p.v * n;
	}

	void operator *= (float t)
	{
		v *= t;
	}
	uint8_t y8() const
	{
		if (v <= 0) return 0;
		if (v >= 1) return 255;
		return (uint8_t)floor(gamma(v) * 255 + 0.5f);
	}
	Float32Gray limit() const
	{
		return Float32Gray(clamp_f32(v));
	}
	Float32Gray color(float amount) const
	{
		if (amount == 1) {
			return *this;
		} else if (amount == 0) {
			return Float32Gray(0);
		}
		float m = 1 / amount;
		Float32Gray t = *this * m;
		return t.limit();
	}
	OctetGray toPixelGray() const
	{
		return OctetGray(y8());
	}
	static inline Float32Gray convert(Float32RGBA const &t);
};

class Float16Gray {
public:
	_float16_t v;
	Float16Gray()
		: v(0)
	{
	}
	Float16Gray(_float16_t y)
		: v(y)
	{
	}
	static Float16Gray convert(OctetGrayA const &src)
	{
		return {src.v / 255.0f};
	}
	static Float16Gray convert(OctetGray const &src)
	{
		return {src.v / 255.0f};
	}
	static inline Float16Gray convert(OctetRGBA const &r);
	Float16Gray operator + (Float16Gray const &right) const
	{
		return Float16Gray(v + right.v);
	}
	Float16Gray operator * (_float16_t t) const
	{
		return Float16Gray(v * t);
	}
	void operator += (Float16Gray const &o)
	{
		v = (float)v + (float)o.v;
	}
	void add(Float16Gray const &p, _float16_t n)
	{
		v = (float)v + (float)p.v * (float)n;
	}
	void sub(Float16Gray const &p, _float16_t n)
	{
		v = (float)v - (float)p.v * (float)n;
	}

	void operator *= (_float16_t t)
	{
		v = (float)v * (float)t;
	}
	uint8_t y8() const
	{
		if ((float)v <= 0) return 0;
		if ((float)v >= 1) return 255;
		return (uint8_t)floorf((float)v * 255 + 0.5f);
	}
	Float16Gray limit() const
	{
		return Float16Gray(clamp_f16(v));
	}
	Float16Gray color(_float16_t amount) const
	{
		if (amount == 1) {
			return *this;
		} else if (amount == 0) {
			return Float16Gray(0);
		}
		Float16Gray t((float)v / (float)amount);
		return t.limit();
	}
	OctetGray toPixelGray() const
	{
		return OctetGray(y8());
	}
	static inline Float16Gray convert(Float16RGBA const &t);
};

class Float32RGBA {
public:
	float r;
	float g;
	float b;
	float a;
	Float32RGBA()
		: r(0)
		, g(0)
		, b(0)
		, a(0)
	{
	}
	explicit Float32RGBA(float r, float g, float b, float a = 1)
		: r(r)
		, g(g)
		, b(b)
		, a(a)
	{
	}
	explicit Float32RGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 1)
	{
		this->r = r / 255.0f;
		this->g = g / 255.0f;
		this->b = b / 255.0f;
		this->a = a / 255.0f;
	}
	static inline Float32RGBA convert(OctetRGBA const &src);
	static inline Float32RGBA convert(Float16RGBA const &src);
	Float32RGBA operator + (Float32RGBA const &right) const
	{
		return Float32RGBA(r + right.r, g + right.g, b + right.b);
	}
	Float32RGBA operator * (float t) const
	{
		return Float32RGBA(r * t, g * t, b * t);
	}
	void operator += (Float32RGBA const &o)
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
	void add(Float32RGBA const &p, float n)
	{
		n *= p.a;
		a += n;
		r += p.r * n;
		g += p.g * n;
		b += p.b * n;
	}
	void sub(Float32RGBA const &p, float n)
	{
		n *= p.a;
		a -= n;
		r -= p.r * n;
		g -= p.g * n;
		b -= p.b * n;
	}
	uint8_t r8() const
	{
		if (r <= 0) return 0;
		if (r >= 1) return 255;
		return (uint8_t)floorf(r * 255 + 0.5f);
	}
	uint8_t g8() const
	{
		if (g <= 0) return 0;
		if (g >= 1) return 255;
		return (uint8_t)floorf(g * 255 + 0.5f);
	}
	uint8_t b8() const
	{
		if (b <= 0) return 0;
		if (b >= 1) return 255;
		return (uint8_t)floorf(b * 255 + 0.5f);
	}
	uint8_t a8() const
	{
		if (a <= 0) return 0;
		if (a >= 1) return 255;
		return (uint8_t)floorf(a * 255 + 0.5f);
	}
	Float32RGBA limit() const
	{
		return Float32RGBA(clamp_f32(r), clamp_f32(g), clamp_f32(b), clamp_f32(a));
	}
	Float32RGBA color(float amount) const
	{
		if (amount == 0) {
			return Float32RGBA(0.0f, 0.0f, 0.0f, 0.0f);
		}
		Float32RGBA t(*this);
		t *= (1.0f / t.a);
		t.a = a / amount;
		return t;
	}
	operator OctetRGBA () const
	{
		return OctetRGBA(r8(), g8(), b8(), a8());
	}
};

class Float16RGBA {
public:
	_float16_t r;
	_float16_t g;
	_float16_t b;
	_float16_t a;
	Float16RGBA()
		: r(0)
		, g(0)
		, b(0)
		, a(0)
	{
	}
	explicit Float16RGBA(_float16_t r, _float16_t g, _float16_t b, _float16_t a = 1)
		: r(r)
		, g(g)
		, b(b)
		, a(a)
	{
	}
	static inline Float16RGBA convert(OctetRGBA const &src);
	static inline Float16RGBA convert(Float32RGBA const &src);
	Float16RGBA operator + (Float16RGBA const &right) const
	{
		return Float16RGBA(r + right.r, g + right.g, b + right.b);
	}
	Float16RGBA operator * (_float16_t t) const
	{
		return Float16RGBA(r * t, g * t, b * t);
	}
	void operator += (Float16RGBA const &o)
	{
		r = (float)r + (float)o.r;
		g = (float)g + (float)o.g;
		b = (float)b + (float)o.b;
	}
	void operator *= (_float16_t t)
	{
		r = (float)r * (float)t;
		g = (float)g * (float)t;
		b = (float)b * (float)t;
	}
	void add(Float16RGBA const &p, _float16_t n)
	{
		n = n * (float)p.a;
		a = (float)a + (float)n;
		r = (float)r + (float)p.r * (float)n;
		g = (float)g + (float)p.g * (float)n;
		b = (float)b + (float)p.b * (float)n;
	}
	void sub(Float16RGBA const &p, _float16_t n)
	{
		n = n * (float)p.a;
		a = (float)a - (float)n;
		r = (float)r - (float)p.r * (float)n;
		g = (float)g - (float)p.g * (float)n;
		b = (float)b - (float)p.b * (float)n;
	}
	uint8_t r8() const
	{
		if (r <= 0) return 0;
		if (r >= 1) return 255;
		return (uint8_t)floorf(r * 255.0f + 0.5f);
	}
	uint8_t g8() const
	{
		if ((float)g <= 0) return 0;
		if ((float)g >= 1) return 255;
		return (uint8_t)floorf(g * 255.0f + 0.5f);
	}
	uint8_t b8() const
	{
		if ((float)b <= 0) return 0;
		if ((float)b >= 1) return 255;
		return (uint8_t)floorf(b * 255.0f + 0.5f);
	}
	uint8_t a8() const
	{
		if ((float)a <= 0) return 0;
		if ((float)a >= 1) return 255;
		return (uint8_t)floorf(a * 255.0f + 0.5f);
	}
	Float16RGBA limit() const
	{
		return Float16RGBA(clamp_f16(r), clamp_f16(g), clamp_f16(b), clamp_f16(a));
	}
	Float16RGBA color(_float16_t amount) const
	{
		if (amount == 0) {
			return Float16RGBA(0.0f, 0.0f, 0.0f, 0.0f);
		}
		Float16RGBA t(*this);
		t *= (1.0f / t.a);
		t.a = a / amount;
		return t;
	}
	operator OctetRGBA () const
	{
		return OctetRGBA(r8(), g8(), b8(), a8());
	}
};

inline OctetRGB OctetRGB::convert(OctetGray const &t)
{
	return OctetRGB(t.v, t.v, t.v);
}

inline OctetRGB OctetRGB::convert(Float32RGB const &t)
{
	auto u8 = [](float v){
		return (uint8_t)clamp(floorf(gamma(v) * 255 + 0.5), 0.0f, 255.0f);
	};
	auto r = u8(t.r);
	auto g = u8(t.g);
	auto b = u8(t.b);
	return OctetRGB(r, g, b);
}

inline OctetRGBA OctetRGBA::convert(Float32RGBA const &t)
{
	auto u8 = [](float v){
		return (uint8_t)clamp(floorf(gamma(v) * 255 + 0.5), 0.0f, 255.0f);
	};
	auto r = u8(t.r);
	auto g = u8(t.g);
	auto b = u8(t.b);
	return OctetRGBA(r, g, b, t.a8());
}

class Float32GrayA {
public:
	float v;
	float a;
	Float32GrayA()
		: v(0)
		, a(0)
	{
	}
	Float32GrayA(float v, float a = 1)
		: v(v)
		, a(a)
	{
	}
	static Float32GrayA convert(OctetGrayA const &src)
	{
		float l = src.v / 255.0;
		return {l, float(src.a / 255.0)};
	}
	static Float32GrayA convert(OctetGray const &src)
	{
		float l = src.v / 255.0;
		return {l, 255};
	}
	Float32GrayA operator + (Float32GrayA const &right) const
	{
		return Float32GrayA(v + right.v);
	}
	Float32GrayA operator * (float t) const
	{
		return Float32GrayA(v * t);
	}
	void operator += (Float32GrayA const &o)
	{
		v += o.v;
	}
	void operator *= (float t)
	{
		v *= t;
	}
	void add(Float32GrayA const &p, float n)
	{
		n *= p.a;
		a += n;
		v += p.v * n;
	}
	void sub(Float32GrayA const &p, float n)
	{
		n *= p.a;
		a -= n;
		v -= p.v * n;
	}
	uint8_t v8() const
	{
		if (v <= 0) return 0;
		if (v >= 1) return 255;
		return clamp(int(gamma(v) * 255 + 0.5f), 0, 255);
	}
	uint8_t a8() const
	{
		if (a <= 0) return 0;
		if (a >= 1) return 255;
		return (uint8_t)floor(a * 255 + 0.5f);
	}
	Float32GrayA limit() const
	{
		return Float32GrayA(clamp_f32(v), clamp_f32(a));
	}
	Float32GrayA color(float amount) const
	{
		if (amount == 0) {
			return Float32GrayA(0, 0);
		}
		Float32GrayA t(*this);
		t *= (1.0f / t.a);
		t.a = a / amount;
		return Float32GrayA(t.v, t.a).limit();
	}
};

class Float16GrayA {
public:
	_float16_t v;
	_float16_t a;
	Float16GrayA()
		: v(0)
		, a(0)
	{
	}
	Float16GrayA(_float16_t v, _float16_t a = 1)
		: v(v)
		, a(a)
	{
	}
	static Float16GrayA convert(OctetGrayA const &src)
	{
		_float16_t l = src.v / 255.0;
		return {l, _float16_t(src.a / 255.0)};
	}
	static Float16GrayA convert(OctetGray const &src)
	{
		_float16_t l = src.v / 255.0;
		return {l, 255};
	}
	Float16GrayA operator + (Float16GrayA const &right) const
	{
		return Float16GrayA(v + right.v);
	}
	Float16GrayA operator * (_float16_t t) const
	{
		return Float16GrayA(v * t);
	}
	void operator += (Float16GrayA const &o)
	{
		v = (float)v + (float)o.v;
	}
	void operator *= (_float16_t t)
	{
		v = (float)v * (float)t;
	}
	void add(Float16GrayA const &p, _float16_t n)
	{
		n = n * (float)p.a;
		a = (float)a + (float)n;
		v = (float)v + (float)p.v * (float)n;
	}
	void sub(Float16GrayA const &p, _float16_t n)
	{
		n = n * (float)p.a;
		a = (float)a - (float)n;
		v = (float)v - (float)p.v * (float)n;
	}
	uint8_t v8() const
	{
		if ((float)v <= 0) return 0;
		if ((float)v >= 1) return 255;
		return (uint8_t)floorf((float)v * 255 + 0.5f);
	}
	uint8_t a8() const
	{
		if ((float)a <= 0) return 0;
		if ((float)a >= 1) return 255;
		return (uint8_t)floorf((float)a * 255 + 0.5f);
	}
	Float16GrayA limit() const
	{
		return Float16GrayA(clamp_f16(v), clamp_f16(a));
	}
	Float16GrayA color(_float16_t amount) const
	{
		if (amount == 0) {
			return Float16GrayA(0, 0);
		}
		Float16GrayA t(*this);
		t *= (1.0f / (float)t.a);
		t.a = (float)a / (float)amount;
		return Float16GrayA(t.v, t.a).limit();
	}
};

template <typename D, typename S> D pixel_cast(S const &src);
template <> inline OctetRGBA pixel_cast<OctetRGBA>(OctetRGBA const &src)
{
	return src;
}
template <> inline Float32RGB pixel_cast<Float32RGB>(Float32RGB const &src)
{
	return src;
}
template <> inline Float32RGBA pixel_cast<Float32RGBA>(Float32RGBA const &src)
{
	return src;
}
template <> inline OctetRGBA pixel_cast<OctetRGBA>(Float32RGBA const &src)
{
	return OctetRGBA::convert(src);
}
template <> inline Float32Gray pixel_cast<Float32Gray>(Float32Gray const &src)
{
	return src;
}
template <> inline Float32GrayA pixel_cast<Float32GrayA>(Float32GrayA const &src)
{
	return src;
}
template <> inline Float32GrayA pixel_cast<Float32GrayA>(OctetGrayA const &src)
{
	return Float32GrayA::convert(src);
}

static inline Float32RGBA gamma(Float32RGBA const &pix)
{
	return Float32RGBA(gamma(pix.r), gamma(pix.g), gamma(pix.b), pix.a);
}

static inline Float32RGBA degamma(Float32RGBA const &pix)
{
	return Float32RGBA(degamma(pix.r), degamma(pix.g), degamma(pix.b), pix.a);
}


static inline Float16RGBA gamma(Float16RGBA const &pix)
{
	return Float16RGBA(gamma(pix.r), gamma(pix.g), gamma(pix.b), pix.a);
}

static inline Float16RGBA degamma(Float16RGBA const &pix)
{
	return Float16RGBA(degamma(pix.r), degamma(pix.g), degamma(pix.b), pix.a);
}


Float32RGBA Float32RGBA::convert(OctetRGBA const &src)
{
	float r = src.r / 255.0f;
	float g = src.g / 255.0f;
	float b = src.b / 255.0f;
	float a = src.a / 255.0f;
	return degamma(Float32RGBA(r, g, b, a));
}

Float32RGBA Float32RGBA::convert(Float16RGBA const &src)
{
	return Float32RGBA((float)src.r, (float)src.g, (float)src.b, (float)src.a);
}

Float16RGBA Float16RGBA::convert(OctetRGBA const &src)
{
	float r = src.r / 255.0f;
	float g = src.g / 255.0f;
	float b = src.b / 255.0f;
	float a = src.a / 255.0f;
	return degamma(Float16RGBA(r, g, b, a));
}

Float16RGBA Float16RGBA::convert(Float32RGBA const &src)
{
	return Float16RGBA(src.r, src.g, src.b, src.a);
}

Float32Gray Float32Gray::convert(Float32RGBA const &t)
{
	return Float32Gray(grayf(t.r, t.g, t.b) * t.a);
}

Float16Gray Float16Gray::convert(Float16RGBA const &t)
{
	return Float16Gray(grayf(t.r, t.g, t.b) * t.a);
}

OctetGray OctetGray::convert(OctetRGB const &t)
{
	return OctetGray(::euclase::gray(t.r, t.g, t.b));
}

OctetGray OctetGray::convert(Float32RGBA const &t)
{
//	auto s = degamma(r);
	auto y = ::euclase::gray(t.r8(), t.g8(), t.b8());
	y = gamma(y);
	return OctetGray(uint8_t(y * t.a));
}

inline OctetGray OctetGray::convert(const OctetGrayA &r)
{
	return OctetGray(r.v);
}

Float32Gray Float32Gray::convert(OctetRGBA const &r)
{
	auto s = Float32RGBA::convert(r);
	return Float32Gray(grayf(s.r, s.g, s.b) * s.a);
}

OctetGrayA OctetGrayA::convert(const Float32RGBA &t)
{
	return OctetGrayA(::euclase::gray(t.r, t.g, t.b), 255);
}

OctetGrayA OctetGrayA::convert(const Float32RGB &t)
{
	float y = ::euclase::grayf(t.r, t.g, t.b);
	return OctetGrayA(clamp(int(gamma(y) * 255 + 0.5), 0, 255), 255);
}

OctetGrayA OctetGrayA::convert(const Float32GrayA &t)
{
	int y = t.v8();
	int a = clamp(int(t.a * 255 + 0.5), 0, 255);
	return OctetGrayA(y, a);
}

OctetGrayA OctetGrayA::convert(const Float32Gray &t)
{
	return OctetGrayA(clamp(int(gamma(t.v) * 255 + 0.5), 0, 255), 255);
}

OctetGrayA OctetGrayA::convert(const OctetRGBA &t)
{
	uint8_t v = ::euclase::gray(t.r, t.g, t.b);
	return OctetGrayA(v, t.a);
}

OctetGrayA OctetGrayA::convert(const OctetRGB &t)
{
	uint8_t v = ::euclase::gray(t.r, t.g, t.b);
	return OctetGrayA(v, 255);
}

OctetGrayA OctetGrayA::convert(const OctetGray &t)
{
	return OctetGrayA(t.gray(), 255);
}


template <typename T> static inline T limit(T const &t)
{
	return t.limit();
}

// image

class Image {
public:
	enum Format {
		Format_Invalid,
		Format_U8_RGB,
		Format_U8_RGBA,
		Format_U8_Grayscale,
		Format_U8_GrayscaleA,
		Format_F32_RGB,
		Format_F32_RGBA,
		Format_F32_Grayscale,
		Format_F32_GrayscaleA,
		Format_F16_RGB,
		Format_F16_RGBA,
		Format_F16_Grayscale,
		Format_F16_GrayscaleA,
	};
	enum MemoryType {
		Host,
		CUDA,
	};
private:
	struct Data {
		RefCounter ref;
		Image::Format format_ = Image::Format_U8_RGBA;
		int width_ = 0;
		int height_ = 0;
		MemoryType memtype_ = Host;
		void *cudamem_ = nullptr;
		Data() = default;
		uint8_t *data()
		{
			switch (memtype_) {
			case Host:
				return ((uint8_t *)this + sizeof(Data));
#ifdef USE_CUDA
			case CUDA:
				return (uint8_t *)cudamem_;
#endif
			}
			assert(0);
			return nullptr;
		}
		uint8_t const *data() const
		{
			return const_cast<Data *>(this)->data();
		}
	};
private:
	Data *ptr_ = nullptr;
	void assign(Data *p);
	void init(int w, int h, Image::Format format, MemoryType memtype = Host, Color const &color = k::transparent);
public:
	Image() = default;
	Image(Image const &r)
	{
		assign(r.ptr_);
	}
	Image &operator = (Image const &r)
	{
		assign(r.ptr_);
		return *this;
	}
	~Image()
	{
		assign(nullptr);
	}

	Image(int width, int height, Image::Format format, MemoryType memtype = Host);

	void assign(Image const &img)
	{
		assign(img.ptr_);
	}

	Image copy(MemoryType memtype = (MemoryType)-1) const;
	Image &memconvert(MemoryType memtype);
	Image toHost() const
	{
		if (memtype() == Host) return *this;
		return copy(Host);
	}
#ifdef USE_CUDA
	Image toCUDA() const
	{
		if (memtype() == CUDA) return *this;
		return copy(CUDA);
	}
#endif
#ifdef USE_QT
	Image(QImage const &image)
	{
		setImage(image);
	}
	void setImage(QImage const &image);
	QImage qimage() const;
#endif

	bool isNull() const
	{
		return !ptr_;
	}

	operator bool () const
	{
		return !isNull();
	}

	MemoryType memtype() const
	{
		return ptr_ ? ptr_->memtype_: Host;
	}

	Image::Format format() const
	{
		return ptr_ ? ptr_->format_ : Image::Format_Invalid;
	}

	void make(int width, int height, Image::Format format, MemoryType memtype = Host, euclase::Color const &color = k::transparent)
	{
		assign(nullptr);
		init(width, height, format, memtype, color);
	}

	uint8_t *data()
	{
		return ptr_ ? ptr_->data() : nullptr;
	}
	uint8_t const *data() const
	{
		return const_cast<Image *>(this)->data();
	}
	uint8_t *scanLine(int y);
	uint8_t const *scanLine(int y) const;

	void fill(const Color &color);

	Image scaled(int w, int h, bool smooth) const;

	int width() const
	{
		return ptr_ ? ptr_->width_ : 0;
	}

	int height() const
	{
		return ptr_ ? ptr_->height_ : 0;
	}

	Size size() const;

	int bytesPerPixel() const;
	int bytesPerLine() const
	{
		return bytesPerPixel() * width();
	}

	Image convertToFormat(Image::Format newformat) const;
	Image makeFPImage() const;

	void swap(Image &other);
};

#ifdef USE_QT
QImage::Format qimageformat(Image::Format format);
#endif

int bytesPerPixel(Image::Format format);

inline int Image::bytesPerPixel() const
{
	return euclase::bytesPerPixel(format());
}

inline uint8_t *Image::scanLine(int y)
{
	return ptr_ ? (ptr_->data() + bytesPerPixel() * width() * y) : nullptr;
}

inline uint8_t const *Image::scanLine(int y) const
{
	return const_cast<Image *>(this)->scanLine(y);
}

// cubic bezier curve

double cubicBezierPoint(double p0, double p1, double p2, double p3, double t);
double cubicBezierGradient(double p0, double p1, double p2, double p3, double t);
PointF cubicBezierPoint(const PointF &p0, const PointF &p1, const PointF &p2, const PointF &p3, double t);
void cubicBezierSplit(PointF *p0, PointF *p1, PointF *p2, PointF *p3, PointF *q0, PointF *q1, PointF *q2, PointF *q3, double t);

//

struct FloatHSV {
	float h = 0; // 0..1
	float s = 0; // 0..1
	float v = 0; // 0..1
	FloatHSV() = default;
	FloatHSV(float h, float s, float v)
		: h(h)
		, s(s)
		, v(v)
	{
	}
};

FloatHSV rgb_to_hsv(Float32RGB const &rgb);
Float32RGB hsv_to_rgb(FloatHSV const &hsv);

enum class EnlargeMethod {
	NearestNeighbor,
	Bilinear,
	Bicubic,
};
euclase::Image resizeImage(euclase::Image const &image, int dst_w, int dst_h, EnlargeMethod method/* = EnlargeMethod::Bilinear*/);
euclase::Image filter_blur(euclase::Image image, int radius, bool *cancel, std::function<void (float)> progress);

#ifdef USE_EUCLASE_IMAGE_READ_WRITE
std::optional<Image> load_jpeg(char const *path);
std::optional<Image> load_png(char const *path);
bool save_jpeg(Image const &image, char const *path);
bool save_png(Image const &image, char const *path);
#endif

} // namespace euclase

namespace std {
template <> inline void swap<euclase::Image>(euclase::Image &a, euclase::Image &b)
{
	a.swap(b);
}
}

#endif // EUCLASE_H
