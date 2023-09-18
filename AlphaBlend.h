#ifndef ALPHABLEND_H
#define ALPHABLEND_H

#include <cstdint>
#include <cmath>
#include "euclase.h"

class AlphaBlend {
public:

	using OctetRGBA = euclase::OctetRGBA;
	using OctetGrayA = euclase::OctetGrayA;
	using FloatRGBA = euclase::FloatRGBA;

	static inline int div255(int v)
	{
		return (v * 257 + 256) / 65536;
	}

	static inline float gamma(float v)
	{
		return sqrt(v);
	}

	static inline float degamma(float v)
	{
		return v * v;
	}

	static inline FloatRGBA gamma(FloatRGBA const &pix)
	{
		return FloatRGBA(gamma(pix.r), gamma(pix.g), gamma(pix.b), pix.a);
	}

	static inline FloatRGBA degamma(FloatRGBA const &pix)
	{
		return FloatRGBA(degamma(pix.r), degamma(pix.g), degamma(pix.b), pix.a);
	}

	class fixed_t {
	private:
		int16_t value;
		static int16_t lut_mul_[65536];
	public:
		explicit fixed_t(int16_t v = 0)
			: value(v)
		{
		}
		explicit fixed_t(uint8_t v = 0)
			: value(v * 4096 / 255)
		{
		}
		explicit fixed_t(float v = 0)
			: value((int16_t)floor(v * 4096 + 0.5))
		{
		}
		fixed_t operator + (fixed_t r) const
		{
			return fixed_t((int16_t)(value + r.value));
		}
		fixed_t operator - (fixed_t r) const
		{
			return fixed_t((int16_t)(value - r.value));
		}
		fixed_t operator * (fixed_t r) const
		{
			return fixed_t((int16_t)(((int32_t)value * r.value) >> 12));
		}
		fixed_t operator / (fixed_t r) const
		{
			return fixed_t((int16_t)((((int32_t)value << 12) + value) / r.value));
		}
		explicit operator uint8_t () const
		{
			if (value < 0) return 0;
			if (value > 0x0fff) return 255;
			return value >> 4;
		}
		explicit operator float () const
		{
			return float(value / 4096.0);
		}
		static fixed_t value0()
		{
			return fixed_t((int16_t)0);
		}
		static fixed_t value1()
		{
			return fixed_t((int16_t)4096);
		}
	};

	struct FixedRGBA {
		fixed_t r, g, b, a;
		FixedRGBA(float r = 0, float g = 0, float b = 0, float a = 1)
			: r(r)
			, g(g)
			, b(b)
			, a(a)
		{
		}
		FixedRGBA(OctetRGBA const &t)
			: r(t.r)
			, g(t.g)
			, b(t.b)
			, a(t.a)
		{
		}
		operator OctetRGBA () const
		{
			return OctetRGBA((uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a);
		}
	};

	static inline OctetRGBA blend(OctetRGBA const &base, OctetRGBA const &over)
	{
		if (over.a == 0) return base;
		if (base.a == 0 || over.a == 255) return over;
		int r = over.r * over.a * 255 + base.r * base.a * (255 - over.a);
		int g = over.g * over.a * 255 + base.g * base.a * (255 - over.a);
		int b = over.b * over.a * 255 + base.b * base.a * (255 - over.a);
		int a = over.a * 255 + base.a * (255 - over.a);
		return OctetRGBA(r / a, g / a, b / a, div255(a));
	}

	static inline OctetGrayA blend(OctetGrayA const &base, OctetGrayA const &over)
	{
		if (over.a == 0) return base;
		if (base.a == 0 || over.a == 255) return over;
		int y = over.v * over.a * 255 + base.v * base.a * (255 - over.a);
		int a = over.a * 255 + base.a * (255 - over.a);
		return OctetGrayA(y / a, div255(a));
	}

	static inline FloatRGBA blend(FloatRGBA const &base, FloatRGBA const &over)
	{
		if (over.a <= 0) return base;
		if (base.a <= 0 || over.a >= 1) return over;
		float r = over.r * over.a + base.r * base.a * (1 - over.a);
		float g = over.g * over.a + base.g * base.a * (1 - over.a);
		float b = over.b * over.a + base.b * base.a * (1 - over.a);
		float a = over.a + base.a * (1 - over.a);
		return FloatRGBA(r / a, g / a, b / a, a);
	}
};

#endif // ALPHABLEND_H
