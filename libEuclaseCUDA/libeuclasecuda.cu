#include <stdio.h>
#include <limits>
#include "libeuclasecuda.h"
#include <cuda_fp16.h>

#define API_FUNC_ENTRY(NAME) cuda_##NAME
#define GAMMA (2.2f)
#define gamma(X) powf(X, 1 / GAMMA)
#define degamma(X) powf(X, GAMMA)

static bool launchCompleted()
{
	return cudaGetLastError() == cudaSuccess && cudaStreamSynchronize(nullptr) == cudaSuccess;
}

static bool regionFits(int w, int h, int x, int y, int stride, int rows)
{
	return w > 0 && h > 0 && x >= 0 && y >= 0 && w <= stride && h <= rows
			&& x <= stride - w && y <= rows - h;
}

template <typename Launch>
static bool blendWithHostMask(int w, int h, const void *src, int src_w, int src_h, int sx, int sy,
							  const uint8_t *host_mask, int mask_w, int mask_h,
							  void *dst, int dst_w, int dst_h, int dx, int dy, Launch launch)
{
	constexpr int max_extent = std::numeric_limits<int>::max() - 15;
	if (!src || !dst || w > max_extent || h > max_extent
			|| !regionFits(w, h, sx, sy, src_w, src_h) || !regionFits(w, h, dx, dy, dst_w, dst_h)) {
		return false;
	}

	uint8_t *device_mask = nullptr;
	if (host_mask) {
		if (mask_w < w || mask_h < h || mask_h > std::numeric_limits<int>::max() / mask_w) return false;
		const size_t bytes = size_t(mask_w) * mask_h;
		if (cudaMalloc(&device_mask, bytes) != cudaSuccess) return false;
		if (cudaMemcpy(device_mask, host_mask, bytes, cudaMemcpyHostToDevice) != cudaSuccess) {
			cudaFree(device_mask);
			return false;
		}
	}

	const dim3 blocks((w + 15) / 16, (h + 15) / 16);
	const dim3 threads(16, 16);
	cudaGetLastError();
	launch(blocks, threads, device_mask);
	const bool completed = launchCompleted();
	const bool freed = !device_mask || cudaFree(device_mask) == cudaSuccess;
	return completed && freed;
}

__device__ inline uint8_t clamp_uint8(float x)
{
	return (uint8_t)max(0.0f, min(255.0f, x));
}

__device__ inline float clamp_f01(float x)
{
	return max(0.0f, min(1.0f, x));
}

__global__ void cu_round_brush(int w, int h, float cx, float cy, float radius, float blur, float mul, __half *p)
{
	int i = blockIdx.x * blockDim.x + threadIdx.x;
	int j = blockIdx.y * blockDim.y + threadIdx.y;
	if (i < w && j < h) {
		float x = i + 0.5 - cx;
		float y = j + 0.5 - cy;

		float value = 0;
		float d = sqrtf(x * x + y * y);
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

bool API_FUNC_ENTRY(round_brush)(int w, int h, float cx, float cy, float radius, float blur, float mul, cudamem_t *mem)
{
	dim3 blocks((w + 15) / 16, (h + 15) / 16);
	dim3 threads(16, 16);
	cudaGetLastError();
	cu_round_brush<<<blocks,threads>>>(w, h, cx, cy, radius, blur, mul, (__half *)mem);
	return launchCompleted();
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

bool API_FUNC_ENTRY(saturation_brightness)(int w, int h, int red, int green, int blue, cudamem_t *mem)
{
	dim3 blocks((w + 15) / 16, (h + 15) / 16);
	dim3 threads(16, 16);
	cudaGetLastError();
	cu_saturation_brightness<<<blocks,threads>>>(w, h, red, green, blue, (uint8_t *)mem);
	return launchCompleted();
}

__global__ void cu_fill_uint8_rgb_kernel(int w, int h, uint8_t r, uint8_t g, uint8_t b, uint8_t a, uint8_t *dst, int dst_w, int dx, int dy)
{
	int x = blockIdx.x * blockDim.x + threadIdx.x;
	int y = blockIdx.y * blockDim.y + threadIdx.y;

	if (x < w && y < h) {
		uint8_t *d = dst + 3 * (dst_w * (dy + y) + dx + x);
		d[0] = r;
		d[1] = g;
		d[2] = b;
	}
}

bool API_FUNC_ENTRY(fill_uint8_rgb)(int w, int h, uint8_t r, uint8_t g, uint8_t b, uint8_t a, cudamem_t *dst, int dst_w, int dst_h, int dx, int dy)
{
	uint8_t *d = (uint8_t *)dst;

	dim3 blocks((w + 15) / 16, (h + 15) / 16);
	dim3 threads(16, 16);
	cudaGetLastError();
	cu_fill_uint8_rgb_kernel<<<blocks,threads>>>(w, h, r, g, b, a, d, dst_w, dx, dy);
	return launchCompleted();
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

bool API_FUNC_ENTRY(fill_uint8_rgba)(int w, int h, uint8_t r, uint8_t g, uint8_t b, uint8_t a, cudamem_t *dst, int dst_w, int dst_h, int dx, int dy)
{
	uint8_t *d = (uint8_t *)dst;

	dim3 blocks((w + 15) / 16, (h + 15) / 16);
	dim3 threads(16, 16);
	cudaGetLastError();
	cu_fill_uint8_rgba_kernel<<<blocks,threads>>>(w, h, r, g, b, a, d, dst_w, dx, dy);
	return launchCompleted();
}

__global__ void cu_fill_fp32_rgba_kernel(int w, int h, float r, float g, float b, float a, float *dst, int dst_w, int dx, int dy)
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

bool API_FUNC_ENTRY(fill_fp32_rgba)(int w, int h, float r, float g, float b, float a, cudamem_t *dst, int dst_w, int dst_h, int dx, int dy)
{
	float *d = (float *)dst;

	dim3 blocks((w + 15) / 16, (h + 15) / 16);
	dim3 threads(16, 16);
	cudaGetLastError();
	cu_fill_fp32_rgba_kernel<<<blocks,threads>>>(w, h, r, g, b, a, d, dst_w, dx, dy);
	return launchCompleted();
}

__global__ void cu_fill_fp16_rgba_kernel(int w, int h, float r, float g, float b, float a, __half *dst, int dst_w, int dx, int dy)
{
	int x = blockIdx.x * blockDim.x + threadIdx.x;
	int y = blockIdx.y * blockDim.y + threadIdx.y;

	if (x < w && y < h) {
		__half *d = dst + 4 * (dst_w * (dy + y) + dx + x);
		d[0] = r;
		d[1] = g;
		d[2] = b;
		d[3] = a;
	}
}

bool API_FUNC_ENTRY(fill_fp16_rgba)(int w, int h, float r, float g, float b, float a, cudamem_t *dst, int dst_w, int dst_h, int dx, int dy)
{
	__half *d = (__half *)dst;

	dim3 blocks((w + 15) / 16, (h + 15) / 16);
	dim3 threads(16, 16);
	cudaGetLastError();
	cu_fill_fp16_rgba_kernel<<<blocks,threads>>>(w, h, r, g, b, a, d, dst_w, dx, dy);
	return launchCompleted();
}

bool API_FUNC_ENTRY(copy_uint8_rgba)(int w, int h, const cudamem_t *src, int src_w, int src_h, int sx, int sy,
									 cudamem_t *dst, int dst_w, int dst_h, int dx, int dy)
{
	if (!src || !dst || !regionFits(w, h, sx, sy, src_w, src_h) || !regionFits(w, h, dx, dy, dst_w, dst_h)) {
		return false;
	}
	const auto *s = static_cast<const uint32_t *>(src) + size_t(src_w) * sy + sx;
	auto *d = static_cast<uint32_t *>(dst) + size_t(dst_w) * dy + dx;
	return cudaMemcpy2D(d, size_t(dst_w) * 4, s, size_t(src_w) * 4, size_t(w) * 4, h,
						cudaMemcpyDeviceToHost) == cudaSuccess;
}

__device__ void alpha_blend_fp32_RGBA(float *d, float const *s, float m)
{
	float baseR = d[0];
	float baseG = d[1];
	float baseB = d[2];
	float baseA = d[3];
	float overR = s[0];
	float overG = s[1];
	float overB = s[2];
	float overA = s[3];
	overA = overA * m;
	if (overA <= 0.0f) return;
	float r = overR * overA + baseR * baseA * (1 - overA);
	float g = overG * overA + baseG * baseA * (1 - overA);
	float b = overB * overA + baseB * baseA * (1 - overA);
	float a = overA + baseA * (1 - overA);
	if (a > 0) {
		r /= a;
		g /= a;
		b /= a;
	}
	d[0] = r;
	d[1] = g;
	d[2] = b;
	d[3] = a;
}

__device__ void alpha_blend_fp16_RGBA(__half *d, __half const *s, __half m)
{
	__half baseR = d[0];
	__half baseG = d[1];
	__half baseB = d[2];
	__half baseA = d[3];
	__half overR = s[0];
	__half overG = s[1];
	__half overB = s[2];
	__half overA = s[3];
	overA = overA * m;
	if (overA <= (__half)0) return;
	__half r = overR * overA + baseR * baseA * ((__half)1 - overA);
	__half g = overG * overA + baseG * baseA * ((__half)1 - overA);
	__half b = overB * overA + baseB * baseA * ((__half)1 - overA);
	__half a = overA + baseA * ((__half)1 - overA);
	if (a > (__half)0) {
		r /= a;
		g /= a;
		b /= a;
	}
	d[0] = r;
	d[1] = g;
	d[2] = b;
	d[3] = a;
}

__device__ void alpha_blend_float_GrayA(float *d, float const *s, float m)
{
	float baseV = d[0];
	float baseA = d[1];
	float overV = s[0];
	float overA = s[1];
	overA = overA * m;
	float r = overV * overA + baseV * baseA * (1 - overA);
	float a = overA + baseA * (1 - overA);
	if (a > 0) {
		r /= a;
	}
	d[0] = r;
	d[1] = a;
}

__global__ void cu_blend_fp32_RGBA_kernel(int w, int h, float const *src, int src_w, int sx, int sy, uint8_t const *mask, int mask_w, float *dst, int dst_w, int dx, int dy)
{
	int x = blockIdx.x * blockDim.x + threadIdx.x;
	int y = blockIdx.y * blockDim.y + threadIdx.y;

	if (x < w && y < h) {
		float const *s = src + 4 * (src_w * (sy + y) + sx + x);
		float *d = dst + 4 * (dst_w * (dy + y) + dx + x);
		float m = mask ? mask[mask_w * y + x] / 255.0f : 1.0f;
		alpha_blend_fp32_RGBA(d, s, m);
	}
}

__global__ void cu_blend_fp16_RGBA_kernel(int w, int h, __half const *src, int src_w, int sx, int sy, uint8_t const *mask, int mask_w, __half *dst, int dst_w, int dx, int dy)
{
	int x = blockIdx.x * blockDim.x + threadIdx.x;
	int y = blockIdx.y * blockDim.y + threadIdx.y;

	if (x < w && y < h) {
		__half const *s = src + 4 * (src_w * (sy + y) + sx + x);
		__half *d = dst + 4 * (dst_w * (dy + y) + dx + x);
		__half m = mask ? mask[mask_w * y + x] / 255.0f : 1.0f;
		alpha_blend_fp16_RGBA(d, s, m);
	}
}

bool API_FUNC_ENTRY(blend_fp32_RGBA)(int w, int h, const cudamem_t *src, int src_w, int src_h, int sx, int sy,
								const uint8_t *host_mask, int mask_w, int mask_h,
								cudamem_t *dst, int dst_w, int dst_h, int dx, int dy)
{
	return blendWithHostMask(w, h, src, src_w, src_h, sx, sy, host_mask, mask_w, mask_h, dst, dst_w, dst_h, dx, dy,
			[&](dim3 blocks, dim3 threads, const uint8_t *device_mask){
				cu_blend_fp32_RGBA_kernel<<<blocks, threads>>>(w, h, static_cast<const float *>(src), src_w, sx, sy,
														device_mask, mask_w, static_cast<float *>(dst), dst_w, dx, dy);
			});
}

bool API_FUNC_ENTRY(blend_fp16_RGBA)(int w, int h, const cudamem_t *src, int src_w, int src_h, int sx, int sy,
								const uint8_t *host_mask, int mask_w, int mask_h,
								cudamem_t *dst, int dst_w, int dst_h, int dx, int dy)
{
	return blendWithHostMask(w, h, src, src_w, src_h, sx, sy, host_mask, mask_w, mask_h, dst, dst_w, dst_h, dx, dy,
			[&](dim3 blocks, dim3 threads, const uint8_t *device_mask){
				cu_blend_fp16_RGBA_kernel<<<blocks, threads>>>(w, h, static_cast<const __half *>(src), src_w, sx, sy,
														device_mask, mask_w, static_cast<__half *>(dst), dst_w, dx, dy);
			});
}

__global__ void cu_erase_fp32_RGBA_kernel(int w, int h, float const *src, int src_w, int sx, int sy, uint8_t const *mask, int mask_w, float *dst, int dst_w, int dx, int dy)
{
	int x = blockIdx.x * blockDim.x + threadIdx.x;
	int y = blockIdx.y * blockDim.y + threadIdx.y;

	if (x < w && y < h) {
		float const *s = src + 4 * (src_w * (sy + y) + sx + x);
		float *d = dst + 4 * (dst_w * (dy + y) + dx + x);
		float m = mask ? mask[mask_w * y + x] / 255.0f : 1.0f;
		d[3] *= 1.0f - clamp_f01(s[3] * m);
	}
}

__global__ void cu_erase_fp16_RGBA_kernel(int w, int h, __half const *src, int src_w, int sx, int sy, uint8_t const *mask, int mask_w, __half *dst, int dst_w, int dx, int dy)
{
	int x = blockIdx.x * blockDim.x + threadIdx.x;
	int y = blockIdx.y * blockDim.y + threadIdx.y;

	if (x < w && y < h) {
		__half const *s = src + 4 * (src_w * (sy + y) + sx + x);
		__half *d = dst + 4 * (dst_w * (dy + y) + dx + x);
		__half m = mask ? mask[mask_w * y + x] / 255.0f : 1.0f;
		d[3] *= 1.0f - clamp_f01(s[3] * m);
	}
}

bool API_FUNC_ENTRY(erase_fp32_RGBA)(int w, int h, const cudamem_t *src, int src_w, int src_h, int sx, int sy,
								const uint8_t *host_mask, int mask_w, int mask_h,
								cudamem_t *dst, int dst_w, int dst_h, int dx, int dy)
{
	return blendWithHostMask(w, h, src, src_w, src_h, sx, sy, host_mask, mask_w, mask_h, dst, dst_w, dst_h, dx, dy,
			[&](dim3 blocks, dim3 threads, const uint8_t *device_mask){
				cu_erase_fp32_RGBA_kernel<<<blocks, threads>>>(w, h, static_cast<const float *>(src), src_w, sx, sy,
														device_mask, mask_w, static_cast<float *>(dst), dst_w, dx, dy);
			});
}

bool API_FUNC_ENTRY(erase_fp16_RGBA)(int w, int h, const cudamem_t *src, int src_w, int src_h, int sx, int sy,
								const uint8_t *host_mask, int mask_w, int mask_h,
								cudamem_t *dst, int dst_w, int dst_h, int dx, int dy)
{
	return blendWithHostMask(w, h, src, src_w, src_h, sx, sy, host_mask, mask_w, mask_h, dst, dst_w, dst_h, dx, dy,
			[&](dim3 blocks, dim3 threads, const uint8_t *device_mask){
				cu_erase_fp16_RGBA_kernel<<<blocks, threads>>>(w, h, static_cast<const __half *>(src), src_w, sx, sy,
														device_mask, mask_w, static_cast<__half *>(dst), dst_w, dx, dy);
			});
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

bool API_FUNC_ENTRY(blend_uint8_grayscale)(int w, int h, const cudamem_t *src, int src_w, int src_h, int sx, int sy,
								const uint8_t *host_mask, int mask_w, int mask_h,
								cudamem_t *dst, int dst_w, int dst_h, int dx, int dy)
{
	return blendWithHostMask(w, h, src, src_w, src_h, sx, sy, host_mask, mask_w, mask_h, dst, dst_w, dst_h, dx, dy,
			[&](dim3 blocks, dim3 threads, const uint8_t *device_mask){
				cu_blend_uint8_grayscale_kernel<<<blocks, threads>>>(w, h, static_cast<const uint8_t *>(src), src_w, sx, sy,
														device_mask, mask_w, static_cast<uint8_t *>(dst), dst_w, dx, dy);
			});
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

void API_FUNC_ENTRY(outline_uint8_grayscale)(int w, int h, cudamem_t const *src, cudamem_t *dst)
{
	uint8_t const *s = (uint8_t const *)src;
	uint8_t *d = (uint8_t *)dst;

	dim3 blocks((w + 15) / 16, (h + 15) / 16);
	dim3 threads(16, 16);
	cu_outline_uint8_grayscale_kernel<<<blocks,threads>>>(w, h, s, d);
}

bool API_FUNC_ENTRY(compose_fp32_rgba)(int w, int h, cudamem_t *dst, cudamem_t const *src, const uint8_t *host_mask)
{
	return cuda_blend_fp32_RGBA(w, h, src, w, h, 0, 0, host_mask, w, h, dst, w, h, 0, 0);
}

bool API_FUNC_ENTRY(compose_fp16_rgba)(int w, int h, cudamem_t *dst, cudamem_t const *src, const uint8_t *host_mask)
{
	return cuda_blend_fp16_RGBA(w, h, src, w, h, 0, 0, host_mask, w, h, dst, w, h, 0, 0);
}

__global__ void cu_scale_fp32_to_uint8_rgba_kernel(int dw, int dh, int dstride, uint8_t *dst, int sw, int sh, float const *src)
{
	int dx = blockIdx.x * blockDim.x + threadIdx.x;
	int dy = blockIdx.y * blockDim.y + threadIdx.y;

	if (dx < dw && dy < dh) {
		int sx = int(int64_t(dx) * sw / dw);
		int sy = int(int64_t(dy) * sh / dh);
		float const *s = src + 4 * (sw * sy + sx);
		uint8_t *d = dst + 4 * (dstride * dy + dx);
		float R = max(0.0f, min(1.0f, s[0]));
		float G = max(0.0f, min(1.0f, s[1]));
		float B = max(0.0f, min(1.0f, s[2]));
		float A = max(0.0f, min(1.0f, s[3]));
		d[0] = int(gamma(R) * 255 + 0.5f);
		d[1] = int(gamma(G) * 255 + 0.5f);
		d[2] = int(gamma(B) * 255 + 0.5f);
		d[3] = int(A * 255 + 0.5f);
	}
}

__global__ void cu_scale_fp16_to_uint8_rgba_kernel(int dw, int dh, int dstride, uint8_t *dst, int sw, int sh, __half const *src)
{
	int dx = blockIdx.x * blockDim.x + threadIdx.x;
	int dy = blockIdx.y * blockDim.y + threadIdx.y;

	if (dx < dw && dy < dh) {
		int sx = int(int64_t(dx) * sw / dw);
		int sy = int(int64_t(dy) * sh / dh);
		__half const *s = src + 4 * (sw * sy + sx);
		uint8_t *d = dst + 4 * (dstride * dy + dx);
		float R = max(0.0f, min(1.0f, s[0]));
		float G = max(0.0f, min(1.0f, s[1]));
		float B = max(0.0f, min(1.0f, s[2]));
		float A = max(0.0f, min(1.0f, s[3]));
		d[0] = int(gamma(R) * 255 + 0.5f);
		d[1] = int(gamma(G) * 255 + 0.5f);
		d[2] = int(gamma(B) * 255 + 0.5f);
		d[3] = int(A * 255 + 0.5f);
	}
}

bool API_FUNC_ENTRY(scale_fp32_to_uint8_rgba)(int dw, int dh, int dstride, cudamem_t *dst, int sw, int sh, cudamem_t const *src)
{
	dim3 blocks((dw + 15) / 16, (dh + 15) / 16);
	dim3 threads(16, 16);
	cudaGetLastError();
	cu_scale_fp32_to_uint8_rgba_kernel<<<blocks,threads>>>(dw, dh, dstride, (uint8_t *)dst, sw, sh, (float const *)src);
	return launchCompleted();
}

bool API_FUNC_ENTRY(scale_fp16_to_uint8_rgba)(int dw, int dh, int dstride, cudamem_t *dst, int sw, int sh, cudamem_t const *src)
{
	dim3 blocks((dw + 15) / 16, (dh + 15) / 16);
	dim3 threads(16, 16);
	cudaGetLastError();
	cu_scale_fp16_to_uint8_rgba_kernel<<<blocks,threads>>>(dw, dh, dstride, (uint8_t *)dst, sw, sh, (__half const *)src);
	return launchCompleted();
}

__global__ void cu_scale_kernel(int dw, int dh, int dstride, uint8_t *dst, int sw, int sh, int sstride, uint8_t const *src, int psize)
{
	int dx = blockIdx.x * blockDim.x + threadIdx.x;
	int dy = blockIdx.y * blockDim.y + threadIdx.y;

	if (dx < dw && dy < dh) {
		int sx = int(int64_t(dx) * sw / dw);
		int sy = int(int64_t(dy) * sh / dh);
		uint8_t const *s = src + sstride * sy + sx * psize;
		uint8_t *d = dst + dstride * dy + dx * psize;
		memcpy(d, s, psize);
	}
}

void API_FUNC_ENTRY(scale)(int dw, int dh, int dstride, cudamem_t *dst, int sw, int sh, int sstride, cudamem_t const *src, int psize)
{
	dim3 blocks((dw + 15) / 16, (dh + 15) / 16);
	dim3 threads(16, 16);
	cu_scale_kernel<<<blocks,threads>>>(dw, dh, dstride, (uint8_t *)dst, sw, sh, sstride, (uint8_t const *)src, psize);
}

cudamem_t *API_FUNC_ENTRY(malloc)(int len)
{
	cudamem_t *mem = nullptr;
	return len > 0 && cudaMalloc(&mem, len) == cudaSuccess ? mem : nullptr;
}

void API_FUNC_ENTRY(free)(cudamem_t *mem)
{
	cudaFree(mem);
}

bool API_FUNC_ENTRY(memcpy_htoh)(void *dst_h, void const *src_h, int len)
{
	return cudaMemcpy(dst_h, src_h, len, cudaMemcpyHostToHost) == cudaSuccess;
}

bool API_FUNC_ENTRY(memcpy_dtoh)(void *dst_h, cudamem_t const *src_d, int len)
{
	return cudaMemcpy(dst_h, src_d, len, cudaMemcpyDeviceToHost) == cudaSuccess;
}

bool API_FUNC_ENTRY(memcpy_htod)(cudamem_t *dst_d, void const *src_h, int len)
{
	return cudaMemcpy(dst_d, src_h, len, cudaMemcpyHostToDevice) == cudaSuccess;
}

bool API_FUNC_ENTRY(memcpy_dtod)(cudamem_t *dst_d, cudamem_t const *src_d, int len)
{
	return cudaMemcpy(dst_d, src_d, len, cudaMemcpyDeviceToDevice) == cudaSuccess;
}

bool API_FUNC_ENTRY(memset)(cudamem_t *dst, uint8_t c, int len)
{
	return cudaMemset(dst, c, len) == cudaSuccess && cudaStreamSynchronize(nullptr) == cudaSuccess;
}

__global__ void cu_init_cudaplugin(uint8_t *p)
{
	int i = blockIdx.x;
	int j = threadIdx.x;
	p[i * 9 + j] = (i + 1) * (j + 1);
}

CUDAIMAGE_API api;

#ifdef _WIN32
extern "C" __declspec(dllexport) CUDAIMAGE_API const *init_cudaplugin_v2(int n)
#else
extern "C" CUDAIMAGE_API const *init_cudaplugin_v2(int n)
#endif
{
	if (n != sizeof(CUDAIMAGE_API)) return nullptr;

	uint8_t table[81];
	uint8_t *mem = nullptr;
	if (cudaMalloc(&mem, sizeof(table)) != cudaSuccess) return nullptr;
	cudaGetLastError();
	cu_init_cudaplugin<<<9, 9>>>(mem);
	const bool ready = launchCompleted()
			&& cudaMemcpy(table, mem, sizeof(table), cudaMemcpyDeviceToHost) == cudaSuccess;
	const bool freed = cudaFree(mem) == cudaSuccess;
	if (!ready || !freed) return nullptr;

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
	API_FUNC(fill_uint8_rgb);
	API_FUNC(fill_uint8_rgba);
	API_FUNC(fill_fp32_rgba);
	API_FUNC(fill_fp16_rgba);
	API_FUNC(copy_uint8_rgba);
	API_FUNC(blend_fp32_RGBA);
	API_FUNC(blend_fp16_RGBA);
	API_FUNC(erase_fp32_RGBA);
	API_FUNC(erase_fp16_RGBA);
	API_FUNC(blend_uint8_grayscale);
	API_FUNC(outline_uint8_grayscale);
	API_FUNC(compose_fp32_rgba);
	API_FUNC(compose_fp16_rgba);
	API_FUNC(scale_fp32_to_uint8_rgba);
	API_FUNC(scale_fp16_to_uint8_rgba);
	API_FUNC(scale);

	return &api;
}
