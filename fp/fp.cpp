
#include "fp.h"
#include <stdio.h>
#include <algorithm>

float fp16_to_fp32(uint16_t fp16)
{
	float f = 0;
	if (fp16 == FP16_P_ZERO) return 0.0f; // +0.0
	if (fp16 == FP16_N_ZERO) return -0.0f; // -0.0
	if ((fp16 & FP16_E_MASK) == FP16_E_MASK) {
		if (fp16 & FP16_M_MASK) {
			*(uint32_t *)&f = (fp16 & FP16_S_MASK) ? FP32_N_NAN : FP32_P_NAN; // nan
		} else {
			*(uint32_t *)&f = (fp16 & FP16_S_MASK) ? FP32_N_INF : FP32_P_INF; // inf
		}
	} else {
		*(uint32_t *)&f = ((fp16 & 0x7fff) << 13) + ((127 - 15) << 23) | ((fp16 & FP16_S_MASK) << 16); // valid number
	}
	return f;
}

uint16_t fp32_to_fp16(float fp32)
{
	uint32_t t = *(uint32_t *)&fp32;
	int e = (t >> 23) & 0xff;
	uint32_t m = t & FP32_M_MASK;
	if (e == 0xff && m != 0) return (t & FP32_S_MASK) ? FP16_N_INF : FP16_P_INF; // nan
	e = std::max(-15, std::min(16, e - 127)) + 15;
	m >>= 13;
	return (e << 10) | m | ((t >> 16) & FP16_S_MASK);
}

float fp8_to_fp32(uint8_t fp8)
{
	float f = 0;
	if (fp8 == FP8_P_ZERO) return 0.0f; // +0.0
	if (fp8 == FP8_N_ZERO) return -0.0f; // -0.0
	if ((fp8 & FP8_E_MASK) == FP8_E_MASK) {
		if (fp8 & 0x07) {
			*(uint32_t *)&f = (fp8 & FP8_S_MASK) ? FP32_N_NAN : FP32_P_NAN; // nan
		} else {
			*(uint32_t *)&f = (fp8 & FP8_S_MASK) ? FP32_N_INF : FP32_P_INF; // inf
		}
	} else {
		*(uint32_t *)&f = ((fp8 & 0x7f) << 20) + ((127 - 7) << 23) | ((fp8 & FP8_S_MASK) << 24); // valid number
	}
	return f;
}

uint8_t fp32_to_fp8(float fp32)
{
	uint32_t t = *(uint32_t *)&fp32;
	int e = (t >> 23) & 0xff;
	uint32_t m = t & FP32_M_MASK;
	if (e == 0xff && m != 0) return (t & FP32_S_MASK) ? FP8_N_NAN : FP8_P_NAN; // nan
	e = std::max(-7, std::min(8, e - 127)) + 7;
	m >>= 20;
	return (e << 3) | m | ((t >> 24) & FP8_S_MASK);
}

uint16_t fp16_sqrt_table[] = {
#include "fp16_sqrt_table.txt"
};

uint16_t fp16_pow2_table[] = {
#include "fp16_pow2_table.txt"
};

uint16_t fp16_sqrt(uint16_t fp16)
{
	if (fp16_is_zero(fp16)) return FP16_P_ZERO;
	if (fp16_is_nan(fp16)) return fp16;
	if (fp16_is_inf(fp16)) return fp16;
	if (fp16 >= 0x8000) return FP16_N_NAN;
	return fp16_sqrt_table[fp16];
}

uint16_t fp16_pow2(uint16_t fp16)
{
	if (fp16_is_zero(fp16)) return FP16_P_ZERO;
	if (fp16_is_nan(fp16)) return fp16;
	if (fp16_is_inf(fp16)) return fp16;
	return fp16_pow2_table[fp16 & ~FP16_S_MASK];
}

uint16_t fp16_degamma_table[] = {
#include "fp16_degamma_table.txt"
};

uint16_t fp16_gamma_table[] = {
#include "fp16_gamma_table.txt"
};

uint16_t fp16_degamma(uint16_t fp16)
{
	if (fp16_is_zero(fp16)) return FP16_P_ZERO;
	if (fp16_is_nan(fp16)) return fp16;
	if (fp16_is_inf(fp16)) return fp16;
	return fp16_degamma_table[fp16];
}

uint16_t fp16_gamma(uint16_t fp16)
{
	if (fp16_is_zero(fp16)) return FP16_P_ZERO;
	if (fp16_is_nan(fp16)) return fp16;
	if (fp16_is_inf(fp16)) return fp16;
	if (fp16 >= 0x8000) return FP16_N_NAN;
	return fp16_gamma_table[fp16 & ~FP16_S_MASK];
}

