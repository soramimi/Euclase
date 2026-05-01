#ifndef EUCLASE_H
#define EUCLASE_H

#ifdef USE_QT
#include <QImage>
#endif

#include "fp/fp.h"
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <functional>
#include <optional>
#include <utility>

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

	_float16_t gamma() const
	{
		_float16_t r;
		r.v_ = fp16_gamma(v_);
		return r;
	}

	_float16_t degamma() const
	{
		_float16_t r;
		r.v_ = fp16_degamma(v_);
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

constexpr float GAMMA = 2.2f;

static inline float gamma(float v)
{
	// return sqrt(v);
	return pow(v, 1.0 / GAMMA);
}

static inline float degamma(float v)
{
	// return v * v;
	return pow(v, GAMMA);
}

static inline _float16_t gamma(_float16_t v)
{
	return v.gamma();
}

static inline _float16_t degamma(_float16_t v)
{
	return v.degamma();
}

class Float32RGB;
class Float32RGBA;
class Float32Gray;
class Float32GrayA;
class Float16RGB;
class Float16RGBA;
class Float16Gray;
class Float16GrayA;
class UInt8RGB;
class UInt8RGBA;
class UInt8Gray;
class UInt8GrayA;
class UInt16RGB;
class UInt16RGBA;
class UInt16Gray;
class UInt16GrayA;

class UInt8RGB {
public:
	uint8_t r, g, b;
	UInt8RGB()
		: r(0)
		, g(0)
		, b(0)
	{
	}
	UInt8RGB(uint8_t r, uint8_t g, uint8_t b)
		: r(r)
		, g(g)
		, b(b)
	{
	}
	inline UInt8RGB(UInt8GrayA const &t);
	uint8_t gray() const
	{
		return euclase::gray(r, g, b);
	}
	static UInt8RGB convert(UInt8Gray const &t);
	static UInt8RGB convert(Float32RGB const &t);
};

class UInt16RGB {
public:
	uint16_t r, g, b;
	UInt16RGB()
		: r(0)
		, g(0)
		, b(0)
	{
	}
	UInt16RGB(uint16_t r, uint16_t g, uint16_t b)
		: r(r)
		, g(g)
		, b(b)
	{
	}
	inline UInt16RGB(UInt16GrayA const &t);
	uint16_t gray() const
	{
		return euclase::gray(r, g, b);
	}
	static UInt16RGB convert(UInt16Gray const &t);
	static UInt16RGB convert(Float32RGB const &t);
};

class UInt8RGBA {
public:
	uint8_t r, g, b, a;
	UInt8RGBA()
		: r(0)
		, g(0)
		, b(0)
		, a(0)
	{
	}
	UInt8RGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
		: r(r)
		, g(g)
		, b(b)
		, a(a)
	{
	}
	inline UInt8RGBA(UInt8GrayA const &t);
	uint8_t gray() const
	{
		return euclase::gray(r, g, b);
	}
	static UInt8RGBA convert(Float32RGBA const &t);
};

class UInt16RGBA {
public:
	uint16_t r, g, b, a;
	UInt16RGBA()
		: r(0)
		, g(0)
		, b(0)
		, a(0)
	{
	}
	UInt16RGBA(uint16_t r, uint16_t g, uint16_t b, uint16_t a = 255)
		: r(r)
		, g(g)
		, b(b)
		, a(a)
	{
	}
	inline UInt16RGBA(UInt16GrayA const &t);
	uint16_t gray() const
	{
		return euclase::gray(r, g, b);
	}
	static UInt16RGBA convert(Float32RGBA const &t);
};

class UInt8Gray {
public:
	uint8_t v;
	UInt8Gray()
		: v(0)
	{
	}
	explicit UInt8Gray(uint8_t l)
		: v(l)
	{
	}
	static UInt8Gray convert(UInt8Gray const &r, bool ignored)
	{
		(void)ignored;
		return r;
	}
	explicit inline UInt8Gray(UInt8RGBA const &t);
	static inline UInt8Gray convert(UInt8RGB const &t);
	static inline UInt8Gray convert(Float32RGBA const &t);
	static inline UInt8Gray convert(UInt8RGBA const &r)
	{
		auto y = ::euclase::gray(r.r, r.g, r.b);
		return UInt8Gray(uint8_t((y * r.a + 128) / 255));
	}
	static UInt8Gray convert(UInt8GrayA const &r);

	uint8_t gray() const
	{
		return v;
	}
};

class UInt16Gray {
public:
	uint16_t v;
	UInt16Gray()
		: v(0)
	{
	}
	explicit UInt16Gray(uint16_t l)
		: v(l)
	{
	}
	static UInt16Gray convert(UInt16Gray const &r, bool ignored)
	{
		(void)ignored;
		return r;
	}
	explicit inline UInt16Gray(UInt16RGBA const &t);
	static inline UInt16Gray convert(UInt16RGB const &t);
	static inline UInt16Gray convert(Float32RGBA const &t);
	static inline UInt16Gray convert(UInt16RGBA const &r)
	{
		auto y = ::euclase::gray(r.r, r.g, r.b);
		return UInt16Gray(uint16_t((y * r.a + 128) / 255));
	}
	static UInt16Gray convert(UInt16GrayA const &r);

	uint16_t gray() const
	{
		return v;
	}
};

class UInt8GrayA {
public:
	uint8_t v, a;
	UInt8GrayA()
		: v(0)
		, a(0)
	{
	}
	explicit UInt8GrayA(uint8_t v, uint8_t a = 255)
		: v(v)
		, a(a)
	{
	}
	static UInt8GrayA convert(UInt8GrayA const &r, bool ignored)
	{
		(void)ignored;
		return r;
	}
	inline UInt8GrayA(UInt8RGBA const &t);
	uint8_t gray() const
	{
		return v;
	}
	static inline UInt8GrayA convert(Float32RGBA const &t);
	static inline UInt8GrayA convert(Float32RGB const &t);
	static inline UInt8GrayA convert(Float32GrayA const &t);
	static inline UInt8GrayA convert(Float32Gray const &t);
	static inline UInt8GrayA convert(UInt8RGBA const &t);
	static inline UInt8GrayA convert(UInt8RGB const &t);
	static inline UInt8GrayA convert(UInt8Gray const &t);
};

class UInt16GrayA {
public:
	uint16_t v, a;
	UInt16GrayA()
		: v(0)
		, a(0)
	{
	}
	explicit UInt16GrayA(uint16_t v, uint16_t a = 255)
		: v(v)
		, a(a)
	{
	}
	static UInt16GrayA convert(UInt16GrayA const &r, bool ignored)
	{
		(void)ignored;
		return r;
	}
	inline UInt16GrayA(UInt16RGBA const &t);
	uint16_t gray() const
	{
		return v;
	}
	static inline UInt16GrayA convert(Float32RGBA const &t);
	static inline UInt16GrayA convert(Float32RGB const &t);
	static inline UInt16GrayA convert(Float32GrayA const &t);
	static inline UInt16GrayA convert(Float32Gray const &t);
	static inline UInt16GrayA convert(UInt16RGBA const &t);
	static inline UInt16GrayA convert(UInt16RGB const &t);
	static inline UInt16GrayA convert(UInt16Gray const &t);
};

inline UInt8RGBA::UInt8RGBA(UInt8GrayA const &t)
	: r(t.v)
	, g(t.v)
	, b(t.v)
	, a(t.a)
{
}

inline UInt16RGBA::UInt16RGBA(UInt16GrayA const &t)
	: r(t.v)
	, g(t.v)
	, b(t.v)
	, a(t.a)
{
}

inline UInt8GrayA::UInt8GrayA(UInt8RGBA const &t)
	: v(euclase::gray(t.r, t.g, t.b))
	, a(t.a)
{
}

inline UInt16GrayA::UInt16GrayA(UInt16RGBA const &t)
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
	static Float32RGB convert(UInt8RGBA const &src)
	{
		float r = degamma(src.r / 255.0f);
		float g = degamma(src.g / 255.0f);
		float b = degamma(src.b / 255.0f);
		return {r, g, b};
	}
	static Float32RGB convert(UInt8GrayA const &src)
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
	uint16_t r16() const
	{
		if (r <= 0) return 0;
		if (r >= 1) return 65535;
		return (uint16_t)floor(r * 65535 + 0.5f);
	}
	uint16_t g16() const
	{
		if (g <= 0) return 0;
		if (g >= 1) return 65535;
		return (uint16_t)floor(g * 65535 + 0.5f);
	}
	uint16_t b16() const
	{
		if (b <= 0) return 0;
		if (b >= 1) return 65535;
		return (uint16_t)floor(b * 65535 + 0.5f);
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
	operator UInt8RGBA () const
	{
		return UInt8RGBA(r8(), g8(), b8());
	}
	operator UInt16RGBA () const
	{
		return UInt16RGBA(r16(), g16(), b16());
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
	static Float16RGB convert(UInt8RGBA const &src)
	{
		_float16_t r = degamma(src.r / 255.0f);
		_float16_t g = degamma(src.g / 255.0f);
		_float16_t b = degamma(src.b / 255.0f);
		return {r, g, b};
	}
	static Float16RGB convert(UInt8GrayA const &src)
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
	uint16_t r16() const
	{
		if ((float)r <= 0) return 0;
		if ((float)r >= 1) return 65535;
		return (uint16_t)floorf((float)r * 65535 + 0.5f);
	}
	uint16_t g16() const
	{
		if ((float)g <= 0) return 0;
		if ((float)g >= 1) return 65535;
		return (uint16_t)floorf((float)g * 65535 + 0.5f);
	}
	uint16_t b16() const
	{
		if ((float)b <= 0) return 0;
		if ((float)b >= 1) return 65535;
		return (uint16_t)floorf((float)b * 65535 + 0.5f);
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
	operator UInt8RGBA () const
	{
		return UInt8RGBA(r8(), g8(), b8());
	}
	operator UInt16RGBA () const
	{
		return UInt16RGBA(r16(), g16(), b16());
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
	static Float32Gray convert(UInt8GrayA const &src)
	{
		return {src.v / 255.0f};
	}
	static Float32Gray convert(UInt8Gray const &src)
	{
		return {src.v / 255.0f};
	}
	static Float32Gray convert(UInt16GrayA const &src)
	{
		return {src.v / 65535.0f};
	}
	static Float32Gray convert(UInt16Gray const &src)
	{
		return {src.v / 65535.0f};
	}
	static inline Float32Gray convert(UInt8RGBA const &r);
	static inline Float32Gray convert(UInt16RGBA const &r);
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
	uint16_t y16() const
	{
		if (v <= 0) return 0;
		if (v >= 1) return 65535;
		return (uint16_t)floor(gamma(v) * 65535 + 0.5f);
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
	UInt8Gray toPixelGray8() const
	{
		return UInt8Gray(y8());
	}
	UInt16Gray toPixelGray16() const
	{
		return UInt16Gray(y16());
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
	static Float16Gray convert(UInt8GrayA const &src)
	{
		return {src.v / 255.0f};
	}
	static Float16Gray convert(UInt16GrayA const &src)
	{
		return {src.v / 65535.0f};
	}
	static Float16Gray convert(UInt8Gray const &src)
	{
		return {src.v / 255.0f};
	}
	static Float16Gray convert(UInt16Gray const &src)
	{
		return {src.v / 65535.0f};
	}
	static inline Float16Gray convert(UInt8RGBA const &r);
	static inline Float16Gray convert(UInt16RGBA const &r);
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
	uint16_t y16() const
	{
		if ((float)v <= 0) return 0;
		if ((float)v >= 1) return 65535;
		return (uint16_t)floorf((float)v * 65535 + 0.5f);
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
	UInt8Gray toPixelGray8() const
	{
		return UInt8Gray(y8());
	}
	UInt16Gray toPixelGray16() const
	{
		return UInt16Gray(y16());
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
	Float32RGBA(Float16RGBA const &t);
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
	static inline Float32RGBA convert(UInt8RGBA const &src);
	static inline Float32RGBA convert(UInt16RGBA const &src);
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
	uint16_t r16() const
	{
		if (r <= 0) return 0;
		if (r >= 1) return 65535;
		return (uint16_t)floorf(r * 65535 + 0.5f);
	}
	uint8_t g8() const
	{
		if (g <= 0) return 0;
		if (g >= 1) return 255;
		return (uint8_t)floorf(g * 255 + 0.5f);
	}
	uint16_t g16() const
	{
		if (g <= 0) return 0;
		if (g >= 1) return 65535;
		return (uint16_t)floorf(g * 65535 + 0.5f);
	}
	uint8_t b8() const
	{
		if (b <= 0) return 0;
		if (b >= 1) return 255;
		return (uint8_t)floorf(b * 255 + 0.5f);
	}
	uint16_t b16() const
	{
		if (b <= 0) return 0;
		if (b >= 1) return 65535;
		return (uint16_t)floorf(b * 65535 + 0.5f);
	}
	uint8_t a8() const
	{
		if (a <= 0) return 0;
		if (a >= 1) return 255;
		return (uint8_t)floorf(a * 255 + 0.5f);
	}
	uint16_t a16() const
	{
		if (a <= 0) return 0;
		if (a >= 1) return 65535;
		return (uint16_t)floorf(a * 65535 + 0.5f);
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
	operator UInt8RGBA () const
	{
		return UInt8RGBA(r8(), g8(), b8(), a8());
	}
	operator UInt16RGBA () const
	{
		return UInt16RGBA(r16(), g16(), b16(), a16());
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
	explicit Float16RGBA(Float32RGBA const &t)
		: r(t.r)
		, g(t.g)
		, b(t.b)
		, a(t.a)
	{
	}
	static inline Float16RGBA convert(UInt8RGB const &src);
	static inline Float16RGBA convert(UInt8RGBA const &src);
	static inline Float16RGBA convert(UInt16RGB const &src);
	static inline Float16RGBA convert(UInt16RGBA const &src);
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
	uint16_t r16() const
	{
		if (r <= 0) return 0;
		if (r >= 1) return 65535;
		return (uint16_t)floorf(r * 65535.0f + 0.5f);
	}
	uint8_t g8() const
	{
		if ((float)g <= 0) return 0;
		if ((float)g >= 1) return 255;
		return (uint8_t)floorf(g * 255.0f + 0.5f);
	}
	uint16_t g16() const
	{
		if ((float)g <= 0) return 0;
		if ((float)g >= 1) return 65535;
		return (uint16_t)floorf(g * 65535.0f + 0.5f);
	}
	uint8_t b8() const
	{
		if ((float)b <= 0) return 0;
		if ((float)b >= 1) return 255;
		return (uint8_t)floorf(b * 255.0f + 0.5f);
	}
	uint16_t b16() const
	{
		if ((float)b <= 0) return 0;
		if ((float)b >= 1) return 65535;
		return (uint16_t)floorf(b * 65535.0f + 0.5f);
	}
	uint8_t a8() const
	{
		if ((float)a <= 0) return 0;
		if ((float)a >= 1) return 255;
		return (uint8_t)floorf(a * 255.0f + 0.5f);
	}
	uint16_t a16() const
	{
		if ((float)a <= 0) return 0;
		if ((float)a >= 1) return 65535;
		return (uint16_t)floorf(a * 65535.0f + 0.5f);
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
	operator UInt8RGBA () const
	{
		return UInt8RGBA(r8(), g8(), b8(), a8());
	}
	operator UInt16RGBA () const
	{
		return UInt16RGBA(r16(), g16(), b16(), a16());
	}
};

inline Float32RGBA::Float32RGBA(Float16RGBA const &t)
	: r((float)t.r)
	, g((float)t.g)
	, b((float)t.b)
	, a((float)t.a)
{
}

inline UInt8RGB UInt8RGB::convert(UInt8Gray const &t)
{
	return UInt8RGB(t.v, t.v, t.v);
}

inline UInt16RGB UInt16RGB::convert(UInt16Gray const &t)
{
	return UInt16RGB(t.v, t.v, t.v);
}

inline UInt8RGB UInt8RGB::convert(Float32RGB const &t)
{
	auto u8 = [](float v){
		return (uint8_t)clamp(floorf(gamma(v) * 255 + 0.5), 0.0f, 255.0f);
	};
	auto r = u8(t.r);
	auto g = u8(t.g);
	auto b = u8(t.b);
	return UInt8RGB(r, g, b);
}

inline UInt16RGB UInt16RGB::convert(Float32RGB const &t)
{
	auto u16 = [](float v){
		return (uint16_t)clamp(floorf(gamma(v) * 65535 + 0.5), 0.0f, 65535.0f);
	};
	auto r = u16(t.r);
	auto g = u16(t.g);
	auto b = u16(t.b);
	return UInt16RGB(r, g, b);
}

inline UInt8RGBA UInt8RGBA::convert(Float32RGBA const &t)
{
	auto u8 = [](float v){
		return (uint8_t)clamp(floorf(gamma(v) * 255 + 0.5), 0.0f, 255.0f);
	};
	auto r = u8(t.r);
	auto g = u8(t.g);
	auto b = u8(t.b);
	return UInt8RGBA(r, g, b, t.a8());
}

inline UInt16RGBA UInt16RGBA::convert(Float32RGBA const &t)
{
	auto u16 = [](float v){
		return (uint16_t)clamp(floorf(gamma(v) * 65535 + 0.5), 0.0f, 65535.0f);
	};
	auto r = u16(t.r);
	auto g = u16(t.g);
	auto b = u16(t.b);
	return UInt16RGBA(r, g, b, t.a16());
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
	static Float32GrayA convert(UInt8GrayA const &src)
	{
		float l = src.v / 255.0;
		return {l, float(src.a / 255.0)};
	}
	static Float32GrayA convert(UInt8Gray const &src)
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
	uint16_t v16() const
	{
		if (v <= 0) return 0;
		if (v >= 1) return 65535;
		return clamp(int(gamma(v) * 65535 + 0.5f), 0, 65535);
	}
	uint8_t a8() const
	{
		if (a <= 0) return 0;
		if (a >= 1) return 255;
		return (uint8_t)floor(a * 255 + 0.5f);
	}
	uint16_t a16() const
	{
		if (a <= 0) return 0;
		if (a >= 1) return 65535;
		return (uint16_t)floor(a * 65535 + 0.5f);
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
	static Float16GrayA convert(UInt8GrayA const &src)
	{
		_float16_t l = src.v / 255.0;
		return {l, _float16_t(src.a / 255.0)};
	}
	static Float16GrayA convert(UInt8Gray const &src)
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
	uint16_t v16() const
	{
		if ((float)v <= 0) return 0;
		if ((float)v >= 1) return 65535;
		return (uint16_t)floorf((float)v * 65535 + 0.5f);
	}
	uint8_t a8() const
	{
		if ((float)a <= 0) return 0;
		if ((float)a >= 1) return 255;
		return (uint8_t)floorf((float)a * 255 + 0.5f);
	}
	uint16_t a16() const
	{
		if ((float)a <= 0) return 0;
		if ((float)a >= 1) return 65535;
		return (uint16_t)floorf((float)a * 65535 + 0.5f);
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
template <> inline UInt8RGBA pixel_cast<UInt8RGBA>(UInt8RGBA const &src)
{
	return src;
}
template <> inline UInt16RGBA pixel_cast<UInt16RGBA>(UInt16RGBA const &src)
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
template <> inline UInt8RGBA pixel_cast<UInt8RGBA>(Float32RGBA const &src)
{
	return UInt8RGBA::convert(src);
}
template <> inline UInt16RGBA pixel_cast<UInt16RGBA>(Float32RGBA const &src)
{
	return UInt16RGBA::convert(src);
}
template <> inline Float32Gray pixel_cast<Float32Gray>(Float32Gray const &src)
{
	return src;
}
template <> inline Float32GrayA pixel_cast<Float32GrayA>(Float32GrayA const &src)
{
	return src;
}
template <> inline Float32GrayA pixel_cast<Float32GrayA>(UInt8GrayA const &src)
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

Float32RGBA Float32RGBA::convert(UInt8RGBA const &src)
{
	float r = src.r / 255.0f;
	float g = src.g / 255.0f;
	float b = src.b / 255.0f;
	return degamma(Float32RGBA(r, g, b, 1.0f));
}

Float32RGBA Float32RGBA::convert(UInt16RGBA const &src)
{
	float r = src.r / 65535.0f;
	float g = src.g / 65535.0f;
	float b = src.b / 65535.0f;
	float a = src.a / 65535.0f;
	return degamma(Float32RGBA(r, g, b, a));
}

Float32RGBA Float32RGBA::convert(Float16RGBA const &src)
{
	return Float32RGBA((float)src.r, (float)src.g, (float)src.b, (float)src.a);
}

Float16RGBA Float16RGBA::convert(UInt8RGB const &src)
{
	float r = src.r / 255.0f;
	float g = src.g / 255.0f;
	float b = src.b / 255.0f;
	return degamma(Float16RGBA(r, g, b, 1.0f));
}

Float16RGBA Float16RGBA::convert(UInt16RGB const &src)
{
	float r = src.r / 65535.0f;
	float g = src.g / 65535.0f;
	float b = src.b / 65535.0f;
	return degamma(Float16RGBA(r, g, b, 1.0f));
}

Float16RGBA Float16RGBA::convert(UInt8RGBA const &src)
{
	float r = src.r / 255.0f;
	float g = src.g / 255.0f;
	float b = src.b / 255.0f;
	float a = src.a / 255.0f;
	return degamma(Float16RGBA(r, g, b, a));
}

Float16RGBA Float16RGBA::convert(UInt16RGBA const &src)
{
	float r = src.r / 65535.0f;
	float g = src.g / 65535.0f;
	float b = src.b / 65535.0f;
	float a = src.a / 65535.0f;
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

UInt8Gray UInt8Gray::convert(UInt8RGB const &t)
{
	return UInt8Gray(::euclase::gray(t.r, t.g, t.b));
}

UInt16Gray UInt16Gray::convert(UInt16RGB const &t)
{
	return UInt16Gray(::euclase::gray(t.r, t.g, t.b));
}

UInt8Gray UInt8Gray::convert(Float32RGBA const &t)
{
	auto y = ::euclase::gray(t.r8(), t.g8(), t.b8());
	y = gamma(y);
	return UInt8Gray(uint8_t(y * t.a));
}

inline UInt8Gray UInt8Gray::convert(const UInt8GrayA &r)
{
	return UInt8Gray(r.v);
}

inline UInt16Gray UInt16Gray::convert(const UInt16GrayA &r)
{
	return UInt16Gray(r.v);
}

Float32Gray Float32Gray::convert(UInt8RGBA const &r)
{
	auto s = Float32RGBA::convert(r);
	return Float32Gray(grayf(s.r, s.g, s.b) * s.a);
}

Float32Gray Float32Gray::convert(UInt16RGBA const &r)
{
	auto s = Float32RGBA::convert(r);
	return Float32Gray(grayf(s.r, s.g, s.b) * s.a);
}

UInt8GrayA UInt8GrayA::convert(const Float32RGBA &t)
{
	return UInt8GrayA(::euclase::gray(t.r, t.g, t.b), 255);
}

UInt16GrayA UInt16GrayA::convert(const Float32RGBA &t)
{
	return UInt16GrayA(::euclase::gray(t.r, t.g, t.b), 65535);
}

UInt8GrayA UInt8GrayA::convert(const Float32RGB &t)
{
	float y = ::euclase::grayf(t.r, t.g, t.b);
	return UInt8GrayA(clamp(int(gamma(y) * 255 + 0.5), 0, 255), 255);
}

UInt16GrayA UInt16GrayA::convert(const Float32RGB &t)
{
	float y = ::euclase::grayf(t.r, t.g, t.b);
	return UInt16GrayA(clamp(int(gamma(y) * 65535 + 0.5), 0, 65535), 65535);
}

UInt8GrayA UInt8GrayA::convert(const Float32GrayA &t)
{
	int y = t.v8();
	int a = clamp(int(t.a * 255 + 0.5), 0, 255);
	return UInt8GrayA(y, a);
}

UInt16GrayA UInt16GrayA::convert(const Float32GrayA &t)
{
	int y = t.v16();
	int a = clamp(int(t.a * 65535 + 0.5), 0, 65535);
	return UInt16GrayA(y, a);
}

UInt8GrayA UInt8GrayA::convert(const Float32Gray &t)
{
	return UInt8GrayA(clamp(int(gamma(t.v) * 255 + 0.5), 0, 255), 255);
}

UInt16GrayA UInt16GrayA::convert(const Float32Gray &t)
{
	return UInt16GrayA(clamp(int(gamma(t.v) * 65535 + 0.5), 0, 65535), 65535);
}

UInt8GrayA UInt8GrayA::convert(const UInt8RGBA &t)
{
	uint8_t v = (uint8_t)::euclase::gray(t.r, t.g, t.b);
	return UInt8GrayA(v, t.a);
}

UInt16GrayA UInt16GrayA::convert(const UInt16RGBA &t)
{
	uint16_t v = (uint16_t)::euclase::gray(t.r, t.g, t.b);
	return UInt16GrayA(v, t.a);
}

UInt8GrayA UInt8GrayA::convert(const UInt8RGB &t)
{
	uint8_t v = (uint8_t)::euclase::gray(t.r, t.g, t.b);
	return UInt8GrayA(v, 255);
}

UInt8GrayA UInt8GrayA::convert(const UInt8Gray &t)
{
	return UInt8GrayA(t.gray(), 255);
}

UInt16GrayA UInt16GrayA::convert(const UInt16Gray &t)
{
	return UInt16GrayA(t.gray(), 65535);
}

template <typename T> static inline T limit(T const &t)
{
	return t.limit();
}

// image

enum ImageFormat {
	Format_Invalid,
	Format_U8_RGB,
	Format_U8_RGBA,
	Format_U8_Grayscale,
	Format_U8_GrayscaleA,
	Format_U16_RGB,
	Format_U16_RGBA,
	Format_U16_Grayscale,
	Format_U16_GrayscaleA,
	Format_F32_RGB,
	Format_F32_RGBA,
	Format_F32_Grayscale,
	Format_F32_GrayscaleA,
	Format_F16_RGB,
	Format_F16_RGBA,
	Format_F16_Grayscale,
	Format_F16_GrayscaleA,
};

class AbstractImage { // Inspired by QImage
public:
	virtual ~AbstractImage() = default;
	virtual int width() const = 0;
	virtual int height() const = 0;
	virtual ImageFormat format() const = 0;
	virtual int bytesPerPixel() const = 0;
	virtual int bytesPerLine() const = 0;
	virtual uint8_t *bits() = 0;
	virtual uint8_t const *bits() const = 0;
	virtual uint8_t *scanLine(int y) = 0;
	virtual uint8_t const *scanLine(int y) const = 0;
};

class Image;

Image convertToFormat(AbstractImage const &source, ImageFormat newformat);

class Image : public AbstractImage {
public:
	enum MemoryType {
		Host,
		CUDA,
	};
private:
	struct Data {
		RefCounter ref;
		ImageFormat format_ = ImageFormat::Format_U8_RGBA;
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
	bool init(int w, int h, ImageFormat format, MemoryType memtype = Host, Color const &color = k::transparent);
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

	Image(int width, int height, ImageFormat format, MemoryType memtype = Host);

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

	ImageFormat format() const
	{
		return ptr_ ? ptr_->format_ : ImageFormat::Format_Invalid;
	}

	bool make(int width, int height, ImageFormat format, MemoryType memtype = Host, euclase::Color const &color = k::transparent)
	{
		assign(nullptr);
		return init(width, height, format, memtype, color);
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

	Image convertToFormat(ImageFormat newformat) const;
	Image makeFPImage() const;

	void swap(Image &other);

	uint8_t *bits();
	const uint8_t *bits() const;
};

#ifdef USE_QT
QImage::Format qimageformat(ImageFormat format);
#endif

int bytesPerPixel(ImageFormat format);

inline int Image::bytesPerPixel() const
{
	return euclase::bytesPerPixel(format());
}

inline uint8_t *Image::scanLine(int y)
{
	if (!ptr_) return nullptr;
	return ptr_->data() + (size_t)bytesPerPixel() * width() * y;
}

inline uint8_t const *Image::scanLine(int y) const
{
	return const_cast<Image *>(this)->scanLine(y);
}

inline uint8_t *Image::bits()
{
	if (!ptr_) return nullptr;
	return ptr_->data();
}

inline const uint8_t *Image::bits() const
{
	if (!ptr_) return nullptr;
	return ptr_->data();
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

class ImageView : public AbstractImage { // Inspired by QImage
private:
	uint8_t *data_;
	ImageFormat format_;
	int width_;
	int height_;
	int bytesPerLine_;
public:
	ImageView(uint8_t *data, int width, int height, ImageFormat format, int bytesPerLine = 0)
		: data_(data)
		, width_(width)
		, height_(height)
		, format_(format)
		, bytesPerLine_(bytesPerLine != 0 ? bytesPerLine : (width * bytesPerPixel()))
	{
	}
	int width() const
	{
		return width_;
	}
	int height() const
	{
		return height_;
	}
	ImageFormat format() const
	{
		return format_;
	}
	int bytesPerPixel() const
	{
		switch (format_) {
		case Format_U8_Grayscale:
			return 1;
		case Format_U8_GrayscaleA:
			return 2;
		case Format_U8_RGB:
			return 3;
		case Format_U8_RGBA:
			return 4;
		case Format_U16_Grayscale:
			return 2;
		case Format_U16_GrayscaleA:
			return 4;
		case Format_U16_RGB:
			return 6;
		case Format_U16_RGBA:
			return 8;
		case Format_F16_Grayscale:
			return 2;
		case Format_F16_GrayscaleA:
			return 4;
		case Format_F16_RGB:
			return 6;
		case Format_F16_RGBA:
			return 8;
		case Format_F32_Grayscale:
			return 4;
		case Format_F32_GrayscaleA:
			return 8;
		case Format_F32_RGB:
			return 12;
		case Format_F32_RGBA:
			return 16;
		}
		return 0;
	}
	int bytesPerLine() const
	{
		return bytesPerLine_;
	}
	uint8_t *bits()
	{
		return data_;
	}
	uint8_t const *bits() const
	{
		return data_;
	}
	uint8_t *scanLine(int y)
	{
		return data_ + (y * bytesPerLine_);
	}
	uint8_t const *scanLine(int y) const
	{
		return data_ + (y * bytesPerLine_);
	}
};

} // namespace euclase

namespace std {
template <> inline void swap<euclase::Image>(euclase::Image &a, euclase::Image &b)
{
	a.swap(b);
}
}

#endif // EUCLASE_H
