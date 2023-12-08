#include <stdio.h>
#include "libeuclasecuda.h"

__device__ inline uint8_t clamp_uint8(float x)
{
	return (uint8_t)max(0.0f, min(255.0f, x));
}

__global__ void cu_round_brush(int w, int h, float cx, float cy, float radius, float blur, float mul, float *p)
{
	int i = blockIdx.x * blockDim.x + threadIdx.x;
	int j = blockIdx.y * blockDim.y + threadIdx.y;
	if (i < w && j < h) {
		float x = i + 0.5 - cx;
		float y = j + 0.5 - cy;

		float value = 0;
		float d = sqrt(x * x + y * y);
		if (d > radius) {
			value = 0;
		} else if (d > blur && mul > 0) {
			float t = (d - blur) * mul;
			if (t < 1) {
				float u = 1 - t;
				value = u * u * (u + t * 3);
			}
		} else {
			value = 1;
		}

		p += 4 * (w * j + i);
		p[3] = value;
	}
}

static void cuda_round_brush(int w, int h, float cx, float cy, float radius, float blur, float mul, cudamem_t *mem)
{
	dim3 blocks((w + 15) / 16, (h + 15) / 16);
	dim3 threads(16, 16);
	cu_round_brush<<<blocks,threads>>>(w, h, cx, cy, radius, blur, mul, (float *)mem);
}

__global__ void cu_saturation_brightness(int w, int h, int red, int green, int blue, uint8_t *p)
{
	int x = blockIdx.x * blockDim.x + threadIdx.x;
	int y = blockIdx.y * blockDim.y + threadIdx.y;

	if (x < w && y < h) {
		float t = x / (w - 1.0);
		float u = 1 - y / (h - 1.0);
		float r = (255 - (255 - red) * t) * u;
		float g = (255 - (255 - green) * t) * u;
		float b = (255 - (255 - blue) * t) * u;
		int z = 4 * (w * y + x);
		p[z + 0] = r;
		p[z + 1] = g;
		p[z + 2] = b;
		p[z + 3] = 255;
	}
}

static void cuda_saturation_brightness(int w, int h, int red, int green, int blue, cudamem_t *mem)
{
	dim3 blocks((w + 15) / 16, (h + 15) / 16);
	dim3 threads(16, 16);
	cu_saturation_brightness<<<blocks,threads>>>(w, h, red, green, blue, (uint8_t *)mem);
}

__global__ void cu_fill_uint8_rgba_kernel(int w, int h, uint8_t r, uint8_t g, uint8_t b, uint8_t a, uint8_t *dst, int dst_w, int dx, int dy)
{
	int x = blockIdx.x * blockDim.x + threadIdx.x;
	int y = blockIdx.y * blockDim.y + threadIdx.y;

	if (x < w && y < h) {
		uint8_t *d = dst + 4 * (dst_w * (dy + y) + dx + x);
		d[0] = r;
		d[1] = g;
		d[2] = b;
		d[3] = a;
	}
}

void cuda_fill_uint8_rgba(int w, int h, uint8_t r, uint8_t g, uint8_t b, uint8_t a, cudamem_t *dst, int dst_w, int dst_h, int dx, int dy)
{
	uint8_t *d = (uint8_t *)dst;

	dim3 blocks((w + 15) / 16, (h + 15) / 16);
	dim3 threads(16, 16);
	cu_fill_uint8_rgba_kernel<<<blocks,threads>>>(w, h, r, g, b, a, d, dst_w, dx, dy);
}

__global__ void cu_fill_float_rgba_kernel(int w, int h, float r, float g, float b, float a, float *dst, int dst_w, int dx, int dy)
{
	int x = blockIdx.x * blockDim.x + threadIdx.x;
	int y = blockIdx.y * blockDim.y + threadIdx.y;

	if (x < w && y < h) {
		float *d = dst + 4 * (dst_w * (dy + y) + dx + x);
		d[0] = r;
		d[1] = g;
		d[2] = b;
		d[3] = a;
	}
}

void cuda_fill_float_rgba(int w, int h, float r, float g, float b, float a, cudamem_t *dst, int dst_w, int dst_h, int dx, int dy)
{
	float *d = (float *)dst;

	dim3 blocks((w + 15) / 16, (h + 15) / 16);
	dim3 threads(16, 16);
	cu_fill_float_rgba_kernel<<<blocks,threads>>>(w, h, r, g, b, a, d, dst_w, dx, dy);
}

void cuda_copy_uint8_rgba(int w, int h, cudamem_t const *src, int src_w, int src_h, int sx, int sy, cudamem_t *dst, int dst_w, int dst_h, int dx, int dy)
{
	uint32_t const *s = (uint32_t const *)src;
	uint32_t *d = (uint32_t *)dst;

	if (w == src_w && w == dst_w && sx == 0 && sy == 0 && dx == 0 && dy == 0) {
		cudaMemcpy(d, s, 4 * w * h, cudaMemcpyDeviceToHost);
	} else {
		s += src_w * sy + sx;
		d += dst_w * dy + dx;
		for (int y = 0; y < h; y++) {
			cudaMemcpy(d + dx, s + sx, 4 * w, cudaMemcpyDeviceToHost);
			s += src_w;
			d += dst_w;
		}
	}
}

__global__ void cu_blend_float_rgba_kernel(int w, int h, float const *src, int src_w, int sx, int sy, uint8_t const *mask, int mask_w, float *dst, int dst_w, int dx, int dy)
{
	int x = blockIdx.x * blockDim.x + threadIdx.x;
	int y = blockIdx.y * blockDim.y + threadIdx.y;

	if (x < w && y < h) {
		uint8_t m = mask ? *(mask + mask_w * y + x) : 255;
		float const *s = src + 4 * (src_w * (sy + y) + sx + x);
		float *d = dst + 4 * (dst_w * (dy + y) + dx + x);
		float baseR = d[0];
		float baseG = d[1];
		float baseB = d[2];
		float baseA = d[3];
		float overR = s[0];
		float overG = s[1];
		float overB = s[2];
		float overA = s[3];
		overA = overA * m / 255;
		float r = overR * overA + baseR * baseA * (1 - overA);
		float g = overG * overA + baseG * baseA * (1 - overA);
		float b = overB * overA + baseB * baseA * (1 - overA);
		float a = overA + baseA * (1 - overA);
		if (a > 0) {
			float t = 1 / a;
			r *= t;
			g *= t;
			b *= t;
		}
		d[0] = r;
		d[1] = g;
		d[2] = b;
		d[3] = a;
	}
}

void cuda_blend_float_rgba(int w, int h, cudamem_t const *src, int src_w, int src_h, int sx, int sy, uint8_t const *mask, int mask_w, int mask_h, cudamem_t *dst, int dst_w, int dst_h, int dx, int dy)
{
	float const *s = (float const *)src;
	float *d = (float *)dst;
	uint8_t *buf_mask = nullptr;

	if (mask) {
		cudaMalloc(&buf_mask, sizeof(uint8_t) * mask_w * mask_h);
		cudaMemcpy(buf_mask, mask, sizeof(uint8_t) * mask_w * mask_h, cudaMemcpyHostToDevice);
	}

	dim3 blocks((w + 15) / 16, (h + 15) / 16);
	dim3 threads(16, 16);
	cu_blend_float_rgba_kernel<<<blocks,threads>>>(w, h, s, src_w, sx, sy, buf_mask, mask_w, d, dst_w, dx, dy);

	if (mask) {
		cudaFree(buf_mask);
	}
}

__global__ void cu_blend_uint8_grayscale_kernel(int w, int h, uint8_t const *src, int src_w, int sx, int sy, uint8_t const *mask, int mask_w, uint8_t *dst, int dst_w, int dx, int dy)
{
	int x = blockIdx.x * blockDim.x + threadIdx.x;
	int y = blockIdx.y * blockDim.y + threadIdx.y;

	if (x < w && y < h) {
		uint8_t m = mask ? *(mask + mask_w * y + x) : 255;
		uint8_t const *s = src + (src_w * (sy + y) + sx + x);
		uint8_t *d = dst + (dst_w * (dy + y) + dx + x);
		*d = (*d * (255 - m) + *s * m) / 255;
	}
}

void cuda_blend_uint8_grayscale(int w, int h, cudamem_t const *src, int src_w, int src_h, int sx, int sy, uint8_t const *mask, int mask_w, int mask_h, cudamem_t *dst, int dst_w, int dst_h, int dx, int dy)
{
	uint8_t const *s = (uint8_t const *)src;
	uint8_t *d = (uint8_t *)dst;
	uint8_t *buf_mask = nullptr;

	if (mask) {
		cudaMalloc(&buf_mask, sizeof(uint8_t) * mask_w * mask_h);
		cudaMemcpy(buf_mask, mask, sizeof(uint8_t) * mask_w * mask_h, cudaMemcpyHostToDevice);
	}

	dim3 blocks((w + 15) / 16, (h + 15) / 16);
	dim3 threads(16, 16);
	cu_blend_uint8_grayscale_kernel<<<blocks,threads>>>(w, h, s, src_w, sx, sy, buf_mask, mask_w, d, dst_w, dx, dy);

	if (mask) {
		cudaFree(buf_mask);
	}
}

__global__ void cu_outline_uint8_grayscale_kernel(int w, int h, uint8_t const *src, uint8_t *dst)
{
	int x = blockIdx.x * blockDim.x + threadIdx.x;
	int y = blockIdx.y * blockDim.y + threadIdx.y;

	if (x < w && y < h) {
		uint8_t *d = dst + w * y + x;
		if (x > 0 && x + 1 < w && y > 0 && y + 1 < h) {
			uint8_t const *s0 = src + w * (y - 1);
			uint8_t const *s1 = src + w * y;
			uint8_t const *s2 = src + w * (y + 1);
			uint8_t v = ~(s0[x - 1] & s0[x] & s0[x + 1] & s1[x - 1] & s1[x + 1] & s2[x - 1] & s2[x] & s2[x + 1]) & s1[x];
			*d = (v & 0x80) ? 0 : 255;
		} else {
			*d = 255;
		}
	}
}

void cuda_outline_uint8_grayscale(int w, int h, cudamem_t const *src, cudamem_t *dst)
{
	uint8_t const *s = (uint8_t const *)src;
	uint8_t *d = (uint8_t *)dst;

	dim3 blocks((w + 15) / 16, (h + 15) / 16);
	dim3 threads(16, 16);
	cu_outline_uint8_grayscale_kernel<<<blocks,threads>>>(w, h, s, d);
}

__global__ void cu_compose_float_rgba_kernel(int w, int h, float *dst, float const *src, uint8_t const *mask)
{
	int x = blockIdx.x * blockDim.x + threadIdx.x;
	int y = blockIdx.y * blockDim.y + threadIdx.y;

	if (x < w && y < h) {
		float *d = dst + 4 * (w * y + x);
		float const *s = src + 4 * (w * y + x);
		float t = mask ? mask[w * y + x] / 255.0f : 1.0f;
		float u = 1.0f - t;
		d[0] = d[0] * u + s[0] * t;
		d[1] = d[1] * u + s[1] * t;
		d[2] = d[2] * u + s[2] * t;
		d[3] = d[3] * u + s[3] * t;
	}
}

void cuda_compose_float_rgba(int w, int h, cudamem_t *dst, cudamem_t const *src, cudamem_t *mask)
{
	float *d = (float *)dst;
	float const *s = (float const *)src;
	uint8_t const *m = (uint8_t const *)mask;

	dim3 blocks((w + 15) / 16, (h + 15) / 16);
	dim3 threads(16, 16);
	cu_compose_float_rgba_kernel<<<blocks,threads>>>(w, h, d, s, m);
}

__global__ void cu_scale_float_to_uint8_rgba_kernel(int dw, int dh, int dstride, uint8_t *dst, int sw, int sh, float const *src)
{
	int dx = blockIdx.x * blockDim.x + threadIdx.x;
	int dy = blockIdx.y * blockDim.y + threadIdx.y;

	if (dx < dw && dy < dh) {
		int sx = dx * sw / dw;
		int sy = dy * sh / dh;
		float const *s = src + 4 * (sw * sy + sx);
		uint8_t *d = dst + dstride * dy + 4 * dx;
		float R = max(0.0f, min(1.0f, s[0]));
		float G = max(0.0f, min(1.0f, s[1]));
		float B = max(0.0f, min(1.0f, s[2]));
		float A = max(0.0f, min(1.0f, s[3]));
		d[0] = int(sqrt(R) * 255 + 0.5f);
		d[1] = int(sqrt(G) * 255 + 0.5f);
		d[2] = int(sqrt(B) * 255 + 0.5f);
		d[3] = int(A * 255 + 0.5f);
	}
}

void cuda_scale_float_to_uint8_rgba(int dw, int dh, int dstride, cudamem_t *dst, int sw, int sh, cudamem_t const *src)
{
	uint8_t *buf_dst = nullptr;
	cudaMalloc(&buf_dst, dstride * dh);

	dim3 blocks((dw + 15) / 16, (dh + 15) / 16);
	dim3 threads(16, 16);
	cu_scale_float_to_uint8_rgba_kernel<<<blocks,threads>>>(dw, dh, dstride, (uint8_t *)buf_dst, sw, sh, (float const *)src);

	cudaMemcpy(dst, buf_dst, dstride * dh, cudaMemcpyDeviceToHost);
	cudaFree(buf_dst);
}

__global__ void cu_scale_kernel(int dw, int dh, int dstride, uint8_t *dst, int sw, int sh, int sstride, uint8_t const *src, int psize)
{
	int dx = blockIdx.x * blockDim.x + threadIdx.x;
	int dy = blockIdx.y * blockDim.y + threadIdx.y;

	if (dx < dw && dy < dh) {
		int sx = dx * sw / dw;
		int sy = dy * sh / dh;
		uint8_t const *s = src + sstride * sy + sx * psize;
		uint8_t *d = dst + dstride * dy + dx * psize;
		memcpy(d, s, psize);
	}
}

void cuda_scale(int dw, int dh, int dstride, cudamem_t *dst, int sw, int sh, int sstride, cudamem_t const *src, int psize)
{
	dim3 blocks((dw + 15) / 16, (dh + 15) / 16);
	dim3 threads(16, 16);
	cu_scale_kernel<<<blocks,threads>>>(dw, dh, dstride, (uint8_t *)dst, sw, sh, sstride, (uint8_t const *)src, psize);
}

cudamem_t *cuda_malloc(int len)
{
	cudamem_t *mem = nullptr;
	cudaMalloc((void **)&mem, len);
	return mem;
}

void cuda_free(cudamem_t *mem)
{
	cudaFree(mem);
}

void cuda_memcpy_htoh(void *dst_h, void const *src_h, int len)
{
	cudaMemcpy(dst_h, src_h, len, cudaMemcpyHostToHost);
}

void cuda_memcpy_dtoh(void *dst_h, cudamem_t const *src_d, int len)
{
	cudaMemcpy(dst_h, src_d, len, cudaMemcpyDeviceToHost);
}

void cuda_memcpy_htod(cudamem_t *dst_d, void const *src_h, int len)
{
	cudaMemcpy(dst_d, src_h, len, cudaMemcpyHostToDevice);
}

void cuda_memcpy_dtod(cudamem_t *dst_d, cudamem_t const *src_d, int len)
{
	cudaMemcpy(dst_d, src_d, len, cudaMemcpyDeviceToDevice);
}

void cuda_memset(cudamem_t *dst, uint8_t c, int len)
{
	cudaMemset(dst, c, len);
}

__global__ void cu_init_cudaplugin(uint8_t *p)
{
	int i = blockIdx.x;
	int j = threadIdx.x;
	p[i * 9 + j] = (i + 1) * (j + 1);
}

CUDAIMAGE_API api;

#ifdef _WIN32
extern "C" __declspec(dllexport) CUDAIMAGE_API const *init_cudaplugin(int n)
#else
extern "C" CUDAIMAGE_API const *init_cudaplugin(int n)
#endif
{
	if (n != sizeof(CUDAIMAGE_API)) return nullptr;

	uint8_t table[81];
	uint8_t *mem;
	cudaMalloc((cudamem_t **)&mem, 81);
	dim3 b(9);
	dim3 t(9);
	cu_init_cudaplugin<<<b,t>>>(mem);
	cudaMemcpy(table, mem, 81, cudaMemcpyDeviceToHost);
	cudaFree(mem);

	for (int i = 0; i < 9; i++) {
		for (int j = 0; j < 9; j++) {
			if (table[9 * i + j] != (i + 1) * (j + 1)) {
				return nullptr;
			}
		}
	}

#define API_FUNC(NAME) api.NAME = cuda_##NAME

	API_FUNC(malloc);
	API_FUNC(free);
	API_FUNC(memcpy_htoh);
	API_FUNC(memcpy_dtoh);
	API_FUNC(memcpy_htod);
	API_FUNC(memcpy_dtod);
	API_FUNC(memset);
	API_FUNC(saturation_brightness);
	API_FUNC(round_brush);
	API_FUNC(fill_uint8_rgba);
	API_FUNC(fill_float_rgba);
	API_FUNC(copy_uint8_rgba);
	API_FUNC(blend_float_rgba);
	API_FUNC(blend_uint8_grayscale);
	API_FUNC(outline_uint8_grayscale);
	API_FUNC(compose_float_rgba);
	API_FUNC(scale_float_to_uint8_rgba);
	API_FUNC(scale);

	return &api;
}
