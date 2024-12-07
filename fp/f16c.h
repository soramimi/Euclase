#ifndef F16C_H
#define F16C_H

#include <immintrin.h>
#include <cstdint>

static inline void fp32_to_fp16_a(float const *floats4, uint16_t *halfs8)
{
	__m128 float_vector = _mm_load_ps(floats4);
	__m128i half_vector = _mm_cvtps_ph(float_vector, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC);
	_mm_store_si128((__m128i *)halfs8, half_vector);
}

static inline void fp32_to_fp16_u(float const *floats4, uint16_t *halfs8)
{
	__m128 float_vector = _mm_loadu_ps(floats4);
	__m128i half_vector = _mm_cvtps_ph(float_vector, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC);
	_mm_storeu_si128((__m128i *)halfs8, half_vector);
}

inline void fp16_to_fp32_a(uint16_t const *halfs8, float *floats4)
{
	__m128i half_vector = _mm_load_si128((__m128i const *)halfs8);
	*(__m128 *)floats4 = _mm_cvtph_ps(half_vector);
}

inline void fp16_to_fp32_u(uint16_t const *halfs8, float *floats4)
{
	__m128i half_vector = _mm_loadu_si128((__m128i const *)halfs8);
	*(__m128 *)floats4 = _mm_cvtph_ps(half_vector);
}

#endif // F16C_H
