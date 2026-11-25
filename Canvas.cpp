#include "AlphaBlend.h"
#include "ApplicationGlobal.h"
#include "Canvas.h"
#include <QPainter>
#include <array>
#include <limits>
#include <new>

static int COMP(QPoint const &a, QPoint const &b)
{
	if (a.y() < b.y()) return -1;
	if (a.y() > b.y()) return 1;
	if (a.x() < b.x()) return -1;
	if (a.x() > b.x()) return 1;
	return 0;
}

namespace {

struct PanelRect {
	int64_t left;
	int64_t top;
	int64_t right;
	int64_t bottom;

	bool empty() const
	{
		return left >= right || top >= bottom;
	}

	PanelRect intersected(const PanelRect &other) const
	{
		return {std::max(left, other.left), std::max(top, other.top),
				std::min(right, other.right), std::min(bottom, other.bottom)};
	}

	QRect toQRect() const
	{
		constexpr auto min = std::numeric_limits<int>::min();
		constexpr auto max = std::numeric_limits<int>::max();
		if (empty() || left < min || top < min || right > max || bottom > max
				|| right - left > max || bottom - top > max) {
			return {};
		}
		return QRect(int(left), int(top), int(right - left), int(bottom - top));
	}
};

PanelRect panelRect(const Canvas::Panel &panel, const QPoint &layer_offset = {})
{
	const int64_t x = int64_t(layer_offset.x()) + panel.offset().x();
	const int64_t y = int64_t(layer_offset.y()) + panel.offset().y();
	return {x, y, x + panel.width(), y + panel.height()};
}

PanelRect panelRect(const QRect &r)
{
	return {r.left(), r.top(), int64_t(r.right()) + 1, int64_t(r.bottom()) + 1};
}

bool pointFits(int64_t x, int64_t y)
{
	constexpr auto min = std::numeric_limits<int>::min();
	constexpr auto max = std::numeric_limits<int>::max();
	return x >= min && x <= max && y >= min && y <= max;
}

template <typename Panels>
auto panelPosition(Panels &panels, const QPoint &offset)
{
	return std::lower_bound(panels.begin(), panels.end(), offset, [](const Canvas::Panel &panel, const QPoint &point){
		return COMP(panel.offset(), point) < 0;
	});
}

const Canvas::Panel *findPanel(const std::vector<Canvas::Panel> &panels, const QPoint &offset)
{
	auto it = panelPosition(panels, offset);
	return it != panels.end() && it->offset() == offset ? &*it : nullptr;
}

bool commitPanels(std::vector<Canvas::Panel> &panels, std::vector<Canvas::Panel> &pending)
{
	size_t new_count = std::count_if(pending.begin(), pending.end(), [&](const Canvas::Panel &panel){
		return !findPanel(panels, panel.offset());
	});
	if (new_count > panels.max_size() - panels.size()) return false;
	panels.reserve(panels.size() + new_count);
	for (auto &panel : pending) {
		auto it = panelPosition(panels, panel.offset());
		if (it != panels.end() && it->offset() == panel.offset()) {
			*it = std::move(panel);
		} else {
			panels.insert(it, std::move(panel));
		}
	}
	return true;
}

} // namespace

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
	return index >= 0 && static_cast<size_t>(index) < m->layers.size() ? m->layers[index].get() : nullptr;
}

Canvas::Layer const *Canvas::layer(int index) const
{
	return index >= 0 && static_cast<size_t>(index) < m->layers.size() ? m->layers[index].get() : nullptr;
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

const Canvas::Layer *Canvas::selection_layer() const
{
	return &m->selection_layer;
}

bool Canvas::renderToSinglePanel(Panel *target_panel, QPoint const &target_offset, Panel const *input_panel, QPoint const &input_offset, Layer const *mask_layer, RenderOption const &opt, QColor const &brush_color, int opacity, bool *abort) try
{
	if (!target_panel || !*target_panel || !input_panel || !*input_panel || (abort && *abort)) return false;

	const auto dst = panelRect(*target_panel, target_offset);
	const auto src = panelRect(*input_panel, input_offset);
	auto area = dst.intersected(src);
	if (opt.mask_rect.isValid()) {
		area = area.intersected(panelRect(opt.mask_rect));
	}
	if (area.empty()) return true;

	const auto srcfmt = input_panel->format();
	const auto dstfmt = target_panel->format();
#ifdef USE_CUDA
	const auto src_memory = input_panel->image().memtype();
	const auto dst_memory = target_panel->image().memtype();
	const bool cuda_rgba = (srcfmt == euclase::Format_F16_RGBA || srcfmt == euclase::Format_F32_RGBA)
			&& (dstfmt == euclase::Format_F16_RGBA || dstfmt == euclase::Format_F32_RGBA);
	const bool cuda_gray = srcfmt == euclase::Format_U8_Grayscale && dstfmt == euclase::Format_U8_Grayscale
			&& src_memory == euclase::Image::CUDA;
	if ((src_memory == euclase::Image::CUDA || dst_memory == euclase::Image::CUDA)
			&& !(dst_memory == euclase::Image::CUDA && (cuda_rgba || cuda_gray))) {
		const Panel host_source(input_panel->image().toHost(), input_panel->offset());
		Panel host_target(target_panel->image().toHost(), target_panel->offset());
		if (!renderToSinglePanel(&host_target, target_offset, &host_source, input_offset, mask_layer, opt, brush_color, opacity, abort)) {
			return false;
		}
		host_target->memconvert(dst_memory);
		if (!host_target) return false;
		*target_panel = std::move(host_target);
		return true;
	}
#endif

	const euclase::Image *input_image = input_panel->imagep();
	const int dx = int(area.left - dst.left);
	const int dy = int(area.top - dst.top);
	const int sx = int(area.left - src.left);
	const int sy = int(area.top - src.top);
	const int w = int(area.right - area.left);
	const int h = int(area.bottom - area.top);
	opacity = std::clamp(opacity, -255, 255);

	Panel maskpanel;
	const euclase::Image *maskimg = nullptr;
	static const auto opaque_tile_row = [] {
		std::array<uint8_t, PANEL_SIZE> row;
		row.fill(255);
		return row;
	}();
	const uint8_t *tmpmask = opaque_tile_row.data();
	std::vector<uint8_t> opaque_row;
	if (opt.use_mask && mask_layer && mask_layer->panelCount() != 0) {
		const QRect mask_area = area.toQRect();
		if (!mask_area.isValid() || !maskpanel->make(w, h, euclase::Format_U8_Grayscale)) return false;
		maskpanel.setOffset(mask_area.topLeft());
		if (!renderToEachPanel(&maskpanel, *mask_layer, nullptr, {}, abort)) return false;
		maskimg = maskpanel.imagep();
	} else if (w > PANEL_SIZE) {
		opaque_row.resize(w, 255);
		tmpmask = opaque_row.data();
	}
	auto grayByte = [](float value) {
		if (std::isnan(value)) return uint8_t(0);
		return uint8_t(std::floor(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f));
	};

	if (srcfmt == euclase::ImageFormat::Format_U8_Grayscale) {
		QColor c = brush_color.isValid() ? brush_color : Qt::white;

		uint8_t invert = 0;
		if (opacity < 0) {
			opacity = -opacity;
			invert = 255;
		}

#ifdef USE_CUDA
		if (dst_memory == euclase::Image::CUDA) {
			constexpr int max_launch_extent = std::numeric_limits<int>::max() - 15;
			if (w > max_launch_extent || h > max_launch_extent) return false;
			auto *out = target_panel->imagep();
			global->cuda->blend_uint8_grayscale(w, h, input_image->data(), input_image->width(),
					input_image->height(), sx, sy, maskimg ? maskimg->data() : nullptr,
					maskimg ? maskimg->width() : 0, maskimg ? maskimg->height() : 0,
					out->data(), out->width(), out->height(), dx, dy);
			return true;
		}
#endif

		if (dstfmt == euclase::ImageFormat::Format_U8_RGBA) {
			euclase::UInt8RGBA color(c.red(), c.green(), c.blue());
			for (int i = 0; i < h; i++) {
				using Pixel = euclase::UInt8RGBA;
				uint8_t const *msk = !maskimg ? tmpmask : maskimg->scanLine(i);
				uint8_t const *src = input_image->scanLine(sy + i);
				Pixel *dst = reinterpret_cast<Pixel *>(target_panel->imagep()->scanLine(dy + i));
				for (int j = 0; j < w; j++) {
					color.a = opacity * (src[sx + j] ^ invert) * msk[j] / (255 * 255);
					dst[dx + j] = AlphaBlend::blend(dst[dx + j], color);
				}
			}
		} else if (dstfmt == euclase::ImageFormat::Format_F32_RGBA) {
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
		} else if (dstfmt == euclase::ImageFormat::Format_U8_Grayscale) {
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
		return dstfmt == euclase::Format_U8_RGBA || dstfmt == euclase::Format_F32_RGBA
				|| dstfmt == euclase::Format_U8_Grayscale;
	}

	if (srcfmt == euclase::ImageFormat::Format_F32_Grayscale) {
		QColor c = brush_color.isValid() ? brush_color : Qt::white;

		uint8_t invert = 0;
		if (opacity < 0) {
			opacity = -opacity;
			invert = 255;
		}

		if (dstfmt == euclase::ImageFormat::Format_U8_RGBA) {
			euclase::UInt8RGBA color(c.red(), c.green(), c.blue());
			for (int i = 0; i < h; i++) {
				using Pixel = euclase::UInt8RGBA;
				uint8_t const *msk = !maskimg ? tmpmask : maskimg->scanLine(i);
				float const *src = (float const *)input_image->scanLine(sy + i);
				Pixel *dst = reinterpret_cast<Pixel *>(target_panel->imagep()->scanLine(dy + i));
				for (int j = 0; j < w; j++) {
					color.a = opacity * (grayByte(src[sx + j]) ^ invert) * msk[j] / (255 * 255);
					dst[dx + j] = AlphaBlend::blend(dst[dx + j], color);
				}
			}
		} else if (dstfmt == euclase::ImageFormat::Format_F32_RGBA) {
			euclase::Float32RGBA color((uint8_t)c.red(), (uint8_t)c.green(), (uint8_t)c.blue());
			for (int i = 0; i < h; i++) {
				using Pixel = euclase::Float32RGBA;
				uint8_t const *msk = !maskimg ? tmpmask : maskimg->scanLine(i);
				float const *src = (float const *)input_image->scanLine(sy + i);
				Pixel *dst = reinterpret_cast<Pixel *>(target_panel->imagep()->scanLine(dy + i));
				for (int j = 0; j < w; j++) {
					color.a = opacity * (grayByte(src[sx + j]) ^ invert) * msk[j] / (255 * 255) / 255.0f;
					dst[dx + j] = AlphaBlend::blend(dst[dx + j], color);
				}
			}
		} else if (dstfmt == euclase::ImageFormat::Format_U8_Grayscale) {
			for (int y = 0; y < h; y++) {
				uint8_t const *msk = !maskimg ? tmpmask : maskimg->scanLine(y);
				float const *src = (float const *)input_image->scanLine(sy + y);
				uint8_t *dst = target_panel->imagep()->scanLine(dy + y);
				for (int x = 0; x < w; x++) {
					float const *s = src + sx + x;
					uint8_t *d = dst + dx + x;
					uint8_t const *m = msk + x;
					const float gray = std::isnan(*s) ? 0.0f : std::clamp(*s, 0.0f, 1.0f);
					*d = (*d * (255 - *m) + gray * 255.0f * *m) / 255;
				}
			}
		}

		return dstfmt == euclase::Format_U8_RGBA || dstfmt == euclase::Format_F32_RGBA
				|| dstfmt == euclase::Format_U8_Grayscale;
	}

	if (dstfmt == euclase::ImageFormat::Format_U8_RGBA) {
		if (srcfmt != euclase::Format_U8_RGBA && srcfmt != euclase::Format_F32_RGBA) {
			return srcfmt == euclase::Format_F16_RGBA;
		}
		const auto in = input_image->convertToFormat(euclase::ImageFormat::Format_U8_RGBA);
		if (!in) return false;
		const size_t pixel_size = in.bytesPerPixel();
		for (int y = 0; y < h; y++) {
			const uint8_t *mask = maskimg ? maskimg->scanLine(y) : tmpmask;
			const auto *row = in.scanLine(sy + y) + pixel_size * sx;
			const auto *src = reinterpret_cast<const euclase::UInt8RGBA *>(row);
			auto *dst = reinterpret_cast<euclase::UInt8RGBA *>(target_panel->scanLine(dy + y)) + dx;
			for (int x = 0; x < w; x++) {
				auto color = src[x];
				color.a = color.a * mask[x] / 255;
				dst[x] = AlphaBlend::blend(dst[x], color);
			}
		}
		return true;
	}

	if (dstfmt == euclase::ImageFormat::Format_F32_RGBA) {
		if (srcfmt == euclase::ImageFormat::Format_F32_RGBA || srcfmt == euclase::ImageFormat::Format_F16_RGBA) {
			if (target_panel->image().memtype() == euclase::Image::CUDA) {
#ifdef USE_CUDA
				euclase::Image in = *input_image;
				euclase::Image *out = target_panel->imagep();
				if (srcfmt != euclase::ImageFormat::Format_F32_RGBA) {
					in = in.convertToFormat(euclase::ImageFormat::Format_F32_RGBA);
				}
				in = in.toCUDA();
				if (!in) return false;
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
#endif
				return true;
			}
		}

		const size_t dstep = euclase::bytesPerPixel(dstfmt);
		const size_t sstep = euclase::bytesPerPixel(srcfmt);
		if (srcfmt == euclase::ImageFormat::Format_U8_RGBA) {
			auto render = [](uint8_t *dst, uint8_t const *src, uint8_t const *msk, int w){
				for (int j = 0; j < w; j++) {
					euclase::Float32RGBA color = euclase::Float32RGBA::convert(((euclase::UInt8RGBA const *)src)[j]);
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
			return true;
		}
		if (srcfmt == euclase::ImageFormat::Format_F32_RGBA) {
			const auto &in = *input_image;
			auto &out = *target_panel->imagep();
			float const *src = (float const *)in.data();
			float *dst = (float *)out.data();
			uint8_t const *mask = maskimg ? (uint8_t const *)maskimg->data() : nullptr;
			int mask_stride = maskimg ? maskimg->width() : 0;
			for (int y = 0; y < h; y++) {
				float const *s = src + 4 * (static_cast<size_t>(in.width()) * (sy + y) + sx);
				float *d = dst + 4 * (static_cast<size_t>(out.width()) * (dy + y) + dx);
				switch (opt.blend_mode) {
				case BlendMode::Normal:
					for (int x = 0; x < w; x++) {
						uint8_t m = mask ? *(mask + static_cast<size_t>(mask_stride) * y + x) : 255;
						euclase::Float32RGBA t = *(euclase::Float32RGBA *)d;
						euclase::Float32RGBA u = *(euclase::Float32RGBA *)s;
						u.a = u.a * m / 255;
						t = AlphaBlend::blend(t, u);
						d[0] = std::min(std::max(t.r, 0.0f), 1.0f);
						d[1] = std::min(std::max(t.g, 0.0f), 1.0f);
						d[2] = std::min(std::max(t.b, 0.0f), 1.0f);
						d[3] = std::min(std::max(t.a, 0.0f), 1.0f);
						s += 4;
						d += 4;
					}
					break;
				case BlendMode::Eraser:
					for (int x = 0; x < w; x++) {
						uint8_t m = mask ? *(mask + static_cast<size_t>(mask_stride) * y + x) : 255;
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
			return true;
		} else if (srcfmt == euclase::ImageFormat::Format_F16_RGBA) {
			const auto &in = *input_image;
			auto &out = *target_panel->imagep();
			euclase::_float16_t const *src = (euclase::_float16_t const *)in.data();
			float *dst = (float *)out.data();
			uint8_t const *mask = maskimg ? (uint8_t const *)maskimg->data() : nullptr;
			int mask_stride = maskimg ? maskimg->width() : 0;
			for (int y = 0; y < h; y++) {
				euclase::_float16_t const *s = src + 4 * (static_cast<size_t>(in.width()) * (sy + y) + sx);
				float *d = dst + 4 * (static_cast<size_t>(out.width()) * (dy + y) + dx);
				switch (opt.blend_mode) {
				case BlendMode::Normal:
					for (int x = 0; x < w; x++) {
						uint8_t m = mask ? *(mask + static_cast<size_t>(mask_stride) * y + x) : 255;
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
						uint8_t m = mask ? *(mask + static_cast<size_t>(mask_stride) * y + x) : 255;
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
			return true;
		}
	}

	if (dstfmt == euclase::ImageFormat::Format_F16_RGBA) {
		if (srcfmt == euclase::ImageFormat::Format_F32_RGBA || srcfmt == euclase::ImageFormat::Format_F16_RGBA) {
			if (target_panel->image().memtype() == euclase::Image::CUDA) {
#ifdef USE_CUDA
				euclase::Image in = *input_image;
				euclase::Image *out = target_panel->imagep();
				if (srcfmt != euclase::ImageFormat::Format_F16_RGBA) {
					in = in.convertToFormat(euclase::ImageFormat::Format_F16_RGBA);
				}
				in = in.toCUDA();
				if (!in) return false;
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
#endif
				return true;
			}
		}

		const size_t dstep = euclase::bytesPerPixel(dstfmt);
		const size_t sstep = euclase::bytesPerPixel(srcfmt);
		if (srcfmt == euclase::ImageFormat::Format_U8_RGBA) {
			auto render = [](uint8_t *dst, uint8_t const *src, uint8_t const *msk, int w){
				for (int j = 0; j < w; j++) {
					euclase::Float16RGBA color = euclase::Float16RGBA::convert(((euclase::UInt8RGBA const *)src)[j]);
					color.a = color.a * (float)msk[j] / 255.0f;
					((euclase::Float16RGBA *)dst)[j] = AlphaBlend::blend(((euclase::Float16RGBA *)dst)[j], color);
				}
			};
			for (int i = 0; i < h; i++) {
				uint8_t const *msk = !maskimg ? tmpmask : maskimg->scanLine(i);
				uint8_t const *src = input_image->scanLine(sy + i);
				uint8_t *dst = target_panel->imagep()->scanLine(dy + i);
				render(dst + dstep * dx, src + sstep * sx, msk, w);
			}
			return true;
		}
		if (srcfmt == euclase::ImageFormat::Format_F32_RGBA) {
			const auto &in = *input_image;
			auto &out = *target_panel->imagep();
			float const *src = (float const *)in.data();
			euclase::_float16_t *dst = (euclase::_float16_t *)out.data();
			uint8_t const *mask = maskimg ? (uint8_t const *)maskimg->data() : nullptr;
			int mask_stride = maskimg ? maskimg->width() : 0;
			for (int y = 0; y < h; y++) {
				float const *s = src + 4 * (static_cast<size_t>(in.width()) * (sy + y) + sx);
				euclase::_float16_t *d = dst + 4 * (static_cast<size_t>(out.width()) * (dy + y) + dx);
				switch (opt.blend_mode) {
				case BlendMode::Normal:
					for (int x = 0; x < w; x++) {
						uint8_t m = mask ? *(mask + static_cast<size_t>(mask_stride) * y + x) : 255;
						euclase::Float16RGBA t = *(euclase::Float16RGBA *)d;
						euclase::Float16RGBA u(*(euclase::Float32RGBA const *)s);
						u.a = u.a * (float)m / 255.0f;
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
						uint8_t m = mask ? *(mask + static_cast<size_t>(mask_stride) * y + x) : 255;
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
			return true;
		} else if (srcfmt == euclase::ImageFormat::Format_F16_RGBA) {
			const auto &in = *input_image;
			auto &out = *target_panel->imagep();
			euclase::_float16_t const *src = (euclase::_float16_t const *)in.data();
			euclase::_float16_t *dst = (euclase::_float16_t *)out.data();
			uint8_t const *mask = maskimg ? (uint8_t const *)maskimg->data() : nullptr;
			int mask_stride = maskimg ? maskimg->width() : 0;
			for (int y = 0; y < h; y++) {
				euclase::_float16_t const *s = src + 4 * (static_cast<size_t>(in.width()) * (sy + y) + sx);
				euclase::_float16_t *d = dst + 4 * (static_cast<size_t>(out.width()) * (dy + y) + dx);
				switch (opt.blend_mode) {
				case BlendMode::Normal:
					for (int x = 0; x < w; x++) {
						uint8_t m = mask ? *(mask + static_cast<size_t>(mask_stride) * y + x) : 255;
						euclase::Float16RGBA t = *(euclase::Float16RGBA *)d;
						euclase::Float16RGBA u = *(euclase::Float16RGBA *)s;
						u.a = u.a * (float)m / 255.0f;
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
						uint8_t m = mask ? *(mask + static_cast<size_t>(mask_stride) * y + x) : 255;
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
			return true;
		}
	}

	if (dstfmt == euclase::Format_U8_Grayscale && srcfmt == euclase::Format_U8_RGBA) {
		for (int y = 0; y < h; y++) {
			const auto *mask = maskimg ? maskimg->scanLine(y) : tmpmask;
			const auto *src = reinterpret_cast<const euclase::UInt8RGBA *>(input_image->scanLine(sy + y)) + sx;
			auto *dst = target_panel->scanLine(dy + y) + dx;
			for (int x = 0; x < w; x++) {
				auto color = src[x];
				color.a = color.a * mask[x] / 255;
				euclase::UInt8GrayA gray(dst[x]);
				gray = AlphaBlend::blend(euclase::UInt8RGBA(gray), color);
				dst[x] = gray.gray();
			}
		}
		return true;
	}

	return false;
} catch (const std::bad_alloc &) {
	return false;
}

namespace {

template <typename Pixel>
void composeRows(euclase::Image &dst, const euclase::Image &src, const euclase::Image &mask,
				 int width, int height, Canvas::BlendMode mode)
{
	for (int y = 0; y < height; y++) {
		auto *d = reinterpret_cast<Pixel *>(dst.scanLine(y));
		const auto *s = reinterpret_cast<const Pixel *>(src.scanLine(y));
		const auto *m = mask ? mask.scanLine(y) : nullptr;
		for (int x = 0; x < width; x++) {
			auto over = s[x];
			if (m) over.a = float(over.a) * m[x] / 255.0f;
			if (mode == Canvas::BlendMode::Eraser) {
				d[x].a = float(d[x].a) * (1.0f - euclase::clamp_f32(over.a));
			} else {
				d[x] = AlphaBlend::blend(d[x], over);
			}
		}
	}
}

int64_t tileStart(int64_t coordinate)
{
	const int64_t remainder = coordinate % PANEL_SIZE;
	return coordinate - (remainder < 0 ? remainder + PANEL_SIZE : remainder);
}

} // namespace

bool Canvas::composePanel(Panel *target_panel, const Panel *alt_panel, const Panel *alt_mask, BlendMode mode)
{
	if (!target_panel || !*target_panel || !alt_panel || !*alt_panel) return false;
	if (mode != BlendMode::Normal && mode != BlendMode::Eraser) return true;
	if (target_panel->offset() != alt_panel->offset()) return false;

	const auto target_format = target_panel->format();
	const auto source_format = alt_panel->format();
	if ((target_format != euclase::Format_F16_RGBA && target_format != euclase::Format_F32_RGBA)
			|| (source_format != euclase::Format_F16_RGBA && source_format != euclase::Format_F32_RGBA)) {
		return false;
	}
	const auto format = (target_format == euclase::Format_F32_RGBA || source_format == euclase::Format_F32_RGBA)
			? euclase::Format_F32_RGBA : euclase::Format_F16_RGBA;
	const auto memory = target_panel->image().memtype();
	auto area = panelRect(*target_panel).intersected(panelRect(*alt_panel));
	euclase::Image mask;
	if (alt_mask) {
		if (!*alt_mask || alt_mask->format() != euclase::Format_U8_Grayscale
				|| alt_mask->offset() != target_panel->offset()) {
			return false;
		}
		area = area.intersected(panelRect(*alt_mask));
		if (area.empty()) return true;
		mask = alt_mask->image().toHost();
		if (!mask) return false;
	}
	if (area.empty()) return true;

	auto dst = target_panel->image().convertToFormat(format);
	auto src = alt_panel->image().convertToFormat(format);
	src.memconvert(memory);
	if (!src || !dst) return false;
	const int width = int(area.right - area.left);
	const int height = int(area.bottom - area.top);

#ifdef USE_CUDA
	if (memory == euclase::Image::CUDA) {
		const auto *cuda = global->cuda;
		const bool erase = mode == BlendMode::Eraser;
		auto compose = format == euclase::Format_F32_RGBA
				? (erase ? cuda->erase_fp32_RGBA : cuda->blend_fp32_RGBA)
				: (erase ? cuda->erase_fp16_RGBA : cuda->blend_fp16_RGBA);
		compose(width, height, src.data(), src.width(), src.height(), 0, 0,
				mask.data(), mask.width(), mask.height(), dst.data(), dst.width(), dst.height(), 0, 0);
	} else
#endif
	if (format == euclase::Format_F32_RGBA) {
		composeRows<euclase::Float32RGBA>(dst, src, mask, width, height, mode);
	} else {
		composeRows<euclase::Float16RGBA>(dst, src, mask, width, height, mode);
	}

	if (format != target_format) {
		dst = dst.convertToFormat(target_format);
		if (!dst) return false;
	}
	*target_panel->imagep() = dst;
	return true;
}

bool Canvas::renderToEachPanel(Panel *target_panel, const Layer &input_layer, const Layer *mask_layer, const RenderOption &opt, bool *abort)
{
	auto area = panelRect(*target_panel);
	if (opt.mask_rect.isValid()) {
		area = area.intersected(panelRect(opt.mask_rect));
	}

	RenderOption render_opt = opt;
	render_opt.use_mask = false;
	for (const auto &input : input_layer.primary_panels) {
		if (abort && *abort) return false;
		if (area.intersected(panelRect(input, input_layer.offset())).empty()) continue;

		const Panel *source = &input;
		Panel composed;
		if (opt.active_panel == AlternateLayer) {
			const auto mode = input_layer.alternate_blend_mode;
			const bool replace = mode == BlendMode::Replace;
			const Panel *alternate = findPanel(input_layer.alternate_panels, input.offset());
			if (alternate && mode != BlendMode::Disable) {
				const std::vector<Panel> *masks = nullptr;
				if (replace && !input_layer.alternate_selection_panels.empty()) {
					masks = &input_layer.alternate_selection_panels;
				} else if (!replace && opt.use_mask && mask_layer) {
					masks = &mask_layer->primary_panels;
				}
				const Panel *mask = masks ? findPanel(*masks, input.offset()) : nullptr;
				if (!masks || mask) {
					composed = input.copy();
					if (!composePanel(&composed, alternate, mask, replace ? BlendMode::Normal : mode)) return false;
					source = &composed;
				}
			}
		}
		if (!renderToSinglePanel(target_panel, {}, source, input_layer.offset(), nullptr, render_opt, opt.brush_color, 255, abort)) {
			return false;
		}
	}
	return true;
}

bool Canvas::renderToLayer(Layer *target_layer, ActivePanel activepanel, const Layer &input_layer, const Layer *mask_layer, const RenderOption &opt, bool *abort) try
{
	if (!target_layer || input_layer.format_ == euclase::Format_Invalid) return false;
	auto *target = target_layer->panels(activepanel);
	if (!target) return false;
	auto format = target_layer->format_;
	if (activepanel == AlternateSelection) {
		format = euclase::Format_U8_Grayscale;
	} else if (format == euclase::Format_Invalid) {
		format = input_layer.format_;
	}

	std::vector<Panel> pending;
	std::vector<QRect> changed;
	const QPoint target_offset = target_layer->offset();
	for (const auto &input : input_layer.primary_panels) {
		if (!input) return false;
		const auto area = panelRect(input, input_layer.offset());
		const auto bounds = area.toQRect();
		if (!bounds.isValid()) return false;
		for (int64_t y = tileStart(area.top - target_offset.y()); y < area.bottom - target_offset.y(); y += PANEL_SIZE) {
			for (int64_t x = tileStart(area.left - target_offset.x()); x < area.right - target_offset.x(); x += PANEL_SIZE) {
				if ((abort && *abort) || !pointFits(x, y)) return false;
				const QPoint offset{int(x), int(y)};
				auto it = panelPosition(pending, offset);
				Panel *tile;
				if (it != pending.end() && it->offset() == offset) {
					tile = &*it;
				} else if (const auto *existing = findPanel(*target, offset)) {
					auto copy = existing->copy();
					if (!copy) return false;
					tile = &*pending.insert(it, std::move(copy));
				} else {
					tile = Layer::addImagePanel(&pending, offset.x(), offset.y(), PANEL_SIZE, PANEL_SIZE,
												format, target_layer->memtype_);
				}
				if (!renderToSinglePanel(tile, target_offset, &input, input_layer.offset(), mask_layer, opt, opt.brush_color, 255, abort)) {
					return false;
				}
			}
		}
		if (opt.notify_changed_rect) changed.push_back(bounds);
	}

	if ((abort && *abort) || !commitPanels(*target, pending)) return false;
	if (activepanel != AlternateSelection) {
		target_layer->format_ = format;
		target_layer->active_panel_ = activepanel;
	}
	for (const auto &rect : changed) opt.notify_changed_rect(rect);
	return true;
} catch (const std::bad_alloc &) {
	return false;
}

void Canvas::clearSelection()
{
	*selection_layer() = Layer();
}

void Canvas::clear()
{
	auto first_layer = newLayer();
	m->size = QSize(0, 0);
	clearSelection();
	m->layers.clear();
	m->layers.emplace_back(std::move(first_layer));
	m->current_layer_index = 0;
}

bool Canvas::paintToCurrentLayer(Layer const &source, RenderOption const &opt, bool *abort)
{
	return renderToLayer(current_layer(), PrimaryLayer, source, selection_layer(), opt, abort);
}

bool Canvas::paintToCurrentAlternate(Layer const &source, RenderOption const &opt, bool *abort)
{
	return renderToLayer(current_layer(), AlternateLayer, source, opt.use_mask ? selection_layer() : nullptr, opt, abort);
}

bool Canvas::addSelection(Layer const &source, RenderOption const &opt, bool *abort)
{
	RenderOption o = opt;
	o.brush_color = Qt::white;
	return renderToLayer(selection_layer(), PrimaryLayer, source, nullptr, o, abort);
}

bool Canvas::subSelection(Layer const &source, RenderOption const &opt, bool *abort)
{
	RenderOption o = opt;
	o.brush_color = Qt::black;
	return renderToLayer(selection_layer(), PrimaryLayer, source, nullptr, o, abort);
}

Canvas::Panel Canvas::renderSelection(const QRect &r, bool *abort) const
{
	const QRect area = panelRect(r).toQRect();
	Panel panel;
	if (!panel->make(area.width(), area.height(), euclase::Format_U8_Grayscale)) return {};
	panel.setOffset(area.topLeft());
	if (!renderToEachPanel(&panel, *selection_layer(), nullptr, {}, abort)) return {};
	return panel;
}

Canvas::Panel Canvas::renderToPanel(InputLayerMode input_layer_mode, euclase::ImageFormat format, const QRect &r, QRect const &maskrect, ActivePanel activepanel, RenderOption const &opt, bool *abort) const
{
	const auto *current = current_layer();
	const QRect area = panelRect(r).toQRect();
	Panel panel;
	if (!current || !panel->make(area.width(), area.height(), format, current->memtype_)) return {};
	panel.setOffset(area.topLeft());

	RenderOption render_opt = opt;
	render_opt.mask_rect = maskrect;
	render_opt.active_panel = activepanel;
	const auto *mask_layer = opt.use_mask && !selection_layer()->primary_panels.empty() ? selection_layer() : nullptr;
	if (input_layer_mode == CurrentLayerOnly) {
		if (!renderToEachPanel(&panel, *current, mask_layer, render_opt, abort)) return {};
	} else {
		for (const auto &layer : m->layers) {
			if (!renderToEachPanel(&panel, *layer, mask_layer, render_opt, abort)) return {};
		}
	}
	return panel;
}

Canvas::Panel Canvas::crop(const QRect &r, bool *abort) const
{
	Panel panel;
	const auto *current = current_layer();
	const QRect area = panelRect(r).toQRect();
	if (!current || !panel->make(area.width(), area.height(), euclase::Format_U8_RGBA)) return {};
	panel.setOffset(area.topLeft());
	if (!renderToEachPanel(&panel, *current, selection_layer(), {}, abort)) return {};
	return panel;
}

bool Canvas::trim(const QRect &r)
{
	const QRect area = panelRect(r).toQRect();
	if (!area.isValid()) return false;
	auto *layer = current_layer();
	if (!layer) return false;
	auto *selection = selection_layer();
	const auto layer_x = int64_t(layer->offset().x()) - r.x();
	const auto layer_y = int64_t(layer->offset().y()) - r.y();
	const auto selection_x = int64_t(selection->offset().x()) - r.x();
	const auto selection_y = int64_t(selection->offset().y()) - r.y();
	if (!pointFits(layer_x, layer_y) || !pointFits(selection_x, selection_y)) return false;
	layer->setOffset({int(layer_x), int(layer_y)});
	selection->setOffset({int(selection_x), int(selection_y)});
	setSize(area.size());
	return true;
}

Canvas::Panel *Canvas::Layer::addPanel(std::vector<Panel> *panels, Panel &&panel) try
{
	if (!panels || !panel) return nullptr;
	auto it = panelPosition(*panels, panel.offset());
	if (it != panels->end() && it->offset() == panel.offset()) return nullptr;
	return &*panels->insert(it, std::move(panel));
} catch (const std::bad_alloc &) {
	return nullptr;
}

Canvas::Panel *Canvas::Layer::addImagePanel(std::vector<Panel> *panels, int x, int y, int w, int h, euclase::ImageFormat format, euclase::Image::MemoryType memtype)
{
	Panel panel;
	if (!panel->make(w, h, format, memtype)) return nullptr;
	panel.setOffset(x, y);
	return addPanel(panels, std::move(panel));
}

void Canvas::Layer::setAlternateOption(BlendMode blendmode)
{
	alternate_blend_mode = blendmode;
}

bool Canvas::Layer::finishAlternatePanels(bool apply, const Layer *mask_layer, RenderOption const &opt) try
{
	if (apply && !alternate_panels.empty()) {
		std::vector<Panel> pending;
		pending.reserve(alternate_panels.size());
		for (const auto &alternate : alternate_panels) {
			const Panel *mask = nullptr;
			if (opt.use_mask && mask_layer) {
				mask = findPanel(mask_layer->primary_panels, alternate.offset());
				if (!mask) continue;
			}

			const auto *existing = findPanel(primary_panels, alternate.offset());
			Panel tile = existing ? existing->copy() : Panel();
			if (!existing) {
				const auto format = format_ == euclase::Format_Invalid ? alternate.format() : format_;
				if (!tile->make(PANEL_SIZE, PANEL_SIZE, format, memtype_)) return false;
				tile.setOffset(alternate.offset());
			}
			if (!composePanel(&tile, &alternate, mask, opt.blend_mode)) return false;
			pending.push_back(std::move(tile));
		}
		if (!commitPanels(primary_panels, pending)) return false;
	}

	active_panel_ = PrimaryLayer;
	alternate_panels.clear();
	alternate_selection_panels.clear();
	return true;
} catch (const std::bad_alloc &) {
	return false;
}

QRect Canvas::Layer::rect() const
{
	PanelRect bounds{};
	for (const auto &panel : primary_panels) {
		const auto format = panel.format();
		if (format != euclase::Format_U8_Grayscale && format != euclase::Format_U8_RGBA) continue;
		const auto image = panel.image().toHost();
		if (!image) continue;

		int x0 = image.width();
		int y0 = image.height();
		int x1 = 0;
		int y1 = 0;
		for (int y = 0; y < image.height(); y++) {
			const auto *row = image.scanLine(y);
			for (int x = 0; x < image.width(); x++) {
				if (format == euclase::Format_U8_Grayscale ? row[x] != 0 : row[size_t(x) * 4 + 3] != 0) {
					x0 = std::min(x0, x);
					y0 = std::min(y0, y);
					x1 = std::max(x1, x);
					y1 = std::max(y1, y);
				}
			}
		}
		if (x0 < x1 && y0 < y1) {
			const auto area = panelRect(panel, offset());
			const PanelRect selected{area.left + x0, area.top + y0, area.left + x1 + 1, area.top + y1 + 1};
			if (bounds.empty()) {
				bounds = selected;
			} else {
				bounds = {std::min(bounds.left, selected.left), std::min(bounds.top, selected.top),
						  std::max(bounds.right, selected.right), std::max(bounds.bottom, selected.bottom)};
			}
		}
	}
	return bounds.toQRect();
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

bool Canvas::changeSelection(SelectionOperation op, const QRect &rect, Bounds::Type bounds_type) try
{
	const QRect area = panelRect(rect).toQRect();
	if (!area.isValid()) return false;
	const euclase::Image image(std::visit([&](const auto &shape){ return makeBoundsImage(shape, area); }, bounds_type));
	if (!image) return false;
	Layer source;
	source.setImage(area.topLeft(), image);

	switch (op) {
	case SelectionOperation::SetSelection: {
		Layer replacement;
		if (!renderToLayer(&replacement, PrimaryLayer, source, nullptr, {}, nullptr)) return false;
		*selection_layer() = std::move(replacement);
		return true;
	}
	case SelectionOperation::AddSelection:
		return addSelection(source, {}, nullptr);
	case SelectionOperation::SubSelection:
		return subSelection(source, {}, nullptr);
	}
	return false;
} catch (const std::bad_alloc &) {
	return false;
}

int Canvas::addNewLayer()
{
	int index = m->current_layer_index + 1;
	m->layers.insert(m->layers.begin() + index, newLayer());
	return index;
}

void Canvas::setCurrentLayer(int index)
{
	if (layer(index)) m->current_layer_index = index;
}

//
euclase::Image cropImage(const euclase::Image &srcimg, int sx, int sy, int sw, int sh)
{
	Canvas::Panel source(srcimg);
	Canvas::Panel target(euclase::Image(sw, sh, srcimg.format(), srcimg.memtype()), QPoint(sx, sy));
	if (!Canvas::renderToSinglePanel(&target, {}, &source, {}, nullptr, {}, {})) return {};
	return target.image();
}
