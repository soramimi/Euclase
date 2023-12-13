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

namespace euclase {

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

inline float clamp_f01(float a)
{
	a = floorf(a * 1000000.0f + 0.5f) / 1000000.0f;
	return std::max(0.0f, std::min(1.0f, a));
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

class FloatRGB;
class FloatRGBA;
class FloatGray;
class FloatGrayA;
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
	static OctetRGB convert(FloatRGB const &t);
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
	static OctetRGBA convert(FloatRGBA const &t);
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
	static inline OctetGray convert(FloatRGBA const &t);
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
	static inline OctetGrayA convert(FloatRGBA const &t);
	static inline OctetGrayA convert(FloatRGB const &t);
	static inline OctetGrayA convert(FloatGrayA const &t);
	static inline OctetGrayA convert(FloatGray const &t);
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



class FloatRGB {
public:
	float r;
	float g;
	float b;
	FloatRGB()
		: r(0)
		, g(0)
		, b(0)
	{
	}
	FloatRGB(float r, float g, float b)
		: r(r)
		, g(g)
		, b(b)
	{
	}
	static FloatRGB convert(OctetRGBA const &src)
	{
		float r = degamma(src.r / 255.0);
		float g = degamma(src.g / 255.0);
		float b = degamma(src.b / 255.0);
		return {r, g, b};
	}
	static FloatRGB convert(OctetGrayA const &src)
	{
		float v = degamma(src.v / 255.0);
		return {v, v, v};
	}
	FloatRGB operator + (FloatRGB const &right) const
	{
		return FloatRGB(r + right.r, g + right.g, b + right.b);
	}
	FloatRGB operator * (float t) const
	{
		return FloatRGB(r * t, g * t, b * t);
	}
	void operator += (FloatRGB const &o)
	{
		r += o.r;
		g += o.g;
		b += o.b;
	}
	void add(FloatRGB const &p, float n)
	{
		r += p.r * n;
		g += p.g * n;
		b += p.b * n;
	}
	void sub(FloatRGB const &p, float n)
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
	FloatRGB limit() const
	{
		return FloatRGB(clamp(r, 0.0f, 1.0f), clamp(g, 0.0f, 1.0f), clamp(b, 0.0f, 1.0f));
	}
	FloatRGB color(float amount) const
	{
		if (amount == 1) {
			return *this;
		} else if (amount == 0) {
			return FloatRGB(0, 0, 0);
		}
		float m = 1.0f / amount;
		FloatRGB t = *this;
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

class FloatGray {
public:
	float v;
	FloatGray()
		: v(0)
	{
	}
	FloatGray(float y)
		: v(y)
	{
	}
	static FloatGray convert(OctetGrayA const &src)
	{
		return {src.v / 255.0f};
	}
	static FloatGray convert(OctetGray const &src)
	{
		return {src.v / 255.0f};
	}
	static inline FloatGray convert(OctetRGBA const &r);
	FloatGray operator + (FloatGray const &right) const
	{
		return FloatGray(v + right.v);
	}
	FloatGray operator * (float t) const
	{
		return FloatGray(v * t);
	}
	void operator += (FloatGray const &o)
	{
		v += o.v;
	}
	void add(FloatGray const &p, float n)
	{
		v += p.v * n;
	}
	void sub(FloatGray const &p, float n)
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
	FloatGray limit() const
	{
		return FloatGray(clamp_f01(v));
	}
	FloatGray color(float amount) const
	{
		if (amount == 1) {
			return *this;
		} else if (amount == 0) {
			return FloatGray(0);
		}
		float m = 1 / amount;
		FloatGray t = *this * m;
		return t.limit();
	}
	OctetGray toPixelGray() const
	{
		return OctetGray(y8());
	}
	static inline FloatGray convert(FloatRGBA const &t);
};

class FloatRGBA {
public:
	float r;
	float g;
	float b;
	float a;
	FloatRGBA()
		: r(0)
		, g(0)
		, b(0)
		, a(0)
	{
	}
	explicit FloatRGBA(float r, float g, float b, float a = 1)
		: r(r)
		, g(g)
		, b(b)
		, a(a)
	{
	}
	explicit FloatRGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 1)
	{
		this->r = r / 255.0f;
		this->g = g / 255.0f;
		this->b = b / 255.0f;
		this->a = a / 255.0f;
	}
	static inline FloatRGBA convert(OctetRGBA const &src);
	FloatRGBA operator + (FloatRGBA const &right) const
	{
		return FloatRGBA(r + right.r, g + right.g, b + right.b);
	}
	FloatRGBA operator * (float t) const
	{
		return FloatRGBA(r * t, g * t, b * t);
	}
	void operator += (FloatRGBA const &o)
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
	void add(FloatRGBA const &p, float n)
	{
		n *= p.a;
		a += n;
		r += p.r * n;
		g += p.g * n;
		b += p.b * n;
	}
	void sub(FloatRGBA const &p, float n)
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
	FloatRGBA limit() const
	{
		return FloatRGBA(clamp_f01(r), clamp_f01(g), clamp_f01(b), clamp_f01(a));
	}
	FloatRGBA color(float amount) const
	{
		if (amount == 0) {
			return FloatRGBA(0.0f, 0.0f, 0.0f, 0.0f);
		}
		FloatRGBA t(*this);
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

inline OctetRGB OctetRGB::convert(FloatRGB const &t)
{
	auto u8 = [](float v){
		return (uint8_t)clamp(floorf(gamma(v) * 255 + 0.5), 0.0f, 255.0f);
	};
	auto r = u8(t.r);
	auto g = u8(t.g);
	auto b = u8(t.b);
	return OctetRGB(r, g, b);
}

inline OctetRGBA OctetRGBA::convert(FloatRGBA const &t)
{
	auto u8 = [](float v){
		return (uint8_t)clamp(floorf(gamma(v) * 255 + 0.5), 0.0f, 255.0f);
	};
	auto r = u8(t.r);
	auto g = u8(t.g);
	auto b = u8(t.b);
	return OctetRGBA(r, g, b, t.a8());
}

class FloatGrayA {
public:
	float v;
	float a;
	FloatGrayA()
		: v(0)
		, a(0)
	{
	}
	FloatGrayA(float v, float a = 1)
		: v(v)
		, a(a)
	{
	}
	static FloatGrayA convert(OctetGrayA const &src)
	{
		float l = src.v / 255.0;
		return {l, float(src.a / 255.0)};
	}
	static FloatGrayA convert(OctetGray const &src)
	{
		float l = src.v / 255.0;
		return {l, 255};
	}
	FloatGrayA operator + (FloatGrayA const &right) const
	{
		return FloatGrayA(v + right.v);
	}
	FloatGrayA operator * (float t) const
	{
		return FloatGrayA(v * t);
	}
	void operator += (FloatGrayA const &o)
	{
		v += o.v;
	}
	void operator *= (float t)
	{
		v *= t;
	}
	void add(FloatGrayA const &p, float n)
	{
		n *= p.a;
		a += n;
		v += p.v * n;
	}
	void sub(FloatGrayA const &p, float n)
	{
		n *= p.a;
		a -= n;
		v -= p.v * n;
	}
	uint8_t v8() const
	{
		if (v <= 0) return 0;
		if (v >= 1) return 255;
		return clamp(int(gamma(v) * 255 + 0.5), 0, 255);
	}
	uint8_t a8() const
	{
		if (a <= 0) return 0;
		if (a >= 1) return 255;
		return (uint8_t)floor(a * 255 + 0.5);
	}
	FloatGrayA limit() const
	{
		return FloatGrayA(clamp_f01(v), clamp_f01(a));
	}
	FloatGrayA color(float amount) const
	{
		if (amount == 0) {
			return FloatGrayA(0, 0);
		}
		FloatGrayA t(*this);
		t *= (1.0f / t.a);
		t.a = a / amount;
		return FloatGrayA(t.v, t.a).limit();
	}
};

template <typename D, typename S> D pixel_cast(S const &src);
template <> inline OctetRGBA pixel_cast<OctetRGBA>(OctetRGBA const &src)
{
	return src;
}
template <> inline FloatRGB pixel_cast<FloatRGB>(FloatRGB const &src)
{
	return src;
}
template <> inline FloatRGBA pixel_cast<FloatRGBA>(FloatRGBA const &src)
{
	return src;
}
template <> inline OctetRGBA pixel_cast<OctetRGBA>(FloatRGBA const &src)
{
	return OctetRGBA::convert(src);
}
template <> inline FloatGray pixel_cast<FloatGray>(FloatGray const &src)
{
	return src;
}
template <> inline FloatGrayA pixel_cast<FloatGrayA>(FloatGrayA const &src)
{
	return src;
}
template <> inline FloatGrayA pixel_cast<FloatGrayA>(OctetGrayA const &src)
{
	return FloatGrayA::convert(src);
}


static inline FloatRGBA gamma(FloatRGBA const &pix)
{
	return FloatRGBA(gamma(pix.r), gamma(pix.g), gamma(pix.b), pix.a);
}

static inline FloatRGBA degamma(FloatRGBA const &pix)
{
	return FloatRGBA(degamma(pix.r), degamma(pix.g), degamma(pix.b), pix.a);
}

FloatRGBA FloatRGBA::convert(OctetRGBA const &src)
{
	float r = src.r / 255.0f;
	float g = src.g / 255.0f;
	float b = src.b / 255.0f;
	float a = src.a / 255.0f;
	return degamma(FloatRGBA(r, g, b, a));
}

FloatGray FloatGray::convert(FloatRGBA const &t)
{
	return FloatGray(grayf(t.r, t.g, t.b) * t.a);
}

OctetGray OctetGray::convert(OctetRGB const &t)
{
	return OctetGray(::euclase::gray(t.r, t.g, t.b));
}

OctetGray OctetGray::convert(FloatRGBA const &t)
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

FloatGray FloatGray::convert(OctetRGBA const &r)
{
	auto s = FloatRGBA::convert(r);
	return FloatGray(grayf(s.r, s.g, s.b) * s.a);
}

OctetGrayA OctetGrayA::convert(const FloatRGBA &t)
{
	return OctetGrayA(::euclase::gray(t.r, t.g, t.b), 255);
}

OctetGrayA OctetGrayA::convert(const FloatRGB &t)
{
	float y = ::euclase::grayf(t.r, t.g, t.b);
	return OctetGrayA(clamp(int(gamma(y) * 255 + 0.5), 0, 255), 255);
}

OctetGrayA OctetGrayA::convert(const FloatGrayA &t)
{
	int y = t.v8();
	int a = clamp(int(t.a * 255 + 0.5), 0, 255);
	return OctetGrayA(y, a);
}

OctetGrayA OctetGrayA::convert(const FloatGray &t)
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
		Format_8_RGB,
		Format_8_RGBA,
		Format_8_Grayscale,
		Format_8_GrayscaleA,
		Format_F_RGB,
		Format_F_RGBA,
		Format_F_Grayscale,
		Format_F_GrayscaleA,
	};
	enum MemoryType {
		Host,
		CUDA,
	};
private:
	struct Data {
		RefCounter ref;
		Image::Format format_ = Image::Format_8_RGBA;
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

FloatHSV rgb_to_hsv(FloatRGB const &rgb);
FloatRGB hsv_to_rgb(FloatHSV const &hsv);

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
