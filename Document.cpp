#include "AlphaBlend.h"
#include "Document.h"
#include <QDebug>
#include <QElapsedTimer>
#include <QPainter>
#include <functional>

struct Document::Private {
	QSize size;
	Document::Layer current_layer;
	Document::Layer filtering_layer;
	Document::Layer selection_layer;
};

Document::Document()
	: m(new Private)
{
}

Document::~Document()
{
	delete m;
}

int Document::width() const
{
	return m->size.width();
}

int Document::height() const
{
	return m->size.height();
}

QSize Document::size() const
{
	return m->size;
}

void Document::setSize(const QSize &s)
{
	m->size = s;
}

Document::Layer *Document::current_layer()
{
	return &m->current_layer;
}

Document::Layer *Document::selection_layer()
{
	return &m->selection_layer;
}

Document::Layer *Document::current_layer() const
{
	return &m->current_layer;
}

Document::Layer *Document::selection_layer() const
{
	return &m->selection_layer;
}

void Document::renderToSinglePanel(Image *target_panel, QPoint const &target_offset, Image const *input_panel, QPoint const &input_offset, Layer const *mask_layer, RenderOption const &opt, QColor const &brush_color, int opacity, bool *abort)
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

	Image const *input_image = input_panel;

	const int dx = x0 - dst_org.x();
	const int dy = y0 - dst_org.y();
	const int sx = x0 - src_org.x();
	const int sy = y0 - src_org.y();
	uint8_t *tmpmask = nullptr;
	Image *maskimg = nullptr;
	Image maskpanel;
	if (mask_layer && !mask_layer->panels_.empty()) {
		maskpanel.setOffset(x0, y0);
		maskpanel.make(w, h, QImage::Format_Grayscale8);
		maskpanel.fill(Qt::black);
		renderToEachPanels_(&maskpanel, target_offset, *mask_layer, nullptr, Qt::white, 255, abort);
		maskimg = &maskpanel;
	} else {
		tmpmask = (uint8_t *)alloca(w);
		memset(tmpmask, 255, w);
	}

	if (input_image->format() == QImage::Format_Grayscale8) {
		Image const *selection = input_image;

		QColor c = brush_color.isValid() ? brush_color : Qt::white;

		uint8_t invert = 0;
		if (opacity < 0) {
			opacity = -opacity;
			invert = 255;
		}

		if (target_panel->isRGBA8888()) {
			euclase::PixelRGBA color(c.red(), c.green(), c.blue());
			for (int i = 0; i < h; i++) {
				using Pixel = euclase::PixelRGBA;
				uint8_t const *msk = !maskimg ? tmpmask : maskimg->scanLine(i);
				uint8_t const *src = selection->scanLine(sy + i);
				Pixel *dst = reinterpret_cast<Pixel *>(target_panel->scanLine(dy + i));
				for (int j = 0; j < w; j++) {
					color.a = opacity * (src[sx + j] ^ invert) * msk[j] / (255 * 255);
					dst[dx + j] = AlphaBlend::blend_with_gamma_collection(dst[dx + j], color);
				}
			}
		} else if (target_panel->isGrayscale8()) {
			euclase::PixelGrayA color(euclase::gray(c.red(), c.green(), c.blue()));
			uint8_t l = color.l;
			for (int i = 0; i < h; i++) {
				using Pixel = euclase::PixelGrayA;
				uint8_t const *msk = !maskimg ? tmpmask : maskimg->scanLine(i);
				uint8_t const *src = reinterpret_cast<uint8_t const *>(selection->scanLine(sy + i));
				uint8_t *dst = reinterpret_cast<uint8_t *>(target_panel->scanLine(dy + i));
				for (int j = 0; j < w; j++) {
					uint8_t a = opacity * (src[sx + j] ^ invert) * msk[j] / (255 * 255);
					dst[dx + j] = AlphaBlend::blend(Pixel(dst[dx + j]), Pixel(l, a)).l;
				}
			}
		}
		return;
	}

	if (target_panel->isRGBA8888()) {

		auto RenderRGBA8888 = [](euclase::PixelRGBA const *src, euclase::PixelRGBA *dst, uint8_t const *msk, int w, RenderOption const &opt){
			if (opt.mode == RenderOption::DirectCopy) {
				memcpy(dst, src, sizeof(euclase::PixelRGBA) * w);
			} else {
				for (int j = 0; j < w; j++) {
					euclase::PixelRGBA color = src[j];
					color.a = color.a * msk[j] / 255;
					dst[j] = AlphaBlend::blend_with_gamma_collection(dst[j], color);
				}
			}
		};

		auto RenderARGB32 = [](euclase::PixelRGBA const *src, euclase::PixelRGBA *dst, uint8_t const *msk, int w, RenderOption const &opt){
			if (opt.mode == RenderOption::DirectCopy) {
				for (int j = 0; j < w; j++) {
					euclase::PixelRGBA color = src[j];
					std::swap(color.r, color.b);
					dst[j] = color;
				}
			} else {
				for (int j = 0; j < w; j++) {
					euclase::PixelRGBA color = src[j];
					std::swap(color.r, color.b);
					color.a = color.a * msk[j] / 255;
					dst[j] = AlphaBlend::blend_with_gamma_collection(dst[j], color);
				}
			}
		};

		auto RenderRGB32 = [](euclase::PixelRGBA const *src, euclase::PixelRGBA *dst, uint8_t const *msk, int w, RenderOption const &opt){
			if (opt.mode == RenderOption::DirectCopy) {
				for (int j = 0; j < w; j++) {
					euclase::PixelRGBA color = src[j];
					std::swap(color.r, color.b);
					color.a = 255;
					dst[j] = color;
				}
			} else {
				for (int j = 0; j < w; j++) {
					euclase::PixelRGBA color = src[j];
					std::swap(color.r, color.b);
					color.a = 255;
					color.a = color.a * msk[j] / 255;
					dst[j] = AlphaBlend::blend_with_gamma_collection(dst[j], color);
				}
			}
		};

		std::function<void(euclase::PixelRGBA const *src, euclase::PixelRGBA *dst, uint8_t const *msk, int w, RenderOption const &opt)> renderer;

		if (input_image->format() == QImage::Format_RGBA8888) {
			renderer = RenderRGBA8888;
		} else if (input_image->format() == QImage::Format_ARGB32) {
			renderer = RenderARGB32;
		} else if (input_image->format() == QImage::Format_RGB32) {
			renderer = RenderRGB32;
		}

		if (renderer) {
			for (int i = 0; i < h; i++) {
				uint8_t const *msk = !maskimg ? tmpmask : maskimg->scanLine(i);
				euclase::PixelRGBA const *src = reinterpret_cast<euclase::PixelRGBA const *>(input_image->scanLine(sy + i));
				euclase::PixelRGBA *dst = reinterpret_cast<euclase::PixelRGBA *>(target_panel->scanLine(dy + i));
				renderer(src + sx, dst + dx, msk, w, opt);
			}
			return;
		}
	}

	if (target_panel->isGrayscale8()) {

		auto RenderRGBA8888 = [](euclase::PixelRGBA const *src, uint8_t *dst, uint8_t const *msk, int w, RenderOption const &opt){
			for (int j = 0; j < w; j++) {
				euclase::PixelRGBA color = src[j];
				color.a = color.a * msk[j] / 255;
				euclase::PixelGrayA d(dst[j]);
				d = AlphaBlend::blend_with_gamma_collection(euclase::PixelRGBA(d), color);
				dst[j] = d.gray();
			}
		};

		auto RenderARGB32 = [](euclase::PixelRGBA const *src, uint8_t *dst, uint8_t const *msk, int w, RenderOption const &opt){
			for (int j = 0; j < w; j++) {
				euclase::PixelRGBA color = src[j];
				std::swap(color.r, color.b);
				color.a = color.a * msk[j] / 255;
				euclase::PixelGrayA d(dst[j]);
				d = AlphaBlend::blend_with_gamma_collection(euclase::PixelRGBA(d), color);
				dst[j] = d.gray();
			}
		};

		auto RenderRGB32 = [](euclase::PixelRGBA const *src, uint8_t *dst, uint8_t const *msk, int w, RenderOption const &opt){
			for (int j = 0; j < w; j++) {
				euclase::PixelRGBA color = src[j];
				std::swap(color.r, color.b);
				color.a = 255;
				color.a = color.a * msk[j] / 255;
				euclase::PixelGrayA d(dst[j]);
				d = AlphaBlend::blend_with_gamma_collection(d, color);
				dst[j] = d.gray();
			}
		};

		std::function<void(euclase::PixelRGBA const *src, uint8_t *dst, uint8_t const *msk, int w, RenderOption const &opt)> renderer;

		if (input_image->format() == QImage::Format_RGBA8888) {
			renderer = RenderRGBA8888;
		} else if (input_image->format() == QImage::Format_ARGB32) {
			renderer = RenderARGB32;
		} else if (input_image->format() == QImage::Format_RGB32) {
			renderer = RenderRGB32;
		}

		if (renderer) {
			for (int i = 0; i < h; i++) {
				uint8_t const *msk = !maskimg ? tmpmask : maskimg->scanLine(i);
				euclase::PixelRGBA const *src = reinterpret_cast<euclase::PixelRGBA const *>(input_image->scanLine(sy + i));
				uint8_t *dst = reinterpret_cast<uint8_t *>(target_panel->scanLine(dy + i));
				renderer(src + sx, dst + dx, msk, w, opt);
			}
			return;
		}
	}

}

void Document::renderToEachPanels_(Image *target_panel, QPoint const &target_offset, Layer const &input_layer, Layer *mask_layer, QColor const &brush_color, int opacity, bool *abort)
{
	if (mask_layer && mask_layer->panels_.empty()) {
		mask_layer = nullptr;
	}
//	for (PanelPtr const &input_panel : input_layer.panels_) {
#pragma omp parallel for
	for (int i = 0; i < (int)input_layer.panels_.size(); i++) {
		if (abort && *abort) {
//			break;
			continue;
		}
		PanelPtr const &input_panel = input_layer.panels_[i];
		RenderOption opt;
		renderToSinglePanel(target_panel, target_offset, input_panel.image(), input_layer.offset(), mask_layer, opt, brush_color, opacity);
	}
}

void Document::renderToEachPanels(Image *target_panel, QPoint const &target_offset, Layer const &input_layer, Layer *mask_layer, QColor const &brush_color, int opacity, QMutex *sync, bool *abort)
{
	if (sync) {
		QMutexLocker lock(sync);
		renderToEachPanels(target_panel, target_offset, input_layer, mask_layer, brush_color, opacity, nullptr, abort);
		return;
	}

	renderToEachPanels_(target_panel, target_offset, input_layer, mask_layer, brush_color, opacity, abort);
}

void Document::renderToLayer(Layer *target_layer, Layer const &input_layer, Layer *mask_layer, RenderOption const &opt, QMutex *sync, bool *abort)
{
	for (PanelPtr const &input_panel : input_layer.panels_) {
		if (input_panel.isImage()) {
			if (target_layer->tile_mode_) {
				auto FindPanel = [](Layer const *layer, int x, int y){
					int lo = 0;
					int hi = (int)layer->panels_.size();
					while (lo < hi) {
						int m = (lo + hi) / 2;
						PanelPtr p = layer->panels_[m];
						Q_ASSERT(p);
						auto COMP = [](PanelPtr const &p, int x, int y){
							if (p->offset().y() < y) return -1;
							if (p->offset().y() > y) return 1;
							if (p->offset().x() < x) return -1;
							if (p->offset().x() > x) return 1;
							return 0;
						};
						int i = COMP(p, x, y);
						if (i == 0) return p;
						if (i < 0) {
							lo = m + 1;
						} else {
							hi = m;
						}
					}
					return PanelPtr();
				};
				QPoint s0 = input_layer.offset() + input_panel->offset();
				QPoint s1 = s0 + QPoint(input_panel->width(), input_panel->height());
				for (int y = (s0.y() & ~63); y < s1.y(); y += 64) {
					for (int x = (s0.x() & ~63); x < s1.x(); x += 64) {
						if (abort && *abort) return;
						if (sync) sync->lock();
						PanelPtr panel = FindPanel(target_layer, x, y);
						if (!panel) {
							panel = target_layer->addImagePanel(x, y, 64, 64);
							panel.image()->fill(Qt::transparent);
						}
						{
							if (panel.isImage()) {
								renderToSinglePanel(panel.image(), target_layer->offset(), input_panel.image(), input_layer.offset(), mask_layer, opt, opt.brush_color, 255, abort);
							}
						}
						if (sync) sync->unlock();
					}
				}
			} else {
				int count = 0;
				for (PanelPtr &panel : target_layer->panels_) {
					if (abort && *abort) return;
					if (sync) sync->lock();
					if (panel.isImage()) {
						RenderOption opt;
						renderToSinglePanel(panel.image(), target_layer->offset(), input_panel.image(), input_layer.offset(), mask_layer, opt, opt.brush_color, 255, abort);
					}
					if (sync) sync->unlock();
					count++;
				}
				if (count == 0) {
					PanelPtr panel = PanelPtr::makeImage();
					panel->setImage(input_panel->copyImage());
					panel->setOffset(input_panel->offset());
					if (sync) sync->lock();
					target_layer->panels_.push_back(panel);
					if (sync) sync->unlock();
				}
			}
		}
	}
}

void Document::clearSelection(QMutex *sync)
{
	selection_layer()->clear(sync);
	selection_layer()->tile_mode_ = true;
}

void Document::clear(QMutex *sync)
{
	m->size = QSize();
	clearSelection(sync);
	current_layer()->clear(sync);
}

void Document::paintToCurrentLayer(Layer const &source, RenderOption const &opt, QMutex *sync, bool *abort)
{
	renderToLayer(&m->current_layer, source, selection_layer(), opt, sync, abort);
}

void Document::addSelection(Layer const &source, RenderOption const &opt, QMutex *sync, bool *abort)
{
	RenderOption o = opt;
	o.brush_color = Qt::white;
	renderToLayer(selection_layer(), source, nullptr, o, sync, abort);
}

void Document::subSelection(Layer const &source, RenderOption const &opt, QMutex *sync, bool *abort)
{
	RenderOption o = opt;
	o.brush_color = Qt::black;
	renderToLayer(selection_layer(), source, nullptr, o, sync, abort);
}

Document::Image Document::renderSelection(const QRect &r, QMutex *sync, bool *abort) const
{
	Image panel;
	panel.make(r.width(), r.height(), QImage::Format_Grayscale8);
	panel.fill(Qt::black);
	panel.setOffset(r.topLeft());
	renderToEachPanels(&panel, QPoint(), *selection_layer(), nullptr, QColor(), 255, sync, abort);
	return panel;
}

Document::Image Document::renderToLayer(const QRect &r, bool quickmask, QMutex *sync, bool *abort) const
{
	Image panel;
	panel.make(r.width(), r.height(), QImage::Format_RGBA8888);
	panel.fill(Qt::transparent);
	panel.setOffset(r.topLeft());
	renderToEachPanels(&panel, QPoint(), *current_layer(), nullptr, QColor(), 255, sync, abort);
	if (quickmask) {
		renderToEachPanels(&panel, QPoint(), *selection_layer(), nullptr, QColor(255, 0, 0), -128, sync, abort);
	}
	return panel;
}

Document::Image Document::crop(const QRect &r, QMutex *sync, bool *abort) const
{
	Image panel;
	panel.make(r.width(), r.height(), QImage::Format_RGBA8888);
	panel.fill(Qt::transparent);
	panel.setOffset(r.topLeft());
	renderToEachPanels(&panel, QPoint(), *current_layer(), selection_layer(), QColor(), 255, sync, abort);
	return panel;
}

void Document::crop2(const QRect &r)
{
	current_layer()->setOffset(current_layer()->offset() - r.topLeft());
	selection_layer()->setOffset(selection_layer()->offset() - r.topLeft());
	setSize(r.size());
}

QRect Document::Layer::rect() const
{
	QRect rect;
	const_cast<Layer *>(this)->eachPanel([&](Image *p){
		if (p->format() == QImage::Format_Grayscale8) {
			int w = p->width();
			int h = p->height();
			int x0 = w;
			int y0 = h;
			int x1 = 0;
			int y1 = 0;
			for (int y = 0; y < h; y++) {
				uint8_t const *s = p->scanLine(y);
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
		} else if (p->format() == QImage::Format_RGBA8888) {
			int w = p->width();
			int h = p->height();
			int x0 = w;
			int y0 = h;
			int x1 = 0;
			int y1 = 0;
			for (int y = 0; y < h; y++) {
				euclase::PixelRGBA const *s = (euclase::PixelRGBA const *)p->scanLine(y);
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

void Document::changeSelection(SelectionOperation op, const QRect &rect, QMutex *sync)
{
	Document::Layer layer;
	auto panel = layer.addImagePanel();
	panel->setOffset(rect.topLeft());
	panel->make(rect.size(), QImage::Format_Grayscale8);
	panel->fill(Qt::white);
//	if (0) {
//		panel->fill(Qt::black);
//		QPainter pr(&panel->getImage());
//		pr.setRenderHint(QPainter::Antialiasing);
//		pr.setBrush(Qt::white);
//		pr.drawEllipse(0, 0, rect.width(), rect.height());
//	}

	RenderOption opt;

	switch (op) {
	case SelectionOperation::SetSelection:
		clearSelection(sync);
		addSelection(layer, opt, sync, nullptr);
		break;
	case SelectionOperation::AddSelection:
		addSelection(layer, opt, sync, nullptr);
		break;
	case SelectionOperation::SubSelection:
		subSelection(layer, opt, sync, nullptr);
		break;
	}
}

