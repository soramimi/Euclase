#ifndef ALPHABLEND_H
#define ALPHABLEND_H

#include <cstdint>
#include <cmath>
#include "euclase.h"

class AlphaBlend {
public:

	using PixelRGBA = euclase::PixelRGBA;
	using PixelGrayA = euclase::PixelGrayA;
	using FPixelRGBA = euclase::FPixelRGBA;

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

	static inline FPixelRGBA gamma(FPixelRGBA const &pix)
	{
		return FPixelRGBA(gamma(pix.r), gamma(pix.g), gamma(pix.b), pix.a);
	}

	static inline FPixelRGBA degamma(FPixelRGBA const &pix)
	{
		return FPixelRGBA(degamma(pix.r), degamma(pix.g), degamma(pix.b), pix.a);
	}

	class fixed_t {
	private:
		int16_t value;
		static int16_t lut_mul_[65536];
		static const int16_t *lut_mul();
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
		FixedRGBA(PixelRGBA const &t)
			: r(t.r)
			, g(t.g)
			, b(t.b)
			, a(t.a)
		{
		}
		operator PixelRGBA () const
		{
			return PixelRGBA((uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a);
		}
	};

	static inline PixelRGBA blend(PixelRGBA const &base, PixelRGBA const &over)
	{
		if (over.a == 0) return base;
		if (base.a == 0 || over.a == 255) return over;
		int r = over.r * over.a * 255 + base.r * base.a * (255 - over.a);
		int g = over.g * over.a * 255 + base.g * base.a * (255 - over.a);
		int b = over.b * over.a * 255 + base.b * base.a * (255 - over.a);
		int a = over.a * 255 + base.a * (255 - over.a);
		return PixelRGBA(r / a, g / a, b / a, div255(a));
	}

	static inline PixelGrayA blend(PixelGrayA const &base, PixelGrayA const &over)
	{
		if (over.a == 0) return base;
		if (base.a == 0 || over.a == 255) return over;
		int y = over.l * over.a * 255 + base.l * base.a * (255 - over.a);
		int a = over.a * 255 + base.a * (255 - over.a);
		return PixelGrayA(y / a, div255(a));
	}

	static inline FPixelRGBA blend(FPixelRGBA const &base, FPixelRGBA const &over)
	{
		if (over.a <= 0) return base;
		if (base.a <= 0 || over.a >= 1) return over;
		float r = over.r * over.a + base.r * base.a * (1 - over.a);
		float g = over.g * over.a + base.g * base.a * (1 - over.a);
		float b = over.b * over.a + base.b * base.a * (1 - over.a);
		float a = over.a + base.a * (1 - over.a);
		return FPixelRGBA(r / a, g / a, b / a, a);
	}

	static inline PixelRGBA blend_with_gamma_collection(PixelRGBA const &base, PixelRGBA const &over)
	{
		return (PixelRGBA)gamma(blend(degamma(FPixelRGBA::convert(base, false)), degamma(FPixelRGBA::convert(over, false))));
	}
};

#endif // ALPHABLEND_H
