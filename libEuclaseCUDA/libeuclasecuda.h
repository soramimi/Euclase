
#ifndef CUDAIMAGE_H
#define CUDAIMAGE_H

#include <cstdint>

typedef void cudamem_t;

struct CUDAIMAGE_API {
	cudamem_t *(*malloc)(int len);
	void (*free)(cudamem_t *mem);
	void (*memcpy_htoh)(void *dst_h, void const *src_h, int len);
	void (*memcpy_dtoh)(void *dst_h, cudamem_t const *src_d, int len);
	void (*memcpy_htod)(cudamem_t *dst_d, void const *src_h, int len);
	void (*memcpy_dtod)(cudamem_t *dst_d, cudamem_t const *src_d, int len);
	void (*memset)(cudamem_t *dst, uint8_t c, int len);
	void (*saturation_brightness)(int w, int h, int red, int green, int blue, cudamem_t *mem);
	void (*round_brush)(int w, int h, float cx, float cy, float radius, float blur, float mul, cudamem_t *mem);
	void (*fill_uint8_rgba)(int w, int h, uint8_t r, uint8_t g, uint8_t b, uint8_t a, cudamem_t *dst, int dst_w, int dst_h, int dx, int dy);
	void (*fill_float_rgba)(int w, int h, float r, float g, float b, float a, cudamem_t *dst, int dst_w, int dst_h, int dx, int dy);
	void (*copy_uint8_rgba)(int w, int h, cudamem_t const *src, int src_w, int src_h, int sx, int sy, cudamem_t *dst, int dst_w, int dst_h, int dx, int dy);
	void (*blend_float_RGBA)(int w, int h, cudamem_t const *src, int src_w, int src_h, int sx, int sy, cudamem_t const *mask, int mask_w, int mask_h, cudamem_t *dst, int dst_w, int dst_h, int dx, int dy);
	void (*blend_uint8_grayscale)(int w, int h, cudamem_t const *src, int src_w, int src_h, int sx, int sy, cudamem_t const *mask, int mask_w, int mask_h, cudamem_t *dst, int dst_w, int dst_h, int dx, int dy);
	void (*outline_uint8_grayscale)(int w, int h, cudamem_t const *src, cudamem_t *dst);
	void (*compose_float_rgba)(int w, int h, cudamem_t *dst, cudamem_t const *src, cudamem_t const *mask);
	void (*scale_float_to_uint8_rgba)(int dw, int dh, int dstride, cudamem_t *dst, int sw, int sh, cudamem_t const *src);
	void (*scale)(int dw, int dh, int dstride, cudamem_t *dst, int sw, int sh, int sstride, cudamem_t const *src, int psize);
};

#endif
