#ifndef FP_H
#define FP_H

#include <cstdint>

#define FP32_P_NAN 0x7fc00000 // positive NaN
#define FP32_N_NAN 0xffc00000 // negative NaN
#define FP32_P_INF 0x7f800000 // positive infinity
#define FP32_N_INF 0xff800000 // negative infinity

#define FP16_P_NAN 0x7e00 // positive NaN
#define FP16_N_NAN 0xfe00 // negative NaN
#define FP16_P_INF 0x7c00 // positive infinity
#define FP16_N_INF 0xfc00 // negative infinity
#define FP16_P_ZERO 0x0000 // positive zero
#define FP16_N_ZERO 0x8000 // negative zero

#define FP8_P_NAN 0x7c // positive NaN
#define FP8_N_NAN 0xfc // negative NaN
#define FP8_P_INF 0x78 // positive infinity
#define FP8_N_INF 0xf8 // negative infinity
#define FP8_P_ZERO 0x00 // positive zero
#define FP8_N_ZERO 0x80 // negative zero

#define FP32_S_MASK 0x80000000 // sign mask
#define FP32_E_MASK 0x7f800000 // exponent mask
#define FP32_M_MASK 0x007fffff // mantissa mask

#define FP16_S_MASK 0x8000 // sign mask
#define FP16_E_MASK 0x7c00 // exponent mask
#define FP16_M_MASK 0x03ff // mantissa mask

#define FP8_S_MASK 0x80 // sign mask
#define FP8_E_MASK 0x78 // exponent mask
#define FP8_M_MASK 0x07 // mantissa mask

static inline bool fp16_is_nan(uint16_t fp16)
{
	return (fp16 & FP16_E_MASK) == FP16_E_MASK && (fp16 & FP16_M_MASK);
}

static inline bool fp16_is_inf(uint16_t fp16)
{
	return (fp16 & FP16_E_MASK) == FP16_E_MASK && !(fp16 & FP16_M_MASK);
}

static inline bool fp16_is_zero(uint16_t fp16)
{
	return (fp16 & ~FP16_S_MASK) == 0;
}

static inline bool fp16_is_neg(uint16_t fp16)
{
	return (fp16 & FP16_S_MASK) != 0;
}

static inline bool fp16_is_pos(uint16_t fp16)
{
	return (fp16 & FP16_S_MASK) == 0;
}

static inline bool fp8_is_nan(uint8_t fp8)
{
	return (fp8 & FP8_E_MASK) == FP8_E_MASK && (fp8 & FP8_M_MASK);
}

static inline bool fp8_is_inf(uint8_t fp8)
{
	return (fp8 & FP8_E_MASK) == FP8_E_MASK && !(fp8 & FP8_M_MASK);
}

static inline bool fp8_is_zero(uint8_t fp8)
{
	return (fp8 & ~FP8_S_MASK) == 0;
}

static inline bool fp8_is_neg(uint8_t fp8)
{
	return (fp8 & FP8_S_MASK) != 0;
}

static inline bool fp8_is_pos(uint8_t fp8)
{
	return (fp8 & FP8_S_MASK) == 0;
}

float fp16_to_fp32(uint16_t fp16);
uint16_t fp32_to_fp16(float fp32);
float fp8_to_fp32(uint8_t fp8);
uint8_t fp32_to_fp8(float fp32);

#endif // FP_H

