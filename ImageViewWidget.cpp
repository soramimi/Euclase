
#include "ImageViewWidget.h"
#include "AlphaBlend.h"
#include "ApplicationGlobal.h"
#include "Canvas.h"
#include "MainWindow.h"
#include "PanelizedImage.h"
#include "SelectionOutline.h"
#include "misc.h"
#include <QBitmap>
#include <QBuffer>
#include <QDebug>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QSvgRenderer>
#include <QWheelEvent>
#include <cmath>
#include <memory>
#include <mutex>
#include <thread>

using SvgRendererPtr = std::shared_ptr<QSvgRenderer>;

const int MAX_SCALE = 32;
const int MIN_SCALE = 16;

struct ImageViewWidget::Private {
	MainWindow *mainwindow = nullptr;
	QScrollBar *v_scroll_bar = nullptr;
	QScrollBar *h_scroll_bar = nullptr;
	QString mime_type;
	QMutex sync;

	struct Data {
		QPointF view_scroll_offset;
		double view_scale = 1;
		QPointF scroll_starting_offset;
		QPoint mouse_pos;
		QPoint mouse_press_pos;
		int wheel_delta = 0;
		QPointF cursor_anchor_pos;
		QPointF center_anchor_pos;
		int top_margin = 1;
		int bottom_margin = 1;
	};
	Data d;

	bool left_button = false;

	int delayed_update_counter = 0; // x 100ms

	bool scrolling = false;

	int stripe_animation = 0;
	int rectangle_animation = 0;

	bool rect_visible = false;
	QPointF rect_start;
	QPointF rect_end;

	QBrush horz_stripe_brush;
	QBrush vert_stripe_brush;

	QCursor cursor;

	QPixmap transparent_pixmap;

	std::thread image_rendering_thread;
	std::mutex render_mutex;
	bool render_interrupted = false;
	bool render_invalidate = false;
	bool render_requested = false;
	bool render_canceled = false;
	std::vector<QRect> render_canvas_rects;
	std::vector<Canvas::Panel> composed_panels_cache;

	CoordinateMapper offscreen1_mapper;
	PanelizedImage offscreen1;

	std::thread selection_outline_thread;
	SelectionOutline selection_outline;
	bool selection_outline_requested = false;
	QPixmap offscreen2;
};

ImageViewWidget::ImageViewWidget(QWidget *parent)
	: QWidget(parent)
	, m(new Private)
{
	setContextMenuPolicy(Qt::DefaultContextMenu);

	initBrushes();

	setMouseTracking(true);

	startRenderingThread();

	connect(this, &ImageViewWidget::notifySelectionOutlineReady, this, &ImageViewWidget::onSelectionOutlineReady);

	connect(&timer_, &QTimer::timeout, this, &ImageViewWidget::onTimer);
	timer_.start(100);
}

ImageViewWidget::~ImageViewWidget()
{
	stopRenderingThread();
	delete m;
}

void ImageViewWidget::initBrushes()
{
	{
		QImage image(8, 8, QImage::Format_Grayscale8);
		for (int y = 0; y < 8; y++) {
			uint8_t *p = image.scanLine(y);
			for (int x = 0; x < 8; x++) {
				p[x] = y < 4 ? 0 : 255;
			}
		}
		m->horz_stripe_brush = QBrush(image);
	}
	{
		QImage image(8, 8, QImage::Format_Grayscale8);
		for (int y = 0; y < 8; y++) {
			uint8_t *p = image.scanLine(y);
			for (int x = 0; x < 8; x++) {
				p[x] = x < 4 ? 0 : 255;
			}
		}
		m->vert_stripe_brush = QBrush(image);
	}
}

MainWindow *ImageViewWidget::mainwindow()
{
	return m->mainwindow;
}

Canvas *ImageViewWidget::canvas()
{
	return m->mainwindow->canvas();
}

Canvas const *ImageViewWidget::canvas() const
{
	return m->mainwindow->canvas();
}

void ImageViewWidget::init(MainWindow *mainwindow, QScrollBar *vsb, QScrollBar *hsb)
{
	m->mainwindow = mainwindow;
	m->v_scroll_bar = vsb;
	m->h_scroll_bar = hsb;
}

static QImage scale_float_to_uint8_rgba(euclase::Image const &src, int w, int h)
{
	QImage ret(w, h, QImage::Format_RGBA8888);
	if (src.memtype() == euclase::Image::CUDA) {
		int dstride = ret.bytesPerLine();
		global->cuda->scale_float_to_uint8_rgba(w, h, dstride, ret.bits(), src.width(), src.height(), src.data());
	} else {
		QImage qimg = src.qimage();
		if (qimg.isNull()) return {};
		return qimg.scaled(w, h, Qt::IgnoreAspectRatio, Qt::FastTransformation);
	}
	return ret;
}

void ImageViewWidget::clearRenderCache()
{
	std::lock_guard lock(m->render_mutex);
	m->offscreen1_mapper = CoordinateMapper(size(), m->d.view_scroll_offset, m->d.view_scale);
	m->composed_panels_cache.clear();
	m->render_canvas_rects.clear();
	m->render_requested = true;
	m->render_canceled = true;
}

void ImageViewWidget::runSelectionRendering()
{
	while (1) {
		if (m->render_interrupted) break;
		if (m->selection_outline_requested) {
			m->selection_outline_requested = false;
			SelectionOutline selection_outline = renderSelectionOutline(&m->selection_outline_requested);
			emit notifySelectionOutlineReady(selection_outline);
		} else {
			std::this_thread::yield();
		}
	}
}

void ImageViewWidget::invalidateComposedPanels(QRect const &rect)
{
	std::lock_guard lock(m->render_mutex);
	if (rect.isEmpty()) {
		m->composed_panels_cache.clear();
	} else {
		size_t i = m->composed_panels_cache.size();
		while (i > 0) {
			i--;
			QRect r(m->composed_panels_cache[i].offset(), m->composed_panels_cache[i].image().size());
			if (r.intersects(rect)) {
				m->composed_panels_cache.erase(m->composed_panels_cache.begin() + i);
			}
		}
	}
}

CoordinateMapper ImageViewWidget::currentCoordinateMapper() const
{
	return CoordinateMapper(size(), m->d.view_scroll_offset, m->d.view_scale);
}

CoordinateMapper ImageViewWidget::offscreenCoordinateMapper() const
{
	return m->offscreen1_mapper;
}

void ImageViewWidget::requestRendering(bool invalidate, QRect const &rect)
{
	QRect r = rect;
	if (0) {//if (invalidate) {
		m->render_invalidate = true;
		r = {};
	}

	invalidateComposedPanels(r);

	if (r.isEmpty()) {
		auto topleft = mapToCanvasFromViewport(QPointF(0, 0));
		auto bottomright = mapToCanvasFromViewport(QPointF(width(), height()));
		int x0 = (int)floor(topleft.x()) - 1;
		int y0 = (int)floor(topleft.y()) - 1;
		int x1 = (int)ceil(bottomright.x()) + 1;
		int y1 = (int)ceil(bottomright.y()) + 1;
		r = {x0, y0, x1 - x0, y1 - y0};
	}
	{
		std::lock_guard lock(m->render_mutex);
		m->offscreen1_mapper = currentCoordinateMapper();
		m->render_canvas_rects.push_back(r);
		m->render_requested = true;
	}
}

void ImageViewWidget::startRenderingThread()
{
	m->image_rendering_thread = std::thread([&](){
		runImageRendering();
	});
	m->selection_outline_thread = std::thread([&](){
		runSelectionRendering();
	});
}

void ImageViewWidget::stopRenderingThread()
{
	m->render_interrupted = true;
	if (m->image_rendering_thread.joinable()) {
		m->image_rendering_thread.join();
	}
	if (m->selection_outline_thread.joinable()) {
		m->selection_outline_thread.join();
	}
}

QPointF ImageViewWidget::mapToCanvasFromViewport(QPointF const &pos)
{
	return currentCoordinateMapper().mapToCanvasFromViewport(pos);
}

QPointF ImageViewWidget::mapToViewportFromCanvas(QPointF const &pos)
{
	return currentCoordinateMapper().mapToViewportFromCanvas(pos);
}

void ImageViewWidget::showRect(QPointF const &start, QPointF const &end)
{
	m->rect_start = start;
	m->rect_end = end;
	m->rect_visible = true;
	update();
}

void ImageViewWidget::hideRect(bool update)
{
	m->rect_visible = false;

	if (update) {
		this->update();
	}
}

bool ImageViewWidget::isRectVisible() const
{
	return m->rect_visible;
}

QBrush ImageViewWidget::stripeBrush()
{
	QImage image(8, 8, QImage::Format_Indexed8);
	image.setColor(0, qRgb(0, 0, 0));
	image.setColor(1, qRgb(255, 255, 255));
	// ストライプパターン
	const int anim = m->stripe_animation;
	for (int y = 0; y < 8; y++) {
		uint8_t *p = image.scanLine(y);
		for (int x = 0; x < 8; x++) {
			p[x] = ((anim - x - y) & 4) ? 1 : 0;
		}
	}
	return QBrush(image);
}

void ImageViewWidget::geometryChanged(bool force_render)
{
	if (force_render) {
		requestRendering(true, {});
	}
}

QSize ImageViewWidget::imageScrollRange() const
{
	QSize sz = imageSize();
	int w = int(sz.width() * m->d.view_scale);
	int h = int(sz.height() * m->d.view_scale);
	return QSize(w, h);
}

void ImageViewWidget::setScrollOffset(double x, double y)
{
	QSizeF sz = imageScrollRange();
	x = std::min(std::max(x, 0.0), sz.width());
	y = std::min(std::max(y, 0.0), sz.height());
	m->d.view_scroll_offset = QPointF(x, y);
}

void ImageViewWidget::internalScrollImage(double x, double y, bool render)
{
	setScrollOffset(x, y);

	geometryChanged(false);

	if (render) {
		// requestRendering(false, {});
		paintViewLater(true);
	}

	update();
}

void ImageViewWidget::scrollImage(double x, double y, bool updateview)
{
	internalScrollImage(x, y, updateview);

	if (m->h_scroll_bar) {
		auto b = m->h_scroll_bar->blockSignals(true);
		m->h_scroll_bar->setValue((int)m->d.view_scroll_offset.x());
		m->h_scroll_bar->blockSignals(b);
	}
	if (m->v_scroll_bar) {
		auto b = m->v_scroll_bar->blockSignals(true);
		m->v_scroll_bar->setValue((int)m->d.view_scroll_offset.y());
		m->v_scroll_bar->blockSignals(b);
	}
}

void ImageViewWidget::refrectScrollBar()
{
	double e = 0.75;
	double x = m->h_scroll_bar->value();
	double y = m->v_scroll_bar->value();
	if (fabs(x - m->d.view_scroll_offset.x()) < e) x = m->d.view_scroll_offset.x(); // 差が小さいときは値を維持する
	if (fabs(y - m->d.view_scroll_offset.y()) < e) y = m->d.view_scroll_offset.y();
	internalScrollImage(x, y, true);
}

void ImageViewWidget::setScrollBarRange(QScrollBar *h, QScrollBar *v)
{
	auto bh = h->blockSignals(true);
	auto bv = v->blockSignals(true);
	QSizeF sz = imageScrollRange();
	h->setRange(0, (int)sz.width());
	v->setRange(0, (int)sz.height());
	h->setPageStep(width());
	v->setPageStep(height());
	h->blockSignals(bh);
	v->blockSignals(bv);
}

void ImageViewWidget::updateScrollBarRange()
{
	setScrollBarRange(m->h_scroll_bar, m->v_scroll_bar);
}

QBrush ImageViewWidget::getTransparentBackgroundBrush()
{
	if (m->transparent_pixmap.isNull()) {
		m->transparent_pixmap = QPixmap(":/image/transparent.png");
	}
	return m->transparent_pixmap;
}

QSize ImageViewWidget::imageSize() const
{
	return canvas()->size();
}

void ImageViewWidget::paintViewLater(bool image)
{
	if (image) {
		QPointF pt0 = mapToCanvasFromViewport(QPointF(0, 0));
		QPointF pt1 = mapToCanvasFromViewport(QPointF(width(), height()));
		int x0 = (int)floor(pt0.x());
		int y0 = (int)floor(pt0.y());
		int x1 = (int)ceil(pt1.x());
		int y1 = (int)ceil(pt1.y());
		x0 = std::max(x0, 0);
		y0 = std::max(y0, 0);
		x1 = std::min(x1, canvas()->width());
		y1 = std::min(y1, canvas()->height());
		QRect r(x0, y0, x1 - x0, y1 - y0);

		QPoint mousepos = mapFromGlobal(QCursor::pos());
		QPointF focus_point = mapToCanvasFromViewport(mousepos);

		int div = 1;
		if (m->d.view_scale < 1) {
			Q_ASSERT(m->d.view_scale > 0);
			div = (int)floorf(1.0f / m->d.view_scale);
		}
	}

	m->selection_outline_requested = true;
}

void ImageViewWidget::updateCursorAnchorPos()
{
	m->d.cursor_anchor_pos = mapToCanvasFromViewport(mapFromGlobal(QCursor::pos()));
}

void ImageViewWidget::updateCenterAnchorPos()
{
	m->d.center_anchor_pos = mapToCanvasFromViewport(QPointF(width() / 2.0, height() / 2.0));
}

bool ImageViewWidget::setImageScale(double scale, bool updateview)
{
	if (scale < 1.0 / MIN_SCALE) scale = 1.0 / MIN_SCALE;
	if (scale > MAX_SCALE) scale = MAX_SCALE;
	if (m->d.view_scale == scale) return false;

	m->d.view_scale = scale;

	geometryChanged(false);

	emit scaleChanged(m->d.view_scale);

	if (updateview) {
		// requestRendering(true, {});
		paintViewLater(true);
		update();
	}

	return true;
}

void ImageViewWidget::scaleFit(double ratio)
{
	QSize sz = imageSize();
	double w = sz.width();
	double h = sz.height();
	if (w > 0 && h > 0) {
		double sx = width() / w;
		double sy = height() / h;
		m->d.view_scale = (sx < sy ? sx : sy) * ratio;
	}
	updateScrollBarRange();

	scrollImage(w * m->d.view_scale / 2.0, h * m->d.view_scale / 2.0, true);
	updateCursorAnchorPos();
}

void ImageViewWidget::clearSelectionOutline()
{

}

void ImageViewWidget::zoomToCursor(double scale)
{
	if (!setImageScale(scale, false)) return;

	clearSelectionOutline();

	QPoint pos = mapFromGlobal(QCursor::pos());

	updateScrollBarRange();

	double x = m->d.cursor_anchor_pos.x() * m->d.view_scale + width() / 2.0 - (pos.x() + 0.5);
	double y = m->d.cursor_anchor_pos.y() * m->d.view_scale + height() / 2.0 - (pos.y() + 0.5);
	scrollImage(x, y, true);

	updateCenterAnchorPos();
}

void ImageViewWidget::zoomToCenter(double scale)
{
	clearSelectionOutline();

	QPointF pos(width() / 2.0, height() / 2.0);
	m->d.cursor_anchor_pos = mapToCanvasFromViewport(pos);

	setImageScale(scale, false);
	updateScrollBarRange();

	double x = m->d.cursor_anchor_pos.x() * m->d.view_scale + width() / 2.0 - pos.x();
	double y = m->d.cursor_anchor_pos.y() * m->d.view_scale + height() / 2.0 - pos.y();
	scrollImage(x, y, true);

	updateCenterAnchorPos();
}

void ImageViewWidget::scale100()
{
	zoomToCenter(1.0);
}

void ImageViewWidget::zoomIn()
{
	zoomToCenter(m->d.view_scale * 2);
}

void ImageViewWidget::zoomOut()
{
	zoomToCenter(m->d.view_scale / 2);
}

QImage ImageViewWidget::generateOutlineImage(euclase::Image const &selection, bool *abort)
{
	QImage image;
	int w = selection.width();
	int h = selection.height();

	if (selection.memtype() == euclase::Image::CUDA) {
		euclase::Image tmpimg(w, h, euclase::Image::Format_8_Grayscale, euclase::Image::CUDA);
		global->cuda->outline_uint8_grayscale(w, h, selection.data(), tmpimg.data());
		return tmpimg.qimage();
	}

	if (selection.memtype() == euclase::Image::Host) {
		image = QImage(w, h, QImage::Format_Grayscale8);
		image.fill(Qt::white);
		for (int y = 1; y + 1 < h; y++) {
			if (abort && *abort) return {};
			uint8_t const *s0 = selection.scanLine(y - 1);
			uint8_t const *s1 = selection.scanLine(y);
			uint8_t const *s2 = selection.scanLine(y + 1);
			uint8_t *d = image.scanLine(y);
			for (int x = 1; x + 1 < w; x++) {
				uint8_t v = ~(s0[x - 1] & s0[x] & s0[x + 1] & s1[x - 1] & s1[x + 1] & s2[x - 1] & s2[x] & s2[x + 1]) & s1[x];
				d[x] = (v & 0x80) ? 0 : 255;
			}
		}
		return image;
	}

	return {};
}

/**
 * @brief ImageViewWidget::renderSelectionOutline
 * @param abort
 * @return
 *
 * 画像の選択範囲を描画する
 */
SelectionOutline ImageViewWidget::renderSelectionOutline(bool *abort)
{
	SelectionOutline data;
	int dw = canvas()->width();
	int dh = canvas()->height();
	if (dw > 0 && dh > 0) {
		QPointF dp0(0, 0);
		QPointF dp1(width(), height());
		dp0 = mapToCanvasFromViewport(dp0);
		dp1 = mapToCanvasFromViewport(dp1);
		dp0.rx() = floor(std::max(dp0.rx(), (double)0));
		dp0.ry() = floor(std::max(dp0.ry(), (double)0));
		dp1.rx() = ceil(std::min(dp1.rx(), (double)dw));
		dp1.ry() = ceil(std::min(dp1.ry(), (double)dh));
		QPointF vp0 = mapToViewportFromCanvas(dp0);
		QPointF vp1 = mapToViewportFromCanvas(dp1);
		vp0.rx() = floor(vp0.rx());
		vp0.ry() = floor(vp0.ry());
		vp1.rx() = ceil(vp1.rx());
		vp1.ry() = ceil(vp1.ry());
		int vx = (int)vp0.x();
		int vy = (int)vp0.y();
		int vw = (int)vp1.x() - vx;
		int vh = (int)vp1.y() - vy;
		euclase::Image selection;
		{
			int dx = int(dp0.x());
			int dy = int(dp0.y());
			int dw = int(dp1.x()) - dx;
			int dh = int(dp1.y()) - dy;
			selection = canvas()->renderSelection(QRect(dx, dy, dw, dh), abort).image();
			if (abort && *abort) return {};
			if (selection.memtype() == euclase::Image::CUDA) {
				euclase::Image sel(vw, vh, euclase::Image::Format_8_Grayscale, euclase::Image::CUDA);
				int sw = selection.width();
				int sh = selection.height();
				global->cuda->scale(vw, vh, vw, sel.data(), sw, sh, sw, selection.data(), 1);
				selection = sel;
			} else {
				selection = selection.scaled(vw, vh, false);
			}
			selection = selection.toHost();
		}
		if (selection.width() > 0 && selection.height() > 0) {
			QImage image = generateOutlineImage(selection, abort);
			if (image.isNull()) return {};
			data.bitmap = QBitmap::fromImage(image);
			data.point = QPoint(vx, vy);
		}
	}
	return data;
}

/**
 * @brief ImageViewWidget::runImageRendering
 *
 * オフスクリーンへレンダリングする
 */
void ImageViewWidget::runImageRendering()
{
	while (1) {
		if (m->render_interrupted) return;
		if (m->render_requested) {
			m->render_requested = false;
			m->render_canceled = false;

			// qDebug() << Q_FUNC_INFO;

			Q_ASSERT(m->offscreen1.offset().x() == 0); // オフスクリーンの原点は常に(0, 0)
			Q_ASSERT(m->offscreen1.offset().y() == 0);

			CoordinateMapper mapper;
			{
				std::lock_guard lock(m->render_mutex);
				mapper = m->offscreen1_mapper;
				if (m->render_invalidate) {
					m->render_invalidate = false;
					m->offscreen1.clear();
				}
			}

			const int canvas_w = mainwindow()->canvasWidth();
			const int canvas_h = mainwindow()->canvasHeight();
			const QRect canvas_rect(0, 0, canvas_w, canvas_h);

			int view_left;
			int view_top;
			int view_right;
			int view_bottom;
			{
				QPointF topleft = mapper.mapToViewportFromCanvas(QPointF(0, 0));
				QPointF bottomright = mapper.mapToViewportFromCanvas(QPointF(canvas_w, canvas_h));
				view_left = std::max((int)floor(topleft.x()), 0);
				view_top = std::max((int)floor(topleft.y()), 0);
				view_right = std::min((int)ceil(bottomright.x() + 1), width()); // ceilだけでは足りなことがあるので 1 足す
				view_bottom = std::min((int)ceil(bottomright.y() + 1), height());
			}

			std::vector<QRect> rects;

			{
				std::lock_guard lock(m->render_mutex);
				QPointF topleft = mapper.mapToCanvasFromViewport(QPointF(0, 0));
				QPointF bottomright = mapper.mapToCanvasFromViewport(QPointF(width(), height()));
				const int S1 = OFFSCREEN_PANEL_SIZE - 1;
				int x0 = (int)topleft.x() & ~S1;
				int y0 = (int)topleft.y() & ~S1;
				int x1 = (int)bottomright.x() & ~S1;
				int y1 = (int)bottomright.y() & ~S1;
				for (int y = y0; y <= y1; y += OFFSCREEN_PANEL_SIZE) {
					for (int x = x0; x <= x1; x += OFFSCREEN_PANEL_SIZE) {
						QRect rect(x, y, OFFSCREEN_PANEL_SIZE, OFFSCREEN_PANEL_SIZE);
						if (rect.intersects(canvas_rect)) {
							rects.push_back(rect);
						}
					}
				}
				m->render_canvas_rects.clear();
			}

			if (1) { // マウスカーソルから近い順にソート
				QPoint center = mapper.mapToCanvasFromViewport(mapFromGlobal(QCursor::pos())).toPoint();
				std::sort(rects.begin(), rects.end(), [&](QRect const &a, QRect const &b){
					auto Center = [=](QRect const &r){
						return r.center();
					};
					auto Distance = [](QPoint const &a, QPoint const &b){
						auto dx = a.x() - b.x();
						auto dy = a.y() - b.y();
						return sqrt(dx * dx + dy * dy);
					};
					QPoint ca = Center(a);
					QPoint cb = Center(b);
					return Distance(ca, center) < Distance(cb, center);
				});
			}

			auto isCanceled = [&](){
				bool f = /*m->render_requested ||*/ m->render_canceled || /*m->render_invalidate ||*/ m->render_interrupted;
				return f;
			};

			int panelindex = 0;
			std::atomic_int rectindex = 0;

#pragma omp parallel for num_threads(16)
			for (int i = 0; i < rects.size(); i++) {
				if (isCanceled()) continue;

				// パネル矩形
				QRect const &rect = rects[rectindex++];
				const int x = rect.x();
				const int y = rect.y();
				const int w = rect.width();
				const int h = rect.height();

				// 描画先座標
				QPointF src_topleft(x, y);
				QPointF src_bottomright(x + w, y + h);

				// 描画元座標
				QPointF dst_topleft = mapper.mapToViewportFromCanvas(src_topleft);
				QPointF dst_bottomright = mapper.mapToViewportFromCanvas(src_bottomright);

				// 描画範囲でクリップ
				if (dst_topleft.x() < view_left) dst_topleft.rx() = view_left;
				if (dst_topleft.y() < view_top) dst_topleft.ry() = view_top;
				if (dst_bottomright.x() > view_right) dst_bottomright.rx() = view_right;
				if (dst_bottomright.y() > view_bottom) dst_bottomright.ry() = view_bottom;

				// 整数化
				src_topleft = mapper.mapToCanvasFromViewport(dst_topleft);
				src_bottomright = mapper.mapToCanvasFromViewport(dst_bottomright);
				src_topleft.rx() = std::max((int)floor(src_topleft.x()), x);
				src_topleft.ry() = std::max((int)floor(src_topleft.y()), y);
				src_bottomright.rx() = std::min((int)ceil(src_bottomright.x()), x + w);
				src_bottomright.ry() = std::min((int)ceil(src_bottomright.y()), y + h);

				// 描画先を再計算
				dst_topleft = mapper.mapToViewportFromCanvas(src_topleft);
				dst_bottomright = mapper.mapToViewportFromCanvas(src_bottomright);
				int dx = (int)round(dst_topleft.x());
				int dy = (int)round(dst_topleft.y());
				int dw = (int)ceil(dst_bottomright.x()) - dx;
				int dh = (int)ceil(dst_bottomright.y()) - dy;

				// 描画元座標を確定
				int sx = (int)src_topleft.x();
				int sy = (int)src_topleft.y();
				int sw = (int)src_bottomright.x() - sx;
				int sh = (int)src_bottomright.y() - sy;
				if (sw <= 0 || sh <= 0) continue;

				// パネル原点を引く
				sx -= x;
				sy -= y;

				euclase::Image image;
				Canvas::Panel *panel = nullptr;
				{
					// パネルキャッシュから検索
					std::lock_guard lock(m->render_mutex);
					QPoint pos(x, y);
					for (int j = panelindex; j < m->composed_panels_cache.size(); j++) {
						if (m->composed_panels_cache[j].offset() == pos) {
							std::swap(m->composed_panels_cache[panelindex], m->composed_panels_cache[j]);
							panel = &m->composed_panels_cache[panelindex];
							image = cropImage(panel->image(), sx, sy, sw, sh); // 画像を切り出す
							panelindex++;
							break;
						}
					}
				}
				if (!panel) {
					// 新規パネルを作成
					Canvas::Panel newpanel = m->mainwindow->renderToPanel(Canvas::AllLayers, euclase::Image::Format_F_RGBA, rect, {}, (bool *)&m->render_interrupted);
					*newpanel.imagep() = newpanel.imagep()->toHost(); // CUDAよりCPUの方が速くて安定する
					newpanel.setOffset(x, y);
					{
						// パネルキャッシュに追加
						std::lock_guard lock(m->render_mutex);
						if (panelindex <= m->composed_panels_cache.size()) { // 別スレッドがcomposed_panels_cacheをclearすることがある
							m->composed_panels_cache.insert(m->composed_panels_cache.begin() + panelindex, newpanel);
							panel = &m->composed_panels_cache[panelindex];
							image = cropImage(panel->image(), sx, sy, sw, sh); // 画像を切り出す
						}
						panelindex++;
					}
				}

				if (isCanceled()) continue;

				// 拡大縮小
				QImage qimg = scale_float_to_uint8_rgba(image, dw, dh);
				if (qimg.isNull()) continue;

				if (isCanceled()) continue;

				// 透明部分の市松模様
				for (int iy = 0; iy < qimg.height(); iy++) {
					uint8_t *p = qimg.scanLine(iy);
					for (int ix = 0; ix < qimg.width(); ix++) {
						euclase::OctetRGBA a, b;
						b.r = b.g = b.b = (((dx + ix) ^ (dy + iy)) & 8) ? 255 : 192; // 市松模様パターン
						b.a = 255;
						a.r = p[0];
						a.g = p[1];
						a.b = p[2];
						a.a = p[3];
						a = AlphaBlend::blend(b, a);
						p[0] = a.r;
						p[1] = a.g;
						p[2] = a.b;
						p[3] = a.a;
						p += 4;
					}
				}

				if (isCanceled()) continue;

				// オフスクリーンへ描画する
				{
					std::lock_guard lock(m->render_mutex);
					if (!isCanceled()) {
						int ox = width() / 2 - m->offscreen1_mapper.scrollOffset().x();
						int oy = height() / 2 - m->offscreen1_mapper.scrollOffset().y();
						m->offscreen1.paintImage(QPoint(dx - ox, dy - oy), qimg, qimg.size(), {});
					}
				}
			}
			{
				std::lock_guard lock(m->render_mutex);

				if (!isCanceled()) {
					update();
				}

				// 多すぎるパネルを削除
				int n = panelindex * 2;
				if (m->composed_panels_cache.size() > n) {
					m->composed_panels_cache.resize(n);
				}
			}
		} else {
			std::this_thread::yield();
		}
	}
}

/**
 * @brief ImageViewWidget::paintEvent
 * @param event
 *
 * 画面描画イベントの処理
 */
void ImageViewWidget::paintEvent(QPaintEvent *)
{
	const QColor bgcolor = BGCOLOR;

	const int view_w = width();
	const int view_h = height();

	const int doc_w = canvas()->width();
	const int doc_h = canvas()->height();

	const CoordinateMapper mapper = currentCoordinateMapper();

	QPointF pt0(0, 0);
	QPointF pt1(doc_w, doc_h);
	pt0 = mapper.mapToViewportFromCanvas(pt0);
	pt1 = mapper.mapToViewportFromCanvas(pt1);
	int visible_x = (int)round(pt0.x());
	int visible_y = (int)round(pt0.y());
	int visible_w = (int)round(pt1.x()) - visible_x;
	int visible_h = (int)round(pt1.y()) - visible_y;

	QPainter pr_view(this);

	// オーバーレイ用オフスクリーンの描画は別スレッドで実行
	std::thread th = std::thread([&](){
		if (m->offscreen2.width() != view_w || m->offscreen2.height() != view_h) {
			m->offscreen2 = QPixmap(view_w, view_h);
		}
		m->offscreen2.fill(Qt::transparent);
		{
			QPainter pr2(&m->offscreen2);

			// 最大拡大時のグリッド
			if (m->d.view_scale == MAX_SCALE && !m->scrolling) { // スクロール中はグリッドを描画しない
				QPoint org = mapper.mapToViewportFromCanvas(QPointF(0, 0)).toPoint();
				QPointF topleft = mapper.mapToCanvasFromViewport(QPointF(0, 0));
				int topleft_x = (int)floor(topleft.x());
				int topleft_y = (int)floor(topleft.y());
				pr2.save();
				pr2.setOpacity(0.25);
				pr2.setBrushOrigin(org);
				int x = topleft_x;
				while (1) {
					QPointF pt = mapper.mapToViewportFromCanvas(QPointF(x, topleft_y));
					int z = (int)floor(pt.x());
					if (z >= width()) break;
					pr2.fillRect(z, 0, 1, height(), m->horz_stripe_brush);
					x++;
				}
				int y = topleft_y;
				while (1) {
					QPointF pt = mapper.mapToViewportFromCanvas(QPointF(topleft_x, y));
					int z = (int)floor(pt.y());
					if (z >= height()) break;
					pr2.fillRect(0, z, width(), 1, m->vert_stripe_brush);
					y++;
				}
				pr2.restore();
			}


			// 選択領域点線
			if (!m->selection_outline.bitmap.isNull()) {
				QBrush brush = stripeBrush();
				pr2.save();
				pr2.setClipRegion(QRegion(m->selection_outline.bitmap).translated(m->selection_outline.point));
				pr2.setOpacity(0.5);
				pr2.fillRect(0, 0, width(), height(), brush);
				pr2.restore();
			}
		}
	});

	// 画像のオフスクリーンを描画
	Q_ASSERT(m->offscreen1.offset().x() == 0); // オフスクリーンの原点は常に(0, 0)
	Q_ASSERT(m->offscreen1.offset().y() == 0);
	{
		std::lock_guard lock(m->render_mutex);
		auto osmapper = offscreenCoordinateMapper();
		for (PanelizedImage::Panel const &panel : m->offscreen1.panels_) {
			QPoint org = panel.offset - m->offscreen1_mapper.scrollOffset().toPoint();
			org.rx() += width() / 2;
			org.ry() += height() / 2;
			QPointF topleft(org);
			QPointF bottomright(org.x() + panel.image.width(), org.y() + panel.image.height());
			topleft = osmapper.mapToCanvasFromViewport(topleft);
			topleft = mapper.mapToViewportFromCanvas(topleft);
			bottomright = osmapper.mapToCanvasFromViewport(bottomright);
			bottomright = mapper.mapToViewportFromCanvas(bottomright);
			bottomright.rx() = ceil(bottomright.x());
			bottomright.ry() = ceil(bottomright.y());
			pr_view.drawImage(QRectF(topleft, bottomright), panel.image, panel.image.rect());
		}
	}

	th.join();

	// オーバーレイを描画
	pr_view.drawPixmap(0, 0, m->offscreen2);

	if (visible_w > 0 && visible_h > 0) {
		// 背景
		QPainterPath entire;
		QPainterPath viewrect;
		entire.addRect(rect());
		viewrect.addRect(visible_x, visible_y, visible_w, visible_h);
		QPainterPath outside = entire.subtracted(viewrect);
		pr_view.save();
		pr_view.setRenderHint(QPainter::Antialiasing);
		pr_view.setClipPath(outside);
		pr_view.fillRect(rect(), bgcolor);
		pr_view.restore();
		// 枠線
		pr_view.drawRect(visible_x, visible_y, visible_w, visible_h);
	} else {
		// 背景
		pr_view.fillRect(rect(), bgcolor);
	}

	// 範囲指定矩形点滅
	if (m->rect_visible) {
		double f = m->rectangle_animation * (2 * 3.1416) / 10.0;
		int v = (sin(f) + 1) * 127;
		QBrush brush = QBrush(QColor(v, v, v));
		pr_view.setOpacity(0.5); // 半透明

		// 枠線
		double x0 = m->rect_start.x();
		double y0 = m->rect_start.y();
		double x1 = m->rect_end.x();
		double y1 = m->rect_end.y();
		if (x0 > x1) std::swap(x0, x1);
		if (y0 > y1) std::swap(y0, y1);
		QPointF pt;
		pt = mapper.mapToViewportFromCanvas(QPointF(x0, y0));
		x0 = floor(pt.x());
		y0 = floor(pt.y());
		pt = mapper.mapToViewportFromCanvas(QPointF(x1, y1));
		x1 = floor(pt.x());
		y1 = floor(pt.y());
		misc::drawFrame(&pr_view, x0, y0, x1 - x0 + 1, y1 - y0 + 1, brush, brush);

		// ハンドル
		auto DrawHandle = [&](int x, int y){
			pr_view.fillRect(x - 4, y - 4, 9, 9, brush);
		};

		// 角
		DrawHandle(x0, y0);
		DrawHandle(x1, y0);
		DrawHandle(x0, y1);
		DrawHandle(x1, y1);

		// 上下左右
		DrawHandle((x0 + x1) / 2, y0);
		DrawHandle((x0 + x1) / 2, y1);
		DrawHandle(x0, (y0 + y1) / 2);
		DrawHandle(x1, (y0 + y1) / 2);

		// 中央
		DrawHandle((x0 + x1) / 2, (y0 + y1) / 2);
	}
}

/**
 * @brief ImageViewWidget::rescaleOffScreen
 *
 *  座標マッピングに応じてオフスクリーンを再描画する
 */
void ImageViewWidget::rescaleOffScreen()
{
	std::lock_guard lock(m->render_mutex);

	Q_ASSERT(m->offscreen1.offset().x() == 0); // オフスクリーンの原点は常に(0, 0)
	Q_ASSERT(m->offscreen1.offset().y() == 0);

	auto old_mapper = m->offscreen1_mapper; // 古い座標系
	auto new_mapper = currentCoordinateMapper(); // 新しい座標系

	QPointF old_org(width() / 2 - old_mapper.scrollOffset().x(), height() / 2 - old_mapper.scrollOffset().y()); // 古い座標系の原点
	QPointF new_org(width() / 2 - new_mapper.scrollOffset().x(), height() / 2 - new_mapper.scrollOffset().y()); // 新しい座標系の原点

	PanelizedImage new_offscreen;

	// 新しい座標系でオフスクリーンを再構築
	for (PanelizedImage::Panel &panel : m->offscreen1.panels_) {
		const QPointF org = old_org + panel.offset; // パネルの原点座標

		// 描画先矩形を計算
		QPointF dst_topleft(org);
		QPointF dst_bottomright = dst_topleft + QPoint(panel.image.width(), panel.image.height());
		dst_topleft = old_mapper.mapToCanvasFromViewport(dst_topleft);
		dst_topleft = new_mapper.mapToViewportFromCanvas(dst_topleft);
		dst_bottomright = old_mapper.mapToCanvasFromViewport(dst_bottomright);
		dst_bottomright = new_mapper.mapToViewportFromCanvas(dst_bottomright);

		// 画面外ならスキップ
		const int x = floor(dst_topleft.x());
		const int y = floor(dst_topleft.y());
		const int w = ceil(dst_bottomright.x()) - x;
		const int h = ceil(dst_bottomright.y()) - y;
		QRect r(x, y, w, h);
		r = r.intersected(rect());
		if (r.isEmpty()) continue;

		// 描画先座標
		int dx = (int)round(dst_topleft.x() - new_org.x());
		int dy = (int)round(dst_topleft.y() - new_org.y());

		// 描画先マスク
		QRect dstmask(r.translated(-new_org.toPoint()));

		// 描画
		new_offscreen.paintImage(QPoint(dx, dy), panel.image, QSize(w, h), dstmask);
	}

	m->offscreen1_mapper = new_mapper;
	m->offscreen1 = std::move(new_offscreen);
}

void ImageViewWidget::resizeEvent(QResizeEvent *)
{
	clearSelectionOutline();
	updateScrollBarRange();
	paintViewLater(true);
}

void ImageViewWidget::mousePressEvent(QMouseEvent *e)
{
	m->left_button = (e->buttons() & Qt::LeftButton);
	if (m->left_button) {
		QPoint pos = mapFromGlobal(QCursor::pos());
		m->d.mouse_press_pos = pos;
		m->d.scroll_starting_offset = m->d.view_scroll_offset;
		mainwindow()->onMouseLeftButtonPress(pos.x(), pos.y());
	}
}

void ImageViewWidget::internalUpdateScroll()
{
	QPoint pos = m->d.mouse_pos;
	clearSelectionOutline();
	int delta_x = pos.x() - m->d.mouse_press_pos.x();
	int delta_y = pos.y() - m->d.mouse_press_pos.y();
	scrollImage(m->d.scroll_starting_offset.x() - delta_x, m->d.scroll_starting_offset.y() - delta_y, false);
}

void ImageViewWidget::doHandScroll()
{
	if (m->left_button) {
		m->scrolling = true;
		internalUpdateScroll();
	}
}

void ImageViewWidget::mouseMoveEvent(QMouseEvent *)
{
	QPoint pos = mapFromGlobal(QCursor::pos());
	if (m->d.mouse_pos == pos) return;
	m->d.mouse_pos = pos;

	setToolCursor(Qt::ArrowCursor);

	if (hasFocus()) {
		mainwindow()->onMouseMove(pos.x(), pos.y(), m->left_button);
	} else {
		mainwindow()->updateToolCursor();
	}

	m->d.cursor_anchor_pos = mapToCanvasFromViewport(pos);
	m->d.wheel_delta = 0;

	updateToolCursor();
}

void ImageViewWidget::setToolCursor(QCursor const &cursor)
{
	m->cursor = cursor;
}

void ImageViewWidget::updateToolCursor()
{
	setCursor(m->cursor);
}

void ImageViewWidget::mouseReleaseEvent(QMouseEvent *)
{
	QPoint pos = mapFromGlobal(QCursor::pos());
	if (m->left_button && hasFocus()) {
		mainwindow()->onMouseLeftButtonRelase(pos.x(), pos.y(), true);

		if (m->scrolling) {
			internalUpdateScroll();
			m->scrolling = false;
		}
	}
	m->left_button = false;
}

void ImageViewWidget::wheelEvent(QWheelEvent *e)
{
	double scale = 1;
	double d = e->angleDelta().y();
	double t = 1.001;
	scale *= pow(t, d);
	zoomToCursor(m->d.view_scale * scale);
	m->delayed_update_counter = 3;
}

void ImageViewWidget::onSelectionOutlineReady(const SelectionOutline &data)
{
	m->selection_outline = data;
	update();
}

void ImageViewWidget::onTimer()
{
	m->stripe_animation = (m->stripe_animation + 1) & 7;
	m->rectangle_animation = (m->rectangle_animation + 1) % 10;

	bool update = true;

	if (m->delayed_update_counter > 0) { // 遅延更新
		m->delayed_update_counter--;
		if (m->delayed_update_counter == 0) { // 更新する
			rescaleOffScreen(); // オフスクリーンを再構築
			m->render_canceled = true; // 現在の再描画要求をキャンセル
			if (1) {
				m->render_requested = true; // 再描画要求
			}
			update = false; // 再描画すると表示がガタつくので再描画しない。次のonTimerで再描画される
		}
	}

	if (update) {
		this->update();
	}
}


