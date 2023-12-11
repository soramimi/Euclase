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

#ifndef _WIN32
#include <x86intrin.h>
#endif

static int COMP(QPoint const &a, QPoint const &b)
{
	if (a.y() < b.y()) return -1;
	if (a.y() > b.y()) return 1;
	if (a.x() < b.x()) return -1;
	if (a.x() > b.x()) return 1;
	return 0;
};

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
	QSize size;
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

Canvas::Layer *Canvas::current_layer()
{
	return layer(m->current_layer_index);
}

Canvas::Layer *Canvas::current_layer() const
{
	return const_cast<Canvas *>(this)->current_layer();
}

Canvas::Layer *Canvas::selection_layer()
{
	return &m->selection_layer;
}

Canvas::Layer *Canvas::selection_layer() const
{
	return &m->selection_layer;
}

void Canvas::renderToSinglePanel(Panel *target_panel, QPoint const &target_offset, Panel const *input_panel, QPoint const &input_offset, Layer const *mask_layer, RenderOption const &opt, QColor const &brush_color, int opacity, bool *abort)
{
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

	const int dx = x0 - dst_org.x();
	const int dy = y0 - dst_org.y();
	const int sx = x0 - src_org.x();
	const int sy = y0 - src_org.y();
	uint8_t *tmpmask = nullptr;
	euclase::Image *maskimg = nullptr;
	Panel maskpanel;
	if (mask_layer && mask_layer->panelCount() != 0) {
		maskpanel.imagep()->make(w, h, euclase::Image::Format_8_Grayscale);
		maskpanel.imagep()->fill(euclase::k::black);
		maskpanel.setOffset(x0, y0);
		renderToEachPanels_internal_(&maskpanel, target_offset, *mask_layer, nullptr, Qt::white, 255, {}, abort);
		maskimg = maskpanel.imagep();
	} else {
		tmpmask = (uint8_t *)alloca(w);
		memset(tmpmask, 255, w);
	}

	if (input_image->format() == euclase::Image::Format_8_Grayscale) {
		QColor c = brush_color.isValid() ? brush_color : Qt::white;

		uint8_t invert = 0;
		if (opacity < 0) {
			opacity = -opacity;
			invert = 255;
		}

		auto memtype = target_panel->imagep()->memtype();

#ifdef USE_CUDA
		if (input_image->memtype() == euclase::Image::CUDA || target_panel->imagep()->memtype() == euclase::Image::CUDA) {
			if (target_panel->imagep()->format() == euclase::Image::Format_8_Grayscale) {
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
				return;
			}
		}
#endif

		target_panel->imagep()->memconvert(euclase::Image::Host);
		if (target_panel->imagep()->format() == euclase::Image::Format_8_RGBA) {
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
		} else if (target_panel->imagep()->format() == euclase::Image::Format_F_RGBA) {
			euclase::FloatRGBA color((uint8_t)c.red(), (uint8_t)c.green(), (uint8_t)c.blue());
			for (int i = 0; i < h; i++) {
				using Pixel = euclase::FloatRGBA;
				uint8_t const *msk = !maskimg ? tmpmask : maskimg->scanLine(i);
				uint8_t const *src = input_image->scanLine(sy + i);
				Pixel *dst = reinterpret_cast<Pixel *>(target_panel->imagep()->scanLine(dy + i));
				for (int j = 0; j < w; j++) {
					color.a = opacity * (src[sx + j] ^ invert) * msk[j] / (255 * 255) / 255.0f;
					dst[dx + j] = AlphaBlend::blend(dst[dx + j], color);
				}
			}
		} else if (target_panel->imagep()->format() == euclase::Image::Format_8_Grayscale) {
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

	if (input_image->format() == euclase::Image::Format_F_Grayscale) {
		QColor c = brush_color.isValid() ? brush_color : Qt::white;

		uint8_t invert = 0;
		if (opacity < 0) {
			opacity = -opacity;
			invert = 255;
		}

		auto memtype = target_panel->imagep()->memtype();
		target_panel->imagep()->memconvert(euclase::Image::Host);

		if (target_panel->imagep()->format() == euclase::Image::Format_8_RGBA) {
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
		} else if (target_panel->imagep()->format() == euclase::Image::Format_F_RGBA) {
			euclase::FloatRGBA color((uint8_t)c.red(), (uint8_t)c.green(), (uint8_t)c.blue());
			for (int i = 0; i < h; i++) {
				using Pixel = euclase::FloatRGBA;
				uint8_t const *msk = !maskimg ? tmpmask : maskimg->scanLine(i);
				float const *src = (float const *)input_image->scanLine(sy + i);
				Pixel *dst = reinterpret_cast<Pixel *>(target_panel->imagep()->scanLine(dy + i));
				for (int j = 0; j < w; j++) {
					color.a = opacity * (uint8_t(floorf(src[sx + j] * 255.0f + 0.5)) ^ invert) * msk[j] / (255 * 255) / 255.0f;
					dst[dx + j] = AlphaBlend::blend(dst[dx + j], color);
				}
			}
		} else if (target_panel->imagep()->format() == euclase::Image::Format_8_Grayscale) {
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

	if (target_panel->imagep()->format() == euclase::Image::Format_8_RGBA) {

		auto RenderRGBA8888 = [](uint8_t *dst, uint8_t const *src, uint8_t const *msk, int w, RenderOption const &opt){
			if (opt.mode == RenderOption::DirectCopy) {
				memcpy(dst, src, sizeof(euclase::OctetRGBA) * w);
			} else {
				for (int j = 0; j < w; j++) {
					euclase::OctetRGBA color = ((euclase::OctetRGBA const *)src)[j];
					color.a = color.a * msk[j] / 255;
					((euclase::OctetRGBA *)dst)[j] = AlphaBlend::blend(((euclase::OctetRGBA *)dst)[j], color);
				}
			}
		};

		auto RenderRGBAF = [](uint8_t *dst, uint8_t const *src, uint8_t const *msk, int w, RenderOption const &opt){
			for (int j = 0; j < w; j++) {
				euclase::OctetRGBA *d = ((euclase::OctetRGBA *)dst) + j;
				euclase::FloatRGBA color = ((euclase::FloatRGBA const *)src)[j];
				if (color.a == 1.0f && msk[j] == 255 && d->a == 0) {
					*d = euclase::OctetRGBA::convert(color);
				} else {
					color.a = color.a * msk[j] / 255;
					euclase::FloatRGBA base = euclase::FloatRGBA::convert(*d);
					*d = euclase::OctetRGBA::convert(AlphaBlend::blend(base, color));
				}
			}
		};


		auto Do = [&](euclase::Image const *inputimg, euclase::Image *outputimg){
			std::function<void(uint8_t *dst, uint8_t const *src, uint8_t const *msk, int w, RenderOption const &opt)> renderer;

			const int sstep = euclase::bytesPerPixel(input_image->format());
			const int dstep = euclase::bytesPerPixel(target_panel->image().format());
			if (inputimg->memtype() == euclase::Image::Host) {
				if (inputimg->format() == euclase::Image::Format_8_RGBA) {
					renderer = RenderRGBA8888;
				} else if (inputimg->format() == euclase::Image::Format_F_RGBA) {
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

		euclase::Image in = input_image->convertToFormat(euclase::Image::Format_8_RGBA).toHost();
		euclase::Image out = target_panel->image().convertToFormat(euclase::Image::Format_8_RGBA).toHost();
		Do(&in, &out);
		out.memconvert(target_panel->image().memtype());
		*target_panel->imagep() = out;

		return;
	}

	if (target_panel->imagep()->format() == euclase::Image::Format_F_RGBA) {

#ifdef USE_CUDA
		if (input_image->format() == euclase::Image::Format_F_RGBA) {
			if (target_panel->image().memtype() == euclase::Image::CUDA) {
				auto memtype = target_panel->imagep()->memtype();
				euclase::Image in = input_image->toCUDA();
				euclase::Image *out = target_panel->imagep();
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
				global->cuda->blend_float_rgba(w, h, src, src_w, src_h, sx, sy, mask, mask_w, mask_h, dst, dst_w, dst_h, dx, dy);
				target_panel->imagep()->memconvert(memtype);
				return;
			}
		}
#endif

		const int dstep = euclase::bytesPerPixel(target_panel->imagep()->format());
		const int sstep = euclase::bytesPerPixel(input_image->format());
		if (input_image->format() == euclase::Image::Format_8_RGBA) {
			auto render = [](uint8_t *dst, uint8_t const *src, uint8_t const *msk, int w){
				for (int j = 0; j < w; j++) {
					euclase::FloatRGBA color = euclase::FloatRGBA::convert(((euclase::OctetRGBA const *)src)[j]);
					color.a = color.a * msk[j] / 255;
					((euclase::FloatRGBA *)dst)[j] = AlphaBlend::blend(((euclase::FloatRGBA *)dst)[j], color);
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
		if (input_image->format() == euclase::Image::Format_F_RGBA) {
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
				for (int x = 0; x < w; x++) {
#if 0
					uint8_t m = mask ? *(mask + mask_stride * y + x) : 255;
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
					d[0] = std::min(std::max(r, 0.0f), 1.0f);
					d[1] = std::min(std::max(g, 0.0f), 1.0f);
					d[2] = std::min(std::max(b, 0.0f), 1.0f);
					d[3] = std::min(std::max(a, 0.0f), 1.0f);
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
			}
			in = in.toHost();
			out = out.toHost();
			return;
		}
	}

	if (target_panel->imagep()->format() == euclase::Image::Format_8_Grayscale) {

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

		const int dstep = euclase::bytesPerPixel(target_panel->imagep()->format());
		const int sstep = euclase::bytesPerPixel(input_image->format());
		if (input_image->format() == euclase::Image::Format_8_RGBA) {
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

void Canvas::composePanel(Panel *target_panel, Panel const *alt_panel, Panel const *alt_mask)
{
	if (!alt_panel) return;
	Q_ASSERT(target_panel);
	Q_ASSERT(target_panel->format() == euclase::Image::Format_F_RGBA);
	Q_ASSERT(target_panel->format() == euclase::Image::Format_F_RGBA);
	Q_ASSERT(alt_mask->format() == euclase::Image::Format_8_Grayscale);

#ifdef USE_CUDA
	if (target_panel->imagep()->memtype() == euclase::Image::CUDA) {
		Q_ASSERT(alt_panel->imagep()->memtype() == euclase::Image::CUDA);
		euclase::Image *dst = target_panel->imagep();
		euclase::Image const *src = alt_panel->imagep();
		euclase::Image mask = alt_mask->imagep()->toCUDA();
		global->cuda->compose_float_rgba(PANEL_SIZE, PANEL_SIZE, dst->data(), src->data(), mask.data());
		return;
	}
#endif

	euclase::FloatRGBA *dst = (euclase::FloatRGBA *)target_panel->imagep()->data();
	euclase::FloatRGBA const *src = (euclase::FloatRGBA const *)alt_panel->imagep()->data();
	uint8_t const *mask = alt_mask ? (uint8_t const *)(*alt_mask).imagep()->data() : nullptr;
	for (int i = 0; i < PANEL_SIZE * PANEL_SIZE; i++) {
		uint8_t m = mask ? mask[i] : 255;
		if (m == 0) {
			// nop
		} else if (m == 255) {
			dst[i] = src[i];
		} else {
			auto t = dst[i];
			auto u = src[i];
			if (t.a == 0) {
				t = u;
				t.a = 0;
			} else if (u.a == 0) {
				u = t;
				u.a = 0;
			}
			float n = m / 255.0f;
			dst[i].r = (dst[i].r * (1.0f - n) + src[i].r * n);
			dst[i].g = (dst[i].g * (1.0f - n) + src[i].g * n);
			dst[i].b = (dst[i].b * (1.0f - n) + src[i].b * n);
			dst[i].a = (dst[i].a * (1.0f - n) + src[i].a * n);
		}
	}
}

void Canvas::composePanels(Panel *target_panel, std::vector<Panel> const *alternate_panels, std::vector<Panel> const *alternate_selection_panels)
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
			composePanel(target_panel, alt_panel, alt_mask);
		}
	} else { // 選択領域がないときは全選択と同義
		*target_panel = *alt_panel;
	}
}

void Canvas::renderToEachPanels_internal_(Panel *target_panel, QPoint const &target_offset, Layer const &input_layer, Layer *mask_layer, QColor const &brush_color, int opacity, RenderOption const &opt, bool *abort)
{
	if (mask_layer && mask_layer->panelCount() == 0) {
		mask_layer = nullptr;
	}

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
		if (opt.active_panel == Layer::Alternate) { // プレビュー有効
			if (input_layer.alternate_selection_panels.empty()) { // 選択が全く無いなら全選択として処理
				Panel *alt_panel = findPanel(&input_layer.alternate_panels, offset);
				if (alt_panel) {
					input_panel = alt_panel;
				}
			} else {
				Panel *alt_mask = findPanel(&input_layer.alternate_selection_panels, offset);
				if (alt_mask) {
					Panel *alt_panel = findPanel(&input_layer.alternate_panels, offset);
					if (alt_panel) {
						composed_panel = input_panel->copy();
						composePanel(&composed_panel, alt_panel, alt_mask);
						input_panel = &composed_panel;
					}
				}
			}
		}

		renderToSinglePanel(target_panel, target_offset, input_panel, input_layer.offset(), mask_layer, opt, brush_color, opacity);
	}
}

void Canvas::renderToEachPanels(Panel *target_panel, QPoint const &target_offset, std::vector<Layer *> const &input_layers, Layer *mask_layer, QColor const &brush_color, int opacity, RenderOption const &opt, bool *abort)
{
	for (Layer *layer : input_layers) {
		renderToEachPanels_internal_(target_panel, target_offset, *layer, mask_layer, brush_color, opacity, opt, abort);
	}
}

void Canvas::renderToLayer(Layer *target_layer, Layer::ActivePanel activepanel, Layer const &input_layer, Layer *mask_layer, RenderOption const &opt, bool *abort)
{
	Q_ASSERT(input_layer.format_ != euclase::Image::Format_Invalid);
	std::vector<Panel> *targetpanels = target_layer->panels(activepanel);
	if (activepanel != Layer::AlternateSelection) {
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
					Panel *p = findPanel(targetpanels, QPoint(x, y));
					if (!p) {
						p = target_layer->addImagePanel(targetpanels, x, y, PANEL_SIZE, PANEL_SIZE, target_layer->format_, target_layer->memtype_);
						p->imagep()->fill(euclase::k::transparent);
					}
					renderToSinglePanel(p, target_layer->offset(), &input_panel, input_layer.offset(), mask_layer, opt, opt.brush_color, 255, abort);
				}
			}
		}
	}
}

void Canvas::clearSelection()
{
	selection_layer()->clear();
	selection_layer()->memtype_ = global->cuda ? euclase::Image::CUDA : euclase::Image::Host;
}

void Canvas::clear(QMutex *sync)
{
	m->size = QSize();
	clearSelection();
	m->layers.clear();
	m->layers.emplace_back(newLayer());
}

void Canvas::paintToCurrentLayer(Layer const &source, RenderOption const &opt, bool *abort)
{
	renderToLayer(current_layer(), Layer::Primary, source, selection_layer(), opt, abort);
}

void Canvas::addSelection(Layer const &source, RenderOption const &opt, bool *abort)
{
	RenderOption o = opt;
	o.brush_color = Qt::white;
	renderToLayer(selection_layer(), Layer::Primary, source, nullptr, o, abort);
}

void Canvas::subSelection(Layer const &source, RenderOption const &opt, bool *abort)
{
	RenderOption o = opt;
	o.brush_color = Qt::black;
	renderToLayer(selection_layer(), Layer::Primary, source, nullptr, o, abort);
}

Canvas::Panel Canvas::renderSelection(const QRect &r, bool *abort) const
{
	Panel panel;
	panel.imagep()->make(r.width(), r.height(), euclase::Image::Format_8_Grayscale, selection_layer()->memtype_);
	panel.imagep()->fill(euclase::k::black);
	panel.setOffset(r.topLeft());
	std::vector<Layer *> layers;
	layers.push_back(selection_layer());
	renderToEachPanels(&panel, QPoint(), layers, nullptr, QColor(), 255, {}, abort);
	return panel;
}

Canvas::Panel Canvas::renderToPanel(InputLayer inputlayer, euclase::Image::Format format, const QRect &r, QRect const &maskrect, Layer::ActivePanel activepanel, bool *abort) const
{
	RenderOption opt;
	opt.mask_rect = maskrect;
	opt.active_panel = activepanel;

	Panel panel;
	panel.imagep()->make(r.width(), r.height(), format, current_layer()->memtype_);
	panel.imagep()->fill(euclase::k::transparent);
	panel.setOffset(r.topLeft());
	std::vector<Layer *> layers;
	switch (inputlayer) {
	case Canvas::AllLayers:
		for (LayerPtr layer : m->layers) {
			layers.push_back(layer.get());
		}
		break;
	case Canvas::CurrentLayerOnly:
		layers.push_back(current_layer());
		break;
	}
	renderToEachPanels(&panel, QPoint(), layers, nullptr, QColor(), 255, opt, abort);
	return panel;
}

Canvas::Panel Canvas::crop(const QRect &r, bool *abort) const
{
	Panel panel;
	panel.imagep()->make(r.width(), r.height(), euclase::Image::Format_8_RGBA);
	panel.imagep()->fill(euclase::k::transparent);
	panel.setOffset(r.topLeft());
	std::vector<Layer *> layers;
	layers.push_back(current_layer());
	renderToEachPanels(&panel, QPoint(), layers, selection_layer(), QColor(), 255, {}, abort);
	return panel;
}

void Canvas::trim(const QRect &r)
{
	current_layer()->setOffset(current_layer()->offset() - r.topLeft());
	selection_layer()->setOffset(selection_layer()->offset() - r.topLeft());
	setSize(r.size());
}

Canvas::Panel *Canvas::Layer::addImagePanel(std::vector<Panel> *panels, int x, int y, int w, int h, euclase::Image::Format format, euclase::Image::MemoryType memtype)
{
	auto NewPanel = [&](){
		Panel panel;
		if (w > 0 && h > 0) {
			panel->make(w, h, format_, memtype_);
			panel->fill(euclase::k::transparent);
		}
		panel.setOffset(x, y);
		return panel;
	};

	size_t lo = 0;
	size_t hi = panels->size();
	if (hi > 0) {
		hi--;

		QPoint pt(x, y);

		// 挿入先を二分検索
		while (lo < hi) {
			size_t m = (lo + hi) / 2;
			auto c = COMP(panels->at(m).offset(), pt);
			if (c > 0) {
				hi = m;
			} else if (c < 0) {
				lo = m + 1;
			} else {
				return &panels->at(m); // 既にある
			}
		}
		if (COMP(panels->at(lo).offset(), pt) < 0) { // 検索結果の次の位置に挿入する場合
			lo++;
		}
	}

	auto it = panels->insert(panels->begin() + lo, NewPanel());
	return &*it;
}

void Canvas::Layer::finishAlternatePanels(bool apply, QMutex *sync)
{
	if (sync) sync->lock();

	if (apply) {
		auto const *sel = alternate_selection_panels.empty() ? nullptr : &alternate_selection_panels;
		for (Panel &panel : primary_panels) {
			composePanels(&panel, &alternate_panels, sel);
		}
	}

	active_panel_ = Primary;
	alternate_panels.clear();
	alternate_selection_panels.clear();

	if (sync) sync->unlock();
}

QRect Canvas::Layer::rect() const
{
	QRect rect;
	const_cast<Layer *>(this)->eachPanel([&](Panel *p){
		if (p->imagep()->format() == euclase::Image::Format_8_Grayscale) {
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
		} else if (p->imagep()->format() == euclase::Image::Format_8_RGBA) {
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

void Canvas::changeSelection(SelectionOperation op, const QRect &rect)
{
	auto format = euclase::Image::Format_8_Grayscale;
	Canvas::Layer layer;
	layer.format_ = format;
	layer.memtype_ = m->selection_layer.memtype_;
	Panel *panel = layer.addImagePanel(&layer.primary_panels, rect.x(), rect.y(), rect.width(), rect.height(), layer.format_, layer.memtype_);
	panel->imagep()->fill(euclase::k::white);

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
