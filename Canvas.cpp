#include "AlphaBlend.h"
#include "ApplicationGlobal.h"
#include "Canvas.h"
#include <QDateTime>
#include <QDebug>
#include <QElapsedTimer>
#include <QPainter>
#include <functional>
#include <mutex>
#include <omp.h>

#if !defined(_WIN32) && !defined(__APPLE__)
#include <x86intrin.h>
#endif

static int COMP(QPoint const &a, QPoint const &b)
{
	if (a.y() < b.y()) return -1;
	if (a.y() > b.y()) return 1;
	if (a.x() < b.x()) return -1;
	if (a.x() > b.x()) return 1;
	return 0;
}

Canvas::Panel *Canvas::findPanel(std::vector<Panel> const *panels, QPoint const &offset)
{
	int lo = 0;
	int hi = (int)panels->size(); // 上限の一つ後
	while (lo < hi) {
		int m = (lo + hi) / 2;
		Canvas::Panel const *p = &panels->at(m);
		Q_ASSERT(p && *p);
		int i = COMP(p->offset(), offset);
		if (i == 0) return const_cast<Canvas::Panel *>(p);
		if (i < 0) {
			lo = m + 1;
		} else {
			hi = m;
		}
	}
	return nullptr;
}

void Canvas::sortPanels(std::vector<Panel> *panels)
{
	std::sort(panels->begin(), panels->end(), [](Panel const &a, Panel const &b){
		return COMP(a.offset(), b.offset()) < 0;
	});
}

struct Canvas::Private {
	QSize size { 0, 0 };
	std::vector<LayerPtr> layers;
	int current_layer_index = 0;
	Canvas::Layer filtering_layer;
	Canvas::Layer selection_layer;
};

Canvas::Canvas()
	: m(new Private)
{
	m->layers.emplace_back(newLayer());
}

Canvas::~Canvas()
{
	delete m;
}



Canvas::LayerPtr Canvas::newLayer()
{
	return std::make_shared<Canvas::Layer>();
}

int Canvas::width() const
{
	return m->size.width();
}

int Canvas::height() const
{
	return m->size.height();
}

QSize Canvas::size() const
{
	return m->size;
}

void Canvas::setSize(const QSize &s)
{
	m->size = s;
}

Canvas::Layer *Canvas::layer(int index)
{
	return m->layers[index].get();
}

Canvas::Layer const *Canvas::layer(int index) const
{
	return m->layers[index].get();
}

Canvas::Layer *Canvas::current_layer()
{
	return layer(m->current_layer_index);
}

Canvas::Layer const *Canvas::current_layer() const
{
	return layer(m->current_layer_index);
}

Canvas::Layer *Canvas::selection_layer()
{
	return &m->selection_layer;
}

void Canvas::renderToSinglePanel(Panel *target_panel, QPoint const &target_offset, Panel const *input_panel, QPoint const &input_offset, Layer const *mask_layer, RenderOption const &opt, QColor const &brush_color, int opacity, bool *abort)
{
	if (!opt.use_mask) {
		mask_layer = nullptr;
	}

	const QPoint dst_org = target_offset + target_panel->offset();
	const QPoint src_org = input_offset + input_panel->offset();
	int dx0 = dst_org.x();
	int dy0 = dst_org.y();
	int dx1 = dx0 + target_panel->width();
	int dy1 = dy0 + target_panel->height();
	int sx0 = src_org.x();
	int sy0 = src_org.y();
	int sx1 = sx0 + input_panel->width();
	int sy1 = sy0 + input_panel->height();
	if (sx1 <= dx0) return;
	if (sy1 <= dy0) return;
	if (dx1 <= sx0) return;
	if (dy1 <= sy0) return;
	const int x0 = std::max(dx0, sx0);
	const int y0 = std::max(dy0, sy0);
	const int x1 = std::min(dx1, sx1);
	const int y1 = std::min(dy1, sy1);
	const int w = x1 - x0;
	const int h = y1 - y0;

	if (w < 1 || h < 1) return;

	euclase::Image const *input_image = input_panel->imagep();

	const auto srcfmt = input_image->format();
	const auto dstfmt = target_panel->imagep()->format();

	const int dx = x0 - dst_org.x();
	const int dy = y0 - dst_org.y();
	const int sx = x0 - src_org.x();
	const int sy = y0 - src_org.y();
	uint8_t *tmpmask = nullptr;
	euclase::Image *maskimg = nullptr;
	Panel maskpanel;
	if (mask_layer && mask_layer->panelCount() != 0) {
		maskpanel.imagep()->make(w, h, euclase::Image::Format_U8_Grayscale);
		maskpanel.imagep()->fill(euclase::k::black);
		maskpanel.setOffset(x0, y0);
		renderToEachPanels_internal_(&maskpanel, target_offset, *mask_layer, nullptr, Qt::white, 255, {}, abort);
		maskimg = maskpanel.imagep();
	} else {
		tmpmask = (uint8_t *)alloca(w);
		memset(tmpmask, 255, w);
	}

	if (srcfmt == euclase::Image::Format_U8_Grayscale) {
		QColor c = brush_color.isValid() ? brush_color : Qt::white;

		uint8_t invert = 0;
		if (opacity < 0) {
			opacity = -opacity;
			invert = 255;
		}

		auto memtype = target_panel->imagep()->memtype();

		if (input_image->memtype() == euclase::Image::CUDA || target_panel->imagep()->memtype() == euclase::Image::CUDA) {
			if (dstfmt == euclase::Image::Format_U8_Grayscale) {
#ifdef USE_CUDA
				euclase::Image in = input_image->toCUDA();
				euclase::Image *out = target_panel->imagep();
				out->memconvert(euclase::Image::CUDA);
				uint8_t const *mp = maskimg ? (uint8_t const *)maskimg->data() : nullptr;
				int mw = maskimg ? maskimg->width() : 0;
				int mh = maskimg ? maskimg->height() : 0;
				global->cuda->blend_uint8_grayscale(w, h
					, in.data(), in.width(), in.height()
					, sx, sy
					, mp, mw, mh
					, out->data(), out->width(), out->height()
					, dx, dy
					);
				out->memconvert(memtype);
#endif
				return;
			}
		}

		target_panel->imagep()->memconvert(euclase::Image::Host);
		if (dstfmt == euclase::Image::Format_U8_RGBA) {
			euclase::OctetRGBA color(c.red(), c.green(), c.blue());
			for (int i = 0; i < h; i++) {
				using Pixel = euclase::OctetRGBA;
				uint8_t const *msk = !maskimg ? tmpmask : maskimg->scanLine(i);
				uint8_t const *src = input_image->scanLine(sy + i);
				Pixel *dst = reinterpret_cast<Pixel *>(target_panel->imagep()->scanLine(dy + i));
				for (int j = 0; j < w; j++) {
					color.a = opacity * (src[sx + j] ^ invert) * msk[j] / (255 * 255);
					dst[dx + j] = AlphaBlend::blend(dst[dx + j], color);
				}
			}
		} else if (dstfmt == euclase::Image::Format_F32_RGBA) {
			euclase::Float32RGBA color((uint8_t)c.red(), (uint8_t)c.green(), (uint8_t)c.blue());
			for (int i = 0; i < h; i++) {
				using Pixel = euclase::Float32RGBA;
				uint8_t const *msk = !maskimg ? tmpmask : maskimg->scanLine(i);
				uint8_t const *src = input_image->scanLine(sy + i);
				Pixel *dst = reinterpret_cast<Pixel *>(target_panel->imagep()->scanLine(dy + i));
				for (int j = 0; j < w; j++) {
					color.a = opacity * (src[sx + j] ^ invert) * msk[j] / (255 * 255) / 255.0f;
					dst[dx + j] = AlphaBlend::blend(dst[dx + j], color);
				}
			}
		} else if (dstfmt == euclase::Image::Format_U8_Grayscale) {
			for (int y = 0; y < h; y++) {
				uint8_t const *msk = !maskimg ? tmpmask : maskimg->scanLine(y);
				uint8_t const *src = input_image->scanLine(sy + y);
				uint8_t *dst = target_panel->imagep()->scanLine(dy + y);
				for (int x = 0; x < w; x++) {
					uint8_t const *s = src + sx + x;
					uint8_t *d = dst + dx + x;
					uint8_t const *m = msk + x;
					*d = (*d * (255 - *m) + *s * *m) / 255;
				}
			}
		}
		target_panel->imagep()->memconvert(memtype);
		return;
	}

	if (srcfmt == euclase::Image::Format_F32_Grayscale) {
		QColor c = brush_color.isValid() ? brush_color : Qt::white;

		uint8_t invert = 0;
		if (opacity < 0) {
			opacity = -opacity;
			invert = 255;
		}

		auto memtype = target_panel->imagep()->memtype();
		target_panel->imagep()->memconvert(euclase::Image::Host);

		if (dstfmt == euclase::Image::Format_U8_RGBA) {
			euclase::OctetRGBA color(c.red(), c.green(), c.blue());
			for (int i = 0; i < h; i++) {
				using Pixel = euclase::OctetRGBA;
				uint8_t const *msk = !maskimg ? tmpmask : maskimg->scanLine(i);
				float const *src = (float const *)input_image->scanLine(sy + i);
				Pixel *dst = reinterpret_cast<Pixel *>(target_panel->imagep()->scanLine(dy + i));
				for (int j = 0; j < w; j++) {
					color.a = opacity * (uint8_t(floorf(src[sx + j] * 255.0f + 0.5)) ^ invert) * msk[j] / (255 * 255);
					dst[dx + j] = AlphaBlend::blend(dst[dx + j], color);
				}
			}
		} else if (dstfmt == euclase::Image::Format_F32_RGBA) {
			euclase::Float32RGBA color((uint8_t)c.red(), (uint8_t)c.green(), (uint8_t)c.blue());
			for (int i = 0; i < h; i++) {
				using Pixel = euclase::Float32RGBA;
				uint8_t const *msk = !maskimg ? tmpmask : maskimg->scanLine(i);
				float const *src = (float const *)input_image->scanLine(sy + i);
				Pixel *dst = reinterpret_cast<Pixel *>(target_panel->imagep()->scanLine(dy + i));
				for (int j = 0; j < w; j++) {
					color.a = opacity * (uint8_t(floorf(src[sx + j] * 255.0f + 0.5)) ^ invert) * msk[j] / (255 * 255) / 255.0f;
					dst[dx + j] = AlphaBlend::blend(dst[dx + j], color);
				}
			}
		} else if (dstfmt == euclase::Image::Format_U8_Grayscale) {
			for (int y = 0; y < h; y++) {
				uint8_t const *msk = !maskimg ? tmpmask : maskimg->scanLine(y);
				float const *src = (float const *)input_image->scanLine(sy + y);
				uint8_t *dst = target_panel->imagep()->scanLine(dy + y);
				for (int x = 0; x < w; x++) {
					float const *s = src + sx + x;
					uint8_t *d = dst + dx + x;
					uint8_t const *m = msk + x;
					*d = (*d * (255 - *m) + *s * 255.0f * *m) / 255;
				}
			}
		}

		target_panel->imagep()->memconvert(memtype);
		return;
	}

	if (dstfmt == euclase::Image::Format_U8_RGBA) {

		auto RenderRGBA8888 = [](uint8_t *dst, uint8_t const *src, uint8_t const *msk, int w, RenderOption const &opt){
			for (int j = 0; j < w; j++) {
				euclase::OctetRGBA color = ((euclase::OctetRGBA const *)src)[j];
				color.a = color.a * msk[j] / 255;
				((euclase::OctetRGBA *)dst)[j] = AlphaBlend::blend(((euclase::OctetRGBA *)dst)[j], color);
			}
		};

		auto RenderRGBAF = [](uint8_t *dst, uint8_t const *src, uint8_t const *msk, int w, RenderOption const &opt){
			for (int j = 0; j < w; j++) {
				euclase::OctetRGBA *d = ((euclase::OctetRGBA *)dst) + j;
				euclase::Float32RGBA color = ((euclase::Float32RGBA const *)src)[j];
				if (color.a == 1.0f && msk[j] == 255 && d->a == 0) {
					*d = euclase::OctetRGBA::convert(color);
				} else {
					color.a = color.a * msk[j] / 255;
					euclase::Float32RGBA base = euclase::Float32RGBA::convert(*d);
					*d = euclase::OctetRGBA::convert(AlphaBlend::blend(base, color));
				}
			}
		};


		auto Do = [&](euclase::Image const *inputimg, euclase::Image *outputimg){
			std::function<void(uint8_t *dst, uint8_t const *src, uint8_t const *msk, int w, RenderOption const &opt)> renderer;

			const int sstep = euclase::bytesPerPixel(srcfmt);
			const int dstep = euclase::bytesPerPixel(target_panel->image().format());
			if (inputimg->memtype() == euclase::Image::Host) {
				if (inputimg->format() == euclase::Image::Format_U8_RGBA) {
					renderer = RenderRGBA8888;
				} else if (inputimg->format() == euclase::Image::Format_F32_RGBA) {
					renderer = RenderRGBAF;
				}
				if (renderer) {
					for (int i = 0; i < h; i++) {
						uint8_t const *msk = !maskimg ? tmpmask : maskimg->scanLine(i);
						uint8_t const *src = inputimg->scanLine(sy + i);
						uint8_t *dst = outputimg->scanLine(dy + i);
						renderer(dst + dstep * dx, src + sstep * sx, msk, w, opt);
					}
					return;
				}
			} else {
				Q_ASSERT(0);
			}
		};

		euclase::Image in = input_image->convertToFormat(euclase::Image::Format_U8_RGBA).toHost();
		euclase::Image out = target_panel->image().convertToFormat(euclase::Image::Format_U8_RGBA).toHost();
		Do(&in, &out);
		out.memconvert(target_panel->image().memtype());
		*target_panel->imagep() = out;

		return;
	}

	if (dstfmt == euclase::Image::Format_F32_RGBA) {
		if (srcfmt == euclase::Image::Format_F32_RGBA || srcfmt == euclase::Image::Format_F16_RGBA) {
			if (target_panel->image().memtype() == euclase::Image::CUDA) {
#ifdef USE_CUDA
				auto memtype = target_panel->imagep()->memtype();
				euclase::Image in = *input_image;
				euclase::Image *out = target_panel->imagep();
				if (srcfmt != euclase::Image::Format_F32_RGBA) {
					in = in.convertToFormat(euclase::Image::Format_F32_RGBA);
				}
				in = in.toCUDA();
				cudamem_t const *src = in.data();
				int src_w = in.width();
				int src_h = in.height();
				uint8_t const *mask = nullptr;
				int mask_w = 0;
				int mask_h = 0;
				if (maskimg) {
					mask = (uint8_t const *)maskimg->data();
					mask_w = maskimg->width();
					mask_h = maskimg->height();
				}
				cudamem_t *dst = out->data();
				int dst_w = out->width();
				int dst_h = out->height();
				global->cuda->blend_fp32_RGBA(w, h, src, src_w, src_h, sx, sy, mask, mask_w, mask_h, dst, dst_w, dst_h, dx, dy);
				target_panel->imagep()->memconvert(memtype);
#endif
				return;
			}
		}

		const int dstep = euclase::bytesPerPixel(dstfmt);
		const int sstep = euclase::bytesPerPixel(srcfmt);
		if (srcfmt == euclase::Image::Format_U8_RGBA) {
			auto render = [](uint8_t *dst, uint8_t const *src, uint8_t const *msk, int w){
				for (int j = 0; j < w; j++) {
					euclase::Float32RGBA color = euclase::Float32RGBA::convert(((euclase::OctetRGBA const *)src)[j]);
					color.a = color.a * msk[j] / 255;
					((euclase::Float32RGBA *)dst)[j] = AlphaBlend::blend(((euclase::Float32RGBA *)dst)[j], color);
				}
			};
			for (int i = 0; i < h; i++) {
				uint8_t const *msk = !maskimg ? tmpmask : maskimg->scanLine(i);
				uint8_t const *src = input_image->scanLine(sy + i);
				uint8_t *dst = target_panel->imagep()->scanLine(dy + i);
				render(dst + dstep * dx, src + sstep * sx, msk, w);
			}
			return;
		}
		if (srcfmt == euclase::Image::Format_F32_RGBA) {
			euclase::Image in = *input_image;
			euclase::Image out = target_panel->image();
			in = in.toHost();
			float const *src = (float const *)in.data();
			float *dst = (float *)out.data();
			int src_stride = in.width();
			int dst_stride = out.width();
			uint8_t const *mask = maskimg ? (uint8_t const *)maskimg->data() : nullptr;
			int mask_stride = maskimg ? maskimg->width() : 0;
			for (int y = 0; y < h; y++) {
				float const *s = src + 4 * (src_stride * (sy + y) + sx);
				float *d = dst + 4 * (dst_stride * (dy + y) + dx);
				switch (opt.blend_mode) {
				case BlendMode::Normal:
					for (int x = 0; x < w; x++) {
#if 1
						uint8_t m = mask ? *(mask + mask_stride * y + x) : 255;
						euclase::Float32RGBA t = *(euclase::Float32RGBA *)d;
						euclase::Float32RGBA u = *(euclase::Float32RGBA *)s;
						u.a = u.a * m / 255;
						t = AlphaBlend::blend(t, u);
						d[0] = std::min(std::max(t.r, 0.0f), 1.0f);
						d[1] = std::min(std::max(t.g, 0.0f), 1.0f);
						d[2] = std::min(std::max(t.b, 0.0f), 1.0f);
						d[3] = std::min(std::max(t.a, 0.0f), 1.0f);
#else
						float m = mask ? mask[mask_stride * y + x] / 255.0f : 1.0f;
						if (m > 0) {
							float a = s[3] + d[3] * (1 - s[3]);
							__m128 base = _mm_load_ps(d);
							__m128 over = _mm_load_ps(s);
							__m128 base_a = _mm_load1_ps(d + 3);
							__m128 over_a = _mm_set1_ps(s[3] * m);
							__m128 rgb = _mm_add_ps(_mm_mul_ps(over, over_a), _mm_mul_ps(_mm_mul_ps(base, base_a), _mm_sub_ps(_mm_set1_ps(1.0f), over_a)));
							__m128 A = _mm_set1_ps(a);
							if (a > 0) {
								rgb = _mm_mul_ps(rgb, A);
							}
							__m128 zero = _mm_set1_ps(0.0f);
							__m128 one = _mm_set1_ps(1.0f);
							rgb = _mm_max_ps(rgb, zero);
							rgb = _mm_min_ps(rgb, one);
							__m128 v = _mm_insert_ps(rgb, A, 0xf0);
							_mm_store_ps(d, v);
						}
#endif
						s += 4;
						d += 4;
					}
					break;
				case BlendMode::Eraser:
					for (int x = 0; x < w; x++) {
						uint8_t m = mask ? *(mask + mask_stride * y + x) : 255;
						float overR = s[0];
						float overG = s[1];
						float overB = s[2];
						float overA = s[3];
						float v = euclase::grayf(overR, overG, overB) * overA * m / 255;
						d[3] *= 1 - euclase::clamp_f32(v);
						s += 4;
						d += 4;
					}
					break;
				}
			}
			in = in.toHost();
			out = out.toHost();
			return;
		} else if (srcfmt == euclase::Image::Format_F16_RGBA) {
			euclase::Image in = *input_image;
			euclase::Image out = target_panel->image();
			in = in.toHost();
			euclase::_float16_t const *src = (euclase::_float16_t const *)in.data();
			float *dst = (float *)out.data();
			int src_stride = in.width();
			int dst_stride = out.width();
			uint8_t const *mask = maskimg ? (uint8_t const *)maskimg->data() : nullptr;
			int mask_stride = maskimg ? maskimg->width() : 0;
			for (int y = 0; y < h; y++) {
				euclase::_float16_t const *s = src + 4 * (src_stride * (sy + y) + sx);
				float *d = dst + 4 * (dst_stride * (dy + y) + dx);
				switch (opt.blend_mode) {
				case BlendMode::Normal:
					for (int x = 0; x < w; x++) {
						uint8_t m = mask ? *(mask + mask_stride * y + x) : 255;
						euclase::Float32RGBA t = *(euclase::Float32RGBA *)d;
						euclase::Float32RGBA u = *(euclase::Float16RGBA *)s;
						u.a = u.a * m / 255;
						t = AlphaBlend::blend(t, u);
						d[0] = std::min(std::max((float)t.r, 0.0f), 1.0f);
						d[1] = std::min(std::max((float)t.g, 0.0f), 1.0f);
						d[2] = std::min(std::max((float)t.b, 0.0f), 1.0f);
						d[3] = std::min(std::max((float)t.a, 0.0f), 1.0f);
						s += 4;
						d += 4;
					}
					break;
				case BlendMode::Eraser:
					for (int x = 0; x < w; x++) {
						uint8_t m = mask ? *(mask + mask_stride * y + x) : 255;
						float overR = s[0];
						float overG = s[1];
						float overB = s[2];
						float overA = s[3];
						float v = euclase::grayf(overR, overG, overB) * overA * m / 255;
						d[3] = (float)d[3] * (1.0f - euclase::clamp_f16(v));
						s += 4;
						d += 4;
					}
					break;
				}
			}
			in = in.toHost();
			out = out.toHost();
			return;
		}
	}







	if (dstfmt == euclase::Image::Format_F16_RGBA) {
		if (srcfmt == euclase::Image::Format_F32_RGBA || srcfmt == euclase::Image::Format_F16_RGBA) {
			if (target_panel->image().memtype() == euclase::Image::CUDA) {
#ifdef USE_CUDA
				auto memtype = target_panel->imagep()->memtype();
				euclase::Image in = *input_image;
				euclase::Image *out = target_panel->imagep();
				if (srcfmt != euclase::Image::Format_F16_RGBA) {
					in = in.convertToFormat(euclase::Image::Format_F16_RGBA);
				}
				in = in.toCUDA();
				cudamem_t const *src = in.data();
				int src_w = in.width();
				int src_h = in.height();
				uint8_t const *mask = nullptr;
				int mask_w = 0;
				int mask_h = 0;
				if (maskimg) {
					mask = (uint8_t const *)maskimg->data();
					mask_w = maskimg->width();
					mask_h = maskimg->height();
				}
				cudamem_t *dst = out->data();
				int dst_w = out->width();
				int dst_h = out->height();
				global->cuda->blend_fp16_RGBA(w, h, src, src_w, src_h, sx, sy, mask, mask_w, mask_h, dst, dst_w, dst_h, dx, dy);
				target_panel->imagep()->memconvert(memtype);
#endif
				return;
			}
		}

		const int dstep = euclase::bytesPerPixel(dstfmt);
		const int sstep = euclase::bytesPerPixel(srcfmt);
		if (srcfmt == euclase::Image::Format_U8_RGBA) {
			auto render = [](uint8_t *dst, uint8_t const *src, uint8_t const *msk, int w){
				for (int j = 0; j < w; j++) {
					euclase::Float16RGBA color = euclase::Float16RGBA::convert(((euclase::OctetRGBA const *)src)[j]);
					color.a = color.a * msk[j] / 255;
					((euclase::Float16RGBA *)dst)[j] = AlphaBlend::blend(((euclase::Float16RGBA *)dst)[j], color);
				}
			};
			for (int i = 0; i < h; i++) {
				uint8_t const *msk = !maskimg ? tmpmask : maskimg->scanLine(i);
				uint8_t const *src = input_image->scanLine(sy + i);
				uint8_t *dst = target_panel->imagep()->scanLine(dy + i);
				render(dst + dstep * dx, src + sstep * sx, msk, w);
			}
			return;
		}
		if (srcfmt == euclase::Image::Format_F32_RGBA) {
			euclase::Image in = *input_image;
			euclase::Image out = target_panel->image();
			in = in.toHost();
			float const *src = (float const *)in.data();
			euclase::_float16_t *dst = (euclase::_float16_t *)out.data();
			int src_stride = in.width();
			int dst_stride = out.width();
			uint8_t const *mask = maskimg ? (uint8_t const *)maskimg->data() : nullptr;
			int mask_stride = maskimg ? maskimg->width() : 0;
			for (int y = 0; y < h; y++) {
				float const *s = src + 4 * (src_stride * (sy + y) + sx);
				euclase::_float16_t *d = dst + 4 * (dst_stride * (dy + y) + dx);
				switch (opt.blend_mode) {
				case BlendMode::Normal:
					for (int x = 0; x < w; x++) {
						uint8_t m = mask ? *(mask + mask_stride * y + x) : 255;
						euclase::Float16RGBA t = *(euclase::Float16RGBA *)d;
						euclase::Float16RGBA u(*(euclase::Float32RGBA const *)s);
						u.a = u.a * m / 255;
						t = AlphaBlend::blend(t, u);
						d[0] = std::min(std::max((float)t.r, 0.0f), 1.0f);
						d[1] = std::min(std::max((float)t.g, 0.0f), 1.0f);
						d[2] = std::min(std::max((float)t.b, 0.0f), 1.0f);
						d[3] = std::min(std::max((float)t.a, 0.0f), 1.0f);
						s += 4;
						d += 4;
					}
					break;
				case BlendMode::Eraser:
					for (int x = 0; x < w; x++) {
						uint8_t m = mask ? *(mask + mask_stride * y + x) : 255;
						float overR = s[0];
						float overG = s[1];
						float overB = s[2];
						float overA = s[3];
						float v = euclase::grayf(overR, overG, overB) * overA * m / 255;
						d[3] = (float)d[3] * (1 - euclase::clamp_f16(v));
						s += 4;
						d += 4;
					}
					break;
				}
			}
			in = in.toHost();
			out = out.toHost();
			return;
		} else if (srcfmt == euclase::Image::Format_F16_RGBA) {
			euclase::Image in = *input_image;
			euclase::Image out = target_panel->image();
			in = in.toHost();
			euclase::_float16_t const *src = (euclase::_float16_t const *)in.data();
			euclase::_float16_t *dst = (euclase::_float16_t *)out.data();
			int src_stride = in.width();
			int dst_stride = out.width();
			uint8_t const *mask = maskimg ? (uint8_t const *)maskimg->data() : nullptr;
			int mask_stride = maskimg ? maskimg->width() : 0;
			for (int y = 0; y < h; y++) {
				euclase::_float16_t const *s = src + 4 * (src_stride * (sy + y) + sx);
				euclase::_float16_t *d = dst + 4 * (dst_stride * (dy + y) + dx);
				switch (opt.blend_mode) {
				case BlendMode::Normal:
					for (int x = 0; x < w; x++) {
						uint8_t m = mask ? *(mask + mask_stride * y + x) : 255;
						euclase::Float16RGBA t = *(euclase::Float16RGBA *)d;
						euclase::Float16RGBA u = *(euclase::Float16RGBA *)s;
						u.a = u.a * m / 255;
						t = AlphaBlend::blend(t, u);
						d[0] = std::min(std::max((float)t.r, 0.0f), 1.0f);
						d[1] = std::min(std::max((float)t.g, 0.0f), 1.0f);
						d[2] = std::min(std::max((float)t.b, 0.0f), 1.0f);
						d[3] = std::min(std::max((float)t.a, 0.0f), 1.0f);
						s += 4;
						d += 4;
					}
					break;
				case BlendMode::Eraser:
					for (int x = 0; x < w; x++) {
						uint8_t m = mask ? *(mask + mask_stride * y + x) : 255;
						float overR = s[0];
						float overG = s[1];
						float overB = s[2];
						float overA = s[3];
						float v = euclase::grayf(overR, overG, overB) * overA * m / 255;
						d[3] = (float)d[3] * (1.0f - euclase::clamp_f16(v));
						s += 4;
						d += 4;
					}
					break;
				}
			}
			in = in.toHost();
			out = out.toHost();
			return;
		}
	}









	if (dstfmt == euclase::Image::Format_U8_Grayscale) {

		auto RenderRGBA8888 = [](uint8_t *dst, uint8_t const *src, uint8_t const *msk, int w, RenderOption const &opt){
			for (int j = 0; j < w; j++) {
				euclase::OctetRGBA color = ((euclase::OctetRGBA const *)src)[j];
				color.a = color.a * msk[j] / 255;
				euclase::OctetGrayA d(dst[j]);
				d = AlphaBlend::blend(euclase::OctetRGBA(d), color);
				dst[j] = d.gray();
			}
			(void)opt;
		};

		std::function<void(uint8_t *dst, uint8_t const *src, uint8_t const *msk, int w, RenderOption const &opt)> renderer;

		const int dstep = euclase::bytesPerPixel(dstfmt);
		const int sstep = euclase::bytesPerPixel(srcfmt);
		if (srcfmt == euclase::Image::Format_U8_RGBA) {
			renderer = RenderRGBA8888;
		}

		if (renderer) {
			for (int i = 0; i < h; i++) {
				uint8_t const *msk = !maskimg ? tmpmask : maskimg->scanLine(i);
				uint8_t const *src = input_image->scanLine(sy + i);
				uint8_t *dst = target_panel->imagep()->scanLine(dy + i);
				renderer(dst + dstep * dx, src + sstep * sx, msk, w, opt);
			}
			return;
		}
	}
}

void Canvas::composePanel(Panel *target_panel, Panel const *alt_panel, Panel const *alt_mask, RenderOption const &opt)
{
	// TODO:
	if (target_panel->format() == euclase::Image::Format_F16_RGBA) {
		if (alt_panel->format() == euclase::Image::Format_F32_RGBA) {
			target_panel->convertToFormat(euclase::Image::Format_F32_RGBA);
			composePanel(target_panel, alt_panel, alt_mask, opt);
			target_panel->convertToFormat(euclase::Image::Format_F16_RGBA);
			return;
		}
	}

	//	if (!alt_panel) return;
	Q_ASSERT(target_panel);
	Q_ASSERT(target_panel->format() == alt_panel->format());

	if (opt.blend_mode == BlendMode::Normal) {
#ifdef USE_CUDA
		if (target_panel->imagep()->memtype() == euclase::Image::CUDA) {
			Q_ASSERT(alt_panel->imagep()->memtype() == euclase::Image::CUDA);
			euclase::Image *dst = target_panel->imagep();
			euclase::Image const *src = alt_panel->imagep();
			euclase::Image mask;
			cudamem_t *m = nullptr;
			if (alt_mask) {
				mask = alt_mask->imagep()->toCUDA();
				m = mask.data();
			}
			if (target_panel->format() == euclase::Image::Format_F16_RGBA) {
				auto compose_fp16 = [](euclase::Image *dst, euclase::Image const *src, cudamem_t const *m){
					global->cuda->blend_fp16_RGBA(PANEL_SIZE, PANEL_SIZE, src->data(), PANEL_SIZE, PANEL_SIZE, 0, 0, m, PANEL_SIZE, PANEL_SIZE, dst->data(), PANEL_SIZE, PANEL_SIZE, 0, 0);
				};
				compose_fp16(dst, src, m);
			} else if (target_panel->format() == euclase::Image::Format_F32_RGBA) {
				auto compose_fp32 = [](euclase::Image *dst, euclase::Image const *src, cudamem_t const *m){
					global->cuda->blend_fp32_RGBA(PANEL_SIZE, PANEL_SIZE, src->data(), PANEL_SIZE, PANEL_SIZE, 0, 0, m, PANEL_SIZE, PANEL_SIZE, dst->data(), PANEL_SIZE, PANEL_SIZE, 0, 0);
				};
				compose_fp32(dst, src, m);
			}
			return;
		}
#endif
		if (target_panel->format() == euclase::Image::Format_F16_RGBA) {
			euclase::Float16RGBA *dst = (euclase::Float16RGBA *)target_panel->imagep()->data();
			euclase::Float16RGBA const *src = (euclase::Float16RGBA const *)alt_panel->imagep()->data();
			uint8_t const *mask = (opt.use_mask && alt_mask) ? (uint8_t const *)(*alt_mask).imagep()->data() : nullptr;

			for (int i = 0; i < PANEL_SIZE * PANEL_SIZE; i++) {
				auto s = src[i];
				if (mask) {
					s.a = s.a * mask[i] / 255;
				}
				dst[i] = AlphaBlend::blend(dst[i], s);
			}
		} else if (target_panel->format() == euclase::Image::Format_F32_RGBA) {
			euclase::Float32RGBA *dst = (euclase::Float32RGBA *)target_panel->imagep()->data();
			euclase::Float32RGBA const *src = (euclase::Float32RGBA const *)alt_panel->imagep()->data();
			uint8_t const *mask = (opt.use_mask && alt_mask) ? (uint8_t const *)(*alt_mask).imagep()->data() : nullptr;

			for (int i = 0; i < PANEL_SIZE * PANEL_SIZE; i++) {
				auto s = src[i];
				if (mask) {
					s.a = s.a * mask[i] / 255;
				}
				dst[i] = AlphaBlend::blend(dst[i], s);
			}
		}
	} else if (opt.blend_mode == BlendMode::Eraser) {
#ifdef USE_CUDA
		if (target_panel->imagep()->memtype() == euclase::Image::CUDA) {
			Q_ASSERT(alt_panel->imagep()->memtype() == euclase::Image::CUDA);
			euclase::Image *dst = target_panel->imagep();
			euclase::Image const *src = alt_panel->imagep();
			euclase::Image mask;
			cudamem_t *m = nullptr;
			if (alt_mask) {
				mask = alt_mask->imagep()->toCUDA();
				m = mask.data();
			}
			if (target_panel->format() == euclase::Image::Format_F16_RGBA) {
				auto compose_fp16 = [](euclase::Image *dst, euclase::Image const *src, cudamem_t const *m){
					global->cuda->erase_fp16_RGBA(PANEL_SIZE, PANEL_SIZE, src->data(), PANEL_SIZE, PANEL_SIZE, 0, 0, m, PANEL_SIZE, PANEL_SIZE, dst->data(), PANEL_SIZE, PANEL_SIZE, 0, 0);
				};
				compose_fp16(dst, src, m);
			} else if (target_panel->format() == euclase::Image::Format_F32_RGBA) {
				auto compose_fp32 = [](euclase::Image *dst, euclase::Image const *src, cudamem_t const *m){
					global->cuda->erase_fp32_RGBA(PANEL_SIZE, PANEL_SIZE, src->data(), PANEL_SIZE, PANEL_SIZE, 0, 0, m, PANEL_SIZE, PANEL_SIZE, dst->data(), PANEL_SIZE, PANEL_SIZE, 0, 0);
				};
				compose_fp32(dst, src, m);
			}
			return;
		}
#endif
		if (target_panel->format() == euclase::Image::Format_F16_RGBA) {
			euclase::Float16RGBA *dst = (euclase::Float16RGBA *)target_panel->imagep()->data();
			euclase::Float16RGBA const *src = (euclase::Float16RGBA const *)alt_panel->imagep()->data();
			uint8_t const *mask = alt_mask ? (uint8_t const *)(*alt_mask).imagep()->data() : nullptr;

			for (int i = 0; i < PANEL_SIZE * PANEL_SIZE; i++) {
				float s = src[i].a; // 消しゴムはアルファ値のみを使う
				if (mask) {
					s = s * mask[i] / 255;
				}
				s = 1 - euclase::clamp_f16(s);
				dst[i].a = (float)dst[i].a * s;
			}
		} else if (target_panel->format() == euclase::Image::Format_F32_RGBA) {
			euclase::Float32RGBA *dst = (euclase::Float32RGBA *)target_panel->imagep()->data();
			euclase::Float32RGBA const *src = (euclase::Float32RGBA const *)alt_panel->imagep()->data();
			uint8_t const *mask = alt_mask ? (uint8_t const *)(*alt_mask).imagep()->data() : nullptr;

			for (int i = 0; i < PANEL_SIZE * PANEL_SIZE; i++) {
				float s = src[i].a; // 消しゴムはアルファ値のみを使う
				if (mask) {
					s = s * mask[i] / 255;
				}
				s = 1 - euclase::clamp_f32(s);
				dst[i].a *= s;
			}
		}
	}
}


void Canvas::composePanels(Panel *target_panel, std::vector<Panel> const *alternate_panels, std::vector<Panel> const *alternate_selection_panels, RenderOption const &opt)
{
	Q_ASSERT(target_panel);
	Q_ASSERT(alternate_panels);

	QPoint offset = target_panel->offset();

	Panel *alt_panel = findPanel(alternate_panels, offset);
	if (!alt_panel) {
		*target_panel = Panel();
	} else if (alternate_selection_panels) { // 選択領域があるとき
		Panel *alt_mask = findPanel(alternate_selection_panels, offset);
		if (alt_mask) {
			composePanel(target_panel, alt_panel, alt_mask, opt);
		}
	} else { // 選択領域がないときは全選択と同義
		*target_panel = *alt_panel;
	}
}

void Canvas::renderToEachPanels_internal_(Panel *target_panel, QPoint const &target_offset, Layer const &input_layer, Layer *mask_layer, QColor const &brush_color, int opacity, RenderOption const &opt, bool *abort)
{
	QRect r1(
		target_offset.x() + target_panel->offset().x(),
		target_offset.y() + target_panel->offset().y(),
		target_panel->width(),
		target_panel->height()
		);
	if (opt.mask_rect.isValid()) {
		r1 = r1.intersected(opt.mask_rect);
	}

	std::vector<Panel const *> panels;
	for (Panel const &input_panel : input_layer.primary_panels) {
		QRect r2(
			input_layer.offset().x() + input_panel.offset().x(),
			input_layer.offset().y() + input_panel.offset().y(),
			input_panel->width(),
			input_panel->height()
			);
		if (r1.intersects(r2)) {
			panels.push_back(&input_panel);
		}
	}

	for (size_t i = 0; i < panels.size(); i++) {
		Panel const *input_panel = panels[i];
		if (abort && *abort) continue;

		QPoint offset = input_panel->offset();

		Panel composed_panel;
		if (opt.active_panel == Canvas::AlternateLayer) { // プレビュー有効
			RenderOption opt2;
			opt2.use_mask = false;
			if (input_layer.alternate_blend_mode == BlendMode::Normal || input_layer.alternate_blend_mode == BlendMode::Eraser) { // ブラシなど
				Panel *alt_mask = nullptr;
				if (opt.use_mask && mask_layer) {
					alt_mask = findPanel(&mask_layer->primary_panels, offset);
					if (!alt_mask) { // マスクが存在していてマスクパネルが存在しない場合
						goto next; // 選択範囲外なのでcomposeは行わずにinput_panelをそのまま使う
					}
				}
				composed_panel = input_panel->copy();
				input_panel = &composed_panel;
				Panel *alt_panel = findPanel(&input_layer.alternate_panels, offset);
				if (alt_panel) {
					opt2.use_mask = opt.use_mask;
					opt2.blend_mode = input_layer.alternate_blend_mode;
					composePanel(&composed_panel, alt_panel, alt_mask, opt2);
				}
			} else if (input_layer.alternate_blend_mode == BlendMode::Replace) { // フィルタなど
				Panel *alt_mask = nullptr;
				if (input_layer.alternate_selection_panels.empty()) {
					// nop: 選択パネルが全く無いなら全選択として処理
				} else {
					alt_mask = findPanel(&input_layer.alternate_selection_panels, offset);
					if (!alt_mask) { // 選択パネルが存在しないなら
						goto next; // 選択範囲外なのでcomposeは行わずにinput_panelをそのまま使う
					}
					opt2.use_mask = true;
				}
				Panel *alt_panel = findPanel(&input_layer.alternate_panels, offset);
				if (alt_panel) {
					composed_panel = input_panel->copy();
					composePanel(&composed_panel, alt_panel, alt_mask, opt2);
					input_panel = &composed_panel;
				}
			}
		}
next:;
		RenderOption opt3 = opt;
		opt3.use_mask = false; // composeの工程で選択範囲のマスクは済んでいるので次はマスクは使わない
		renderToSinglePanel(target_panel, target_offset, input_panel, input_layer.offset(), nullptr, opt3, brush_color, opacity);
	}
}

void Canvas::renderToEachPanels(Panel *target_panel, QPoint const &target_offset, std::vector<Layer *> const &input_layers, Layer *mask_layer, QColor const &brush_color, int opacity, RenderOption const &opt, bool *abort)
{
	for (Layer *layer : input_layers) {
		renderToEachPanels_internal_(target_panel, target_offset, *layer, mask_layer, brush_color, opacity, opt, abort);
	}
}

void Canvas::renderToLayer(Layer *target_layer, ActivePanel activepanel, Layer const &input_layer, Layer *mask_layer, RenderOption const &opt, bool *abort)
{
	Q_ASSERT(input_layer.format_ != euclase::Image::Format_Invalid);
	std::vector<Panel> *targetpanels = target_layer->panels(activepanel);
	if (activepanel != Canvas::AlternateSelection) {
		target_layer->active_panel_ = activepanel;
	}
	target_layer->format_ = input_layer.format_;
	for (Panel const &input_panel : *input_layer.panels()) {
		if (input_panel.isImage()) {
			QPoint s0 = input_layer.offset() + input_panel.offset();
			QPoint s1 = s0 + QPoint(input_panel.width(), input_panel.height());
			for (int y = (s0.y() & ~(PANEL_SIZE - 1)); y < s1.y(); y += PANEL_SIZE) {
				for (int x = (s0.x() & ~(PANEL_SIZE - 1)); x < s1.x(); x += PANEL_SIZE) {
					if (abort && *abort) return;
					QPoint d0 = QPoint(x, y) - target_layer->offset();
					QPoint d1 = d0 + QPoint(PANEL_SIZE, PANEL_SIZE);
					for (int y2 = (d0.y() & ~(PANEL_SIZE - 1)); y2 < d1.y(); y2 += PANEL_SIZE) {
						for (int x2 = (d0.x() & ~(PANEL_SIZE - 1)); x2 < d1.x(); x2 += PANEL_SIZE) {
							QPoint pt(x2, y2);
							Panel *p = findPanel(targetpanels, pt);
							if (!p) {
								p = target_layer->addImagePanel(targetpanels, pt.x(), pt.y(), PANEL_SIZE, PANEL_SIZE, target_layer->format_, target_layer->memtype_);
								p->imagep()->fill(euclase::k::transparent);
							}
							renderToSinglePanel(p, target_layer->offset(), &input_panel, input_layer.offset(), mask_layer, opt, opt.brush_color, 255, abort);
						}
					}
				}
			}
			if (opt.notify_changed_rect){
				QRect rect(input_layer.offset() + input_panel.offset(), input_panel.size());
				opt.notify_changed_rect(rect);
			}
		}
	}
}

void Canvas::clearSelection()
{
	selection_layer()->clear();
	selection_layer()->memtype_ = euclase::Image::Host;//global->cuda ? euclase::Image::CUDA : euclase::Image::Host; //@ CUDA不安定(?)
}

void Canvas::clear()
{
	m->size = QSize();
	clearSelection();
	m->layers.clear();
	m->layers.emplace_back(newLayer());
}

void Canvas::paintToCurrentLayer(Layer const &source, RenderOption const &opt, bool *abort)
{
	renderToLayer(current_layer(), Canvas::PrimaryLayer, source, selection_layer(), opt, abort);
}

void Canvas::paintToCurrentAlternate(Layer const &source, RenderOption const &opt, bool *abort)
{
	renderToLayer(current_layer(), Canvas::AlternateLayer, source, opt.use_mask ? selection_layer() : nullptr, opt, abort);
}

void Canvas::addSelection(Layer const &source, RenderOption const &opt, bool *abort)
{
	RenderOption o = opt;
	o.brush_color = Qt::white;
	renderToLayer(selection_layer(), Canvas::PrimaryLayer, source, nullptr, o, abort);
}

void Canvas::subSelection(Layer const &source, RenderOption const &opt, bool *abort)
{
	RenderOption o = opt;
	o.brush_color = Qt::black;
	renderToLayer(selection_layer(), Canvas::PrimaryLayer, source, nullptr, o, abort);
}

Canvas::Panel Canvas::renderSelection(const QRect &r, bool *abort) const
{
	Panel panel;
	panel.imagep()->make(r.width(), r.height(), euclase::Image::Format_U8_Grayscale, /*selection_layer()->memtype_*/euclase::Image::Host, euclase::k::black);
	panel.setOffset(r.topLeft());
	std::vector<Layer *> layers;
	layers.push_back(const_cast<Canvas *>(this)->selection_layer());
	renderToEachPanels(&panel, QPoint(), layers, nullptr, QColor(), 255, {}, abort);
	return panel;
}

Canvas::Panel Canvas::renderToPanel(InputLayerMode input_layer_mode, euclase::Image::Format format, const QRect &r, QRect const &maskrect, ActivePanel activepanel, RenderOption const &opt, bool *abort) const
{
	if (r.width() < 1 || r.height() < 1) return {};

	RenderOption opt2 = opt;
	opt2.mask_rect = maskrect;
	opt2.active_panel = activepanel;

	Panel target_panel;
	target_panel.imagep()->make(r.width(), r.height(), format, const_cast<Canvas *>(this)->current_layer()->memtype_);
	target_panel.setOffset(r.topLeft());
	std::vector<Layer *> input_layers;
	Layer *mask_layer = nullptr;
	switch (input_layer_mode) {
	case Canvas::AllLayers:
		for (LayerPtr layer : m->layers) {
			input_layers.push_back(layer.get());
		}
		break;
	case Canvas::CurrentLayerOnly:
		input_layers.push_back(const_cast<Canvas *>(this)->current_layer());
		input_layers.back()->alternate_blend_mode = opt.blend_mode;
		break;
	}
	if (opt2.use_mask) {
		if (m->selection_layer.panels()->empty()) {
			opt2.use_mask = false; // 選択パネルが全く無いなら全選択として処理
		} else {
			mask_layer = &m->selection_layer;
		}
	}
	renderToEachPanels(&target_panel, QPoint(), input_layers, mask_layer, QColor(), 255, opt2, abort);
	return target_panel;
}

Canvas::Panel Canvas::crop(const QRect &r, bool *abort) const
{
	Panel panel;
	panel.imagep()->make(r.width(), r.height(), euclase::Image::Format_U8_RGBA);
	panel.setOffset(r.topLeft());
	std::vector<Layer *> layers;
	layers.push_back(const_cast<Canvas *>(this)->current_layer());
	renderToEachPanels(&panel, QPoint(), layers, const_cast<Canvas *>(this)->selection_layer(), QColor(), 255, {}, abort);
	return panel;
}

void Canvas::trim(const QRect &r)
{
	current_layer()->setOffset(current_layer()->offset() - r.topLeft());
	selection_layer()->setOffset(selection_layer()->offset() - r.topLeft());
	setSize(r.size());
}

/**
 * @brief Canvas::Layer::addPanel
 * @param panels
 * @param offset
 * @return
 *
 * パネルを追加
 */
Canvas::Panel *Canvas::Layer::addPanel(std::vector<Panel> *panels, Panel &&panel)
{
	size_t lo = 0;
	size_t hi = panels->size();
	if (hi > 0) {
		hi--;

		const QPoint pt = panel.offset();

		// 挿入先を二分検索
		while (lo < hi) {
			size_t m = (lo + hi) / 2;
			auto c = COMP(panels->at(m).offset(), pt);
			if (c > 0) {
				hi = m;
			} else if (c < 0) {
				lo = m + 1;
			} else {
				return nullptr; // 既にある
			}
		}
		if (COMP(panels->at(lo).offset(), pt) < 0) { // 検索結果の次の位置に挿入する場合
			lo++;
		}
	}
	auto it = panels->insert(panels->begin() + lo, panel);
	return &*it;
}

/**
 * @brief Canvas::Layer::addImagePanel
 * @param panels
 * @param x
 * @param y
 * @param w
 * @param h
 * @param format
 * @param memtype
 * @return
 *
 * パネルを追加
 */
Canvas::Panel *Canvas::Layer::addImagePanel(std::vector<Panel> *panels, int x, int y, int w, int h, euclase::Image::Format format, euclase::Image::MemoryType memtype)
{
	if (w < 1 || h < 1) return nullptr;

	Panel panel;
	panel->make(w, h, format, memtype);
	panel.setOffset(x, y);

	return addPanel(panels, std::move(panel));
}

void Canvas::Layer::setAlternateOption(BlendMode blendmode)
{
	alternate_blend_mode = blendmode;
}

void Canvas::Layer::finishAlternatePanels(bool apply, Layer *mask_layer, RenderOption const &opt)
{
	if (apply) {
		for (Panel const &panel : alternate_panels) {
			Panel *p = findPanel(&primary_panels, panel.offset());
			if (!p) {
				p = addImagePanel(&primary_panels, panel.offset().x(), panel.offset().y(), PANEL_SIZE, PANEL_SIZE, panel.format(), panel.image().memtype());
			}
			Panel *mask = nullptr;
			if (opt.use_mask && mask_layer) {
				mask = findPanel(&mask_layer->primary_panels, panel.offset());
				if (!mask) continue; // マスクが存在していてマスクパネルが存在しない場合、選択範囲外なのでcomposeは行わない
			}
			composePanel(p, &panel, mask, opt);
		}
	}

	active_panel_ = PrimaryLayer;
	alternate_panels.clear();
	alternate_selection_panels.clear();
}

QRect Canvas::Layer::rect() const
{
	QRect rect;
	const_cast<Layer *>(this)->eachPanel([&](Panel *p){
		if (p->imagep()->format() == euclase::Image::Format_U8_Grayscale) {
			int w = p->width();
			int h = p->height();
			int x0 = w;
			int y0 = h;
			int x1 = 0;
			int y1 = 0;
			for (int y = 0; y < h; y++) {
				uint8_t const *s = p->imagep()->scanLine(y);
				for (int x = 0; x < w; x++) {
					if (s[x] != 0) {
						x0 = std::min(x0, x);
						y0 = std::min(y0, y);
						x1 = std::max(x1, x);
						y1 = std::max(y1, y);
					}
				}
			}
			if (x0 < x1 && y0 < y1) {
				QRect r = QRect(x0, y0, x1 - x0 + 1, y1 - y0 + 1).translated(offset() + p->offset());
				if (rect.isNull()) {
					rect = r;
				} else {
					rect = rect.united(r);
				}
			}
		} else if (p->imagep()->format() == euclase::Image::Format_U8_RGBA) {
			int w = p->width();
			int h = p->height();
			int x0 = w;
			int y0 = h;
			int x1 = 0;
			int y1 = 0;
			for (int y = 0; y < h; y++) {
				euclase::OctetRGBA const *s = (euclase::OctetRGBA const *)p->imagep()->scanLine(y);
				for (int x = 0; x < w; x++) {
					if (s[x].a != 0) {
						x0 = std::min(x0, x);
						y0 = std::min(y0, y);
						x1 = std::max(x1, x);
						y1 = std::max(y1, y);
					}
				}
			}
			if (x0 < x1 && y0 < y1) {
				QRect r = QRect(x0, y0, x1 - x0 + 1, y1 - y0 + 1).translated(offset() + p->offset());
				if (rect.isNull()) {
					rect = r;
				} else {
					rect = rect.united(r);
				}
			}
		}
	});
	return rect;
}

static QImage makeBoundsImage(Bounds::Rectangle const &x, QRect const &rect)
{
	QImage image(rect.width(), rect.height(), QImage::Format_Grayscale8);
	image.fill(Qt::white);
	return image;
}

static QImage makeBoundsImage(Bounds::Ellipse const &x, QRect const &rect)
{
	int w = rect.width();
	int h = rect.height();
	QImage image(w, h, QImage::Format_Grayscale8);
	image.fill(Qt::black);
	QPainter pr(&image);
	pr.setPen(Qt::NoPen);
	pr.setBrush(Qt::white);
	pr.setRenderHint(QPainter::Antialiasing);
	pr.drawEllipse(0, 0, w, h);
	return image;
}

void Canvas::changeSelection(SelectionOperation op, const QRect &rect, Bounds::Type bounds_type)
{
	auto format = euclase::Image::Format_U8_Grayscale;
	Canvas::Layer layer;
	layer.format_ = format;
	layer.memtype_ = m->selection_layer.memtype_;
	Panel *panel = layer.addImagePanel(&layer.primary_panels, rect.x(), rect.y(), rect.width(), rect.height(), layer.format_, layer.memtype_);

	QImage image = std::visit([&](auto const &x){ return makeBoundsImage(x, rect); }, bounds_type);

	image = image.convertToFormat(QImage::Format_Grayscale8);
	image = image.scaled(rect.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
	panel->imagep()->setImage(image);

	RenderOption opt;

	switch (op) {
	case SelectionOperation::SetSelection:
		clearSelection();
		addSelection(layer, opt, nullptr);
		break;
	case SelectionOperation::AddSelection:
		addSelection(layer, opt, nullptr);
		break;
	case SelectionOperation::SubSelection:
		subSelection(layer, opt, nullptr);
		break;
	}
}

int Canvas::addNewLayer()
{
	int index = m->current_layer_index + 1;
	m->layers.insert(m->layers.begin() + index, newLayer());
	return index;
}

void Canvas::setCurrentLayer(int index)
{
	m->current_layer_index = index;
}

//
euclase::Image cropImage(const euclase::Image &srcimg, int sx, int sy, int sw, int sh)
{
	Canvas::Panel tmp1(srcimg);
	Canvas::Panel tmp2(euclase::Image(sw, sh, srcimg.format(), srcimg.memtype()));
	Canvas::renderToSinglePanel(&tmp2, QPoint(0, 0), &tmp1, QPoint(-sx, -sy), nullptr, {}, {});
	return tmp2.image();
}

