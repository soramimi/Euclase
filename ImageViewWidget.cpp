
#include "ImageViewWidget.h"
#include "AlphaBlend.h"
#include "Canvas.h"
#include "MainWindow.h"
#include "PanelizedImage.h"
#include "SelectionOutline.h"
#include "misc.h"
#include "Bounds.h"
#include <QBitmap>
#include <QPainter>
#include <QPainterPath>
#include <QRegion>
#include <QSvgRenderer>
#include <QWheelEvent>
#include <atomic>
#include <cmath>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <utility>

const int MAX_SCALE = 32; // 32x
const int MIN_SCALE = 16; // 1/16x


class BoundsDrawer {
public:
	QPainter *painter_;
	CoordinateMapper mapper_;
	QColor color_;
	QPointF rect_start_;
	QPointF rect_end_;
	double animation_level_ = 0;


	void operator () (Bounds::Rectangle const &)
	{
	}

	void operator () (Bounds::Ellipse const &bounds)
	{
		painter_->save();
		painter_->setPen(QPen(color_, 0));
		painter_->setRenderHint(QPainter::Antialiasing);
		painter_->setTransform(mapper_.transformToViewportFromCanvas());
		painter_->drawEllipse(QRectF(rect_start_, rect_end_));
		painter_->restore();
	}

	BoundsDrawer(QPainter *painter, CoordinateMapper const &mapper, QPointF rect_start, QPointF rect_end, double animation_level)
		: painter_(painter)
		, mapper_(mapper)
		, rect_start_(rect_start)
		, rect_end_(rect_end)
		, animation_level_(animation_level)
	{
		int v = (sin(animation_level_) + 1) * 127;
		color_ = QColor(v, v, v);
	}

	void draw(Bounds::Type t)
	{
		QBrush brush = QBrush(color_);
		painter_->setOpacity(0.5); // 半透明

		// 枠線
		double x0 = rect_start_.x();
		double y0 = rect_start_.y();
		double x1 = rect_end_.x();
		double y1 = rect_end_.y();
		if (x0 > x1) std::swap(x0, x1);
		if (y0 > y1) std::swap(y0, y1);
		QPointF pt;
		pt = mapper_.mapToViewportFromCanvas(QPointF(x0, y0));
		x0 = floor(pt.x());
		y0 = floor(pt.y());
		pt = mapper_.mapToViewportFromCanvas(QPointF(x1, y1));
		x1 = floor(pt.x());
		y1 = floor(pt.y());
		misc::drawFrame(painter_, x0, y0, x1 - x0 + 1, y1 - y0 + 1, brush, brush);

		std::visit(*this, t);

		// ハンドル
		auto DrawHandle = [&](int x, int y){
			painter_->fillRect(x - 4, y - 4, 9, 9, brush);
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
};


namespace {

struct RenderView {
	CoordinateMapper mapper;
	QSize canvas_size;
	QPointF cursor_in_canvas;
};

struct RenderRequest {
	RenderView view;
	uint64_t generation = 0;
	bool pending = false;
	std::atomic_bool canceled{false};

	// Request state is protected by the view mutex. Workers only poll canceled without it.
	void schedule(const RenderView &snapshot, bool invalidate = true)
	{
		view = snapshot;
		pending = true;
		if (invalidate) {
			++generation;
			canceled.store(true, std::memory_order_relaxed);
		}
	}

	void discard()
	{
		pending = false;
		++generation;
		canceled.store(true, std::memory_order_relaxed);
	}

	uint64_t take(RenderView &snapshot)
	{
		snapshot = view;
		pending = false;
		canceled.store(false, std::memory_order_relaxed);
		return generation;
	}
};

struct RenderedTile {
	QPoint position;
	QImage image;
};

struct ImageRenderResult {
	uint64_t generation = 0;
	CoordinateMapper mapper;
	bool start_full_render = false;
	bool finish_full_render = false;
	std::vector<RenderedTile> tiles;
};

struct RenderCache {
	CoordinateMapper mapper;
	PanelizedImage image;
};

QRect visibleCanvas(const RenderView &view)
{
	const auto &mapper = view.mapper;
	const QSize viewport = mapper.viewport_size();
	const QPointF start = mapper.mapToCanvasFromViewport({});
	const QPointF end = mapper.mapToCanvasFromViewport(QPointF(viewport.width(), viewport.height()));
	const double width = std::max(0, view.canvas_size.width());
	const double height = std::max(0, view.canvas_size.height());
	const int x = int(std::floor(std::clamp(start.x(), 0.0, width)));
	const int y = int(std::floor(std::clamp(start.y(), 0.0, height)));
	const int right = int(std::ceil(std::clamp(end.x(), 0.0, width)));
	const int bottom = int(std::ceil(std::clamp(end.y(), 0.0, height)));
	return {x, y, right - x, bottom - y};
}

} // namespace

struct ImageViewWidget::Private {
	MainWindow *mainwindow = nullptr;
	QScrollBar *v_scroll_bar = nullptr;
	QScrollBar *h_scroll_bar = nullptr;
	QString mime_type;

	struct Data {
		QPointF view_scroll_offset;
		double view_scale = 1;
		QPointF scroll_starting_offset;
		QPoint mouse_pos;
		QPoint mouse_press_pos;
		int wheel_delta = 0;
		QPointF scale_anchor_pos; // 拡大縮小の時の中心座標 (in canvas coordinates)
		int top_margin = 1;
		int bottom_margin = 1;
	};
	Data d;

	bool left_button = false;

	bool scrolling = false;

	int stripe_animation = 0;
	int bounds_animation = 0;

	bool bounds_visible = false;
	QPointF bounds_start;
	QPointF bounds_end;

	QBrush horz_stripe_brush;
	QBrush vert_stripe_brush;

	QCursor cursor;

	QPixmap transparent_pixmap;

	std::mutex mutex;
	std::condition_variable cond;

	bool stopping = false;
	RenderRequest image_request;
	RenderRequest outline_request;
	QRegion dirty;
	bool redraw_entire_view = false;
	bool image_rendering_active = false;
	bool brush_dragging = false;
	ImageRenderResult image_result;
	bool image_update_pending = false;
	std::thread image_rendering_thread;
	std::thread selection_outline_thread;

	// The display cache and overlay belong to the GUI thread.
	RenderCache render_cache;
	RenderCache previous_render_cache;
	CoordinateMapper partial_preview_mapper;
	QImage partial_preview;
	SelectionOutline selection_outline;
};

ImageViewWidget::ImageViewWidget(QWidget *parent)
	: QWidget(parent)
	, m(new Private)
{
	setContextMenuPolicy(Qt::DefaultContextMenu);

	initBrushes();

	setMouseTracking(true);

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

void ImageViewWidget::init(MainWindow *mainwindow, QScrollBar *vsb, QScrollBar *hsb)
{
	m->mainwindow = mainwindow;
	m->v_scroll_bar = vsb;
	m->h_scroll_bar = hsb;
	startRenderingThread();
}

void ImageViewWidget::runSelectionRendering()
{
	auto &request = m->outline_request;
	while (true) {
		RenderView view;
		uint64_t generation;
		{
			std::unique_lock lock(m->mutex);
			m->cond.wait(lock, [&] { return request.pending || m->stopping; });
			if (m->stopping) return;
			generation = request.take(view);
		}
		QPoint position;
		auto outline = renderSelectionOutline(view.mapper, view.canvas_size, request.canceled, position);
		if (request.canceled.load(std::memory_order_relaxed)) continue;

		QMetaObject::invokeMethod(this, [this, generation, position, outline = std::move(outline)] {
			{
				std::lock_guard lock(m->mutex);
				if (m->stopping || generation != m->outline_request.generation) return;
			}
			m->selection_outline = {position, outline.isNull() ? QBitmap() : QBitmap::fromImage(outline)};
			update();
		}, Qt::QueuedConnection);
	}
}

CoordinateMapper ImageViewWidget::currentCoordinateMapper() const
{
	return CoordinateMapper(size(), m->d.view_scroll_offset, scale());
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
	{
		std::lock_guard lock(m->mutex);
		m->stopping = true;
		m->image_request.discard();
		m->outline_request.discard();
	}
	m->cond.notify_all();
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

void ImageViewWidget::showBounds(QPointF const &start, QPointF const &end)
{
	m->bounds_start = start;
	m->bounds_end = end;
	m->bounds_visible = true;
	update();
}

void ImageViewWidget::hideRect(bool update)
{
	m->bounds_visible = false;

	if (update) {
		this->update();
	}
}

bool ImageViewWidget::isRectVisible() const
{
	return m->bounds_visible;
}

/**
 * @brief ImageViewWidget::stripeBrush
 * @return ブラシ
 *
 * ストライプパターンのブラシを返す
 */
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

/**
 * @brief ImageViewWidget::canvasSize
 * @return 画像のサイズ
 *
 * キャンバスのサイズを返す
 */
QSize ImageViewWidget::canvasSize() const
{
	return m->mainwindow ? m->mainwindow->canvasSize() : QSize(0, 0);
}

/**
 * @brief ImageViewWidget::centerF
 *
 * ビューポート座標系での中心座標を返す
 */
QPointF ImageViewWidget::centerF() const
{
	return QPointF(width() / 2.0, height() / 2.0);
}

/**
 * @brief ImageViewWidget::imageScrollRange
 * @return スクロール範囲
 *
 * 画像のスクロール範囲を返す
 */
QSize ImageViewWidget::imageScrollRange() const
{
	QSize sz = canvasSize();
	int w = int(sz.width() * scale());
	int h = int(sz.height() * scale());
	return QSize(w, h);
}

void ImageViewWidget::clearRenderCache()
{
	{
		std::lock_guard lock(m->mutex);
		m->image_request.discard();
		m->outline_request.discard();
		m->image_result = {};
		m->redraw_entire_view = false;
		m->dirty = {};
	}
	m->render_cache.image.clear();
	m->previous_render_cache.image.clear();
	m->partial_preview = {};
	m->selection_outline = {};
	update();
}

void ImageViewWidget::requestRendering(const QRegion &canvas_region)
{
	scheduleRendering(canvas_region, false);
}

void ImageViewWidget::requestBrushRendering(const QRegion &canvas_region)
{
	scheduleRendering(canvas_region, true);
}

void ImageViewWidget::scheduleRendering(const QRegion &canvas_region, bool preserve_current_render)
{
	const auto mapper = currentCoordinateMapper();
	const RenderView snapshot{mapper, canvasSize(), mapper.mapToCanvasFromViewport(mapFromGlobal(QCursor::pos()))};
	{
		std::lock_guard lock(m->mutex);
		if (m->stopping) return;
		const auto &previous = m->image_request.view;
		bool full = canvas_region.isEmpty() || previous.canvas_size != snapshot.canvas_size
				|| previous.mapper.scale() != mapper.scale() || previous.mapper.scrollOffset() != mapper.scrollOffset()
				|| previous.mapper.viewport_size() != mapper.viewport_size();
		if (!preserve_current_render && (m->image_rendering_active || m->image_update_pending)) {
			// A canceled batch may contain dirty tiles that have not reached the cache.
			full = true;
		}
		if (full) {
			m->redraw_entire_view = true;
			m->dirty = {};
		} else if (!m->redraw_entire_view) {
			m->dirty += canvas_region;
		}
		m->image_request.schedule(snapshot, !preserve_current_render || full);
	}
	m->cond.notify_all();
}

void ImageViewWidget::scrollImage(double x, double y)
{
	const auto range = imageScrollRange();
	m->d.view_scroll_offset = {std::clamp(x, 0.0, double(range.width())),
							   std::clamp(y, 0.0, double(range.height()))};
	requestRendering({});
	requestUpdateSelectionOutline();

	// スクロールバーの位置を更新
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

	emit updateDocInfo();
}

/**
 * @brief ImageViewWidget::refrectScrollBar
 *
 * スクロールバーの値をスクロール位置に反映する
 */
void ImageViewWidget::refrectScrollBar()
{
	// スクロールバーの値をスクロール位置に反映する
	const double e = 0.75;
	double x = m->h_scroll_bar->value();
	double y = m->v_scroll_bar->value();
	if (fabs(x - m->d.view_scroll_offset.x()) < e) x = m->d.view_scroll_offset.x(); // 差が小さいときは値を維持する
	if (fabs(y - m->d.view_scroll_offset.y()) < e) y = m->d.view_scroll_offset.y();
	scrollImage(x, y);
}

/**
 * @brief ImageViewWidget::setScrollBarRange
 *
 * スクロールバーの範囲を設定する
 * @param h 水平スクロールバー
 * @param v 垂直スクロールバー
 */
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

/**
 * @brief ImageViewWidget::updateScrollBarRange
 *
 * スクロールバーの範囲を更新する
 */
void ImageViewWidget::updateScrollBarRange()
{
	if (!m->h_scroll_bar || !m->v_scroll_bar) return;
	setScrollBarRange(m->h_scroll_bar, m->v_scroll_bar);
}

/**
 * @brief ImageViewWidget::requestUpdateSelectionOutline
 *
 * 選択領域のアウトラインを更新要求する
 */
void ImageViewWidget::requestUpdateSelectionOutline()
{
	const RenderView snapshot{currentCoordinateMapper(), canvasSize(), {}};
	{
		std::lock_guard lock(m->mutex);
		if (m->stopping) return;
		m->outline_request.schedule(snapshot);
	}
	m->selection_outline = {};
	m->cond.notify_all();
	update();
}

/**
 * @brief ImageViewWidget::setScaleAnchorPos
 * @param pos 拡大縮小の基準座標
 *
 * 拡大縮小の基準座標を設定する
 */
void ImageViewWidget::setScaleAnchorPos(QPointF const &pos)
{
	m->d.scale_anchor_pos = pos;
}

/**
 * @brief ImageViewWidget::getScaleAnchorPos
 * @return 拡大縮小の基準座標
 *
 * 拡大縮小の基準座標を取得する
 */
QPointF ImageViewWidget::getScaleAnchorPos()
{
	return m->d.scale_anchor_pos;
}

/**
 * @brief ImageViewWidget::updateCursorAnchorPos
 *
 * ホイール拡大縮小の際の基準座標を更新する
 */
void ImageViewWidget::updateCursorAnchorPos()
{
	setScaleAnchorPos(mapToCanvasFromViewport(mapFromGlobal(QCursor::pos())));
}

/**
 * @brief ImageViewWidget::setScale
 * @param s 拡大率
 * @param fire_event イベントを発行する場合はtrue
 * @return 拡大率が変更された場合はtrue
 *
 * 拡大率を設定する
 */
bool ImageViewWidget::setScale(double s, bool fire_event)
{
	if (s < 1.0 / MIN_SCALE) s = 1.0 / MIN_SCALE;
	if (s > MAX_SCALE) s = MAX_SCALE;
	s = round(s * 1000000.0) / 1000000.0;
	if (scale() == s) return false;

	m->d.view_scale = s;
	updateScrollBarRange(); // スクロールバーの範囲を更新

	if (fire_event) {
		emit scaleChanged(scale());
	}

	return true;
}

/**
 * @brief ImageViewWidget::scale
 *
 * 現在の拡大率を取得する
 * @return 現在の拡大率
 */
double ImageViewWidget::scale() const
{
	return m->d.view_scale;
}

/**
 * @brief ImageViewWidget::zoomInternal
 * @param pos 拡大縮小の基準座標
 *
 * 拡大縮小の内部処理
 */
void ImageViewWidget::zoomInternal(QPointF const &pos)
{
	QPointF pt = getScaleAnchorPos() * scale() + centerF() - pos;
	scrollImage(pt.x(), pt.y());
	update();
}

/**
 * @brief ImageViewWidget::zoomToCursor
 *
 * カーソル位置を基準に拡大縮小する
 * @param scale 拡大率
 */
bool ImageViewWidget::zoomToCursor(double scale)
{
	if (!setScale(scale, true)) return false;

	QPoint pos = mapFromGlobal(QCursor::pos());
	zoomInternal({pos.x() + 0.5, pos.y() + 0.5});
	return true;
}

/**
 * @brief ImageViewWidget::zoomToCenter
 *
 * 中心を基準に拡大縮小する
 * @param scale 拡大率
 */
void ImageViewWidget::zoomToCenter(double scale)
{
	QPointF pos = centerF();

	setScaleAnchorPos(mapToCanvasFromViewport(centerF())); // 中心を基準に拡大縮小する

	setScale(scale, true);

	zoomInternal(pos);
}

/**
 * @brief ImageViewWidget::scaleTo
 *
 * ビューサイズに合わせて拡大率を設定する
 * @param ratio ビューサイズに対する拡大率（通常は1.0未満）
 */
void ImageViewWidget::scaleFit(double ratio)
{
	QSize sz = canvasSize();
	double w = sz.width();
	double h = sz.height();
	if (w > 0 && h > 0) {
		double sx = width() / w;
		double sy = height() / h;
		setScale(std::min(sx, sy) * ratio, true);
	}
	updateScrollBarRange(); // スクロールバーの範囲を更新

	scrollImage(w * scale() / 2.0, h * scale() / 2.0); // 中心にスクロール

	updateCursorAnchorPos(); // ホイールスクロールの基準座標を更新
}

/**
 * @brief ImageViewWidget::scale100
 *
 * 拡大率を100%に設定する
 */
void ImageViewWidget::scale100()
{
	zoomToCenter(1.0);
}

/**
 * @brief ImageViewWidget::scaleIn
 *
 * 拡大率を1段階大きくする
 */
void ImageViewWidget::zoomIn()
{
	zoomToCenter(scale() * 2);
}

/**
 * @brief ImageViewWidget::scaleOut
 *
 * 拡大率を1段階小さくする
 */
void ImageViewWidget::zoomOut()
{
	zoomToCenter(scale() / 2);
}

QImage ImageViewWidget::renderSelectionOutline(const CoordinateMapper &mapper, const QSize &canvas_size, const std::atomic_bool &canceled, QPoint &position)
{
	const QRect area = visibleCanvas({mapper, canvas_size, {}});
	if (area.isEmpty()) return {};
	const QPointF first = mapper.mapToViewportFromCanvas(area.topLeft());
	const QPointF last = mapper.mapToViewportFromCanvas(QPointF(area.x() + area.width(), area.y() + area.height()));
	position = {int(std::floor(first.x())), int(std::floor(first.y()))};
	const QSize output_size(int(std::ceil(last.x())) - position.x(), int(std::ceil(last.y())) - position.y());
	if (output_size.isEmpty()) return {};

	const auto mask = mainwindow()->renderSelection(area, &canceled);
	if (!mask || canceled.load(std::memory_order_relaxed)) return {};
	const auto selection = mask.qimage().scaled(output_size).convertToFormat(QImage::Format_Grayscale8);
	if (selection.isNull()) return {};

	QImage outline(output_size, QImage::Format_Grayscale8);
	if (outline.isNull()) return {};
	outline.fill(Qt::white);
	const int right = outline.width() - 1;
	const int bottom = outline.height() - 1;
	const bool at_left = area.left() == 0;
	const bool at_right = area.right() == canvas_size.width() - 1;
	const bool at_top = area.top() == 0;
	const bool at_bottom = area.bottom() == canvas_size.height() - 1;
	for (int y = 0; y <= bottom; y++) {
		if (canceled.load(std::memory_order_relaxed)) return {};
		const auto *row = selection.constScanLine(y);
		auto *dest = outline.scanLine(y);
		if ((at_top && y == 0) || (at_bottom && y == bottom)) {
			for (int x = 0; x <= right; x++) dest[x] = (row[x] & 0x80) ? 0 : 255;
			continue;
		}
		if (at_left && (row[0] & 0x80)) dest[0] = 0;
		if (at_right && (row[right] & 0x80)) dest[right] = 0;
		if (y == 0 || y == bottom) continue;
		const auto *above = selection.constScanLine(y - 1);
		const auto *below = selection.constScanLine(y + 1);
		for (int x = 1; x < right; x++) {
			const uint8_t edge = ~(above[x - 1] & above[x] & above[x + 1] & row[x - 1] & row[x + 1]
								   & below[x - 1] & below[x] & below[x + 1]) & row[x];
			dest[x] = (edge & 0x80) ? 0 : 255;
		}
	}
	return outline;
}

void ImageViewWidget::paintRenderCache(QPainter &painter, const CoordinateMapper &mapper, bool include_previous) const
{
	auto Draw = [&](const CoordinateMapper &source_mapper, const QPoint &position, const QImage &image) {
		const QPointF first = mapper.mapToViewportFromCanvas(source_mapper.mapToCanvasFromViewport(position));
		const QPointF end = QPointF(position) + QPointF(image.width(), image.height());
		const QPointF last = mapper.mapToViewportFromCanvas(source_mapper.mapToCanvasFromViewport(end));
		painter.drawImage(QRectF(first, last), image, image.rect());
	};
	auto drawPanels = [&](const RenderCache &cache) {
		for (const auto &panel : cache.image.panels_) {
			Draw(cache.mapper, panel.offset, panel.image);
		}
	};
	if (include_previous) {
		drawPanels(m->previous_render_cache);
	}
	if (!m->partial_preview.isNull()) {
		Draw(m->partial_preview_mapper, {}, m->partial_preview);
	}
	drawPanels(m->render_cache);
}

void ImageViewWidget::preserveRenderCache()
{
	if (m->render_cache.image.panels_.empty()) return;
	if (m->previous_render_cache.image.panels_.empty()) {
		m->previous_render_cache = std::move(m->render_cache);
		return;
	}

	// Keep panels from interrupted renders without accumulating a cache per zoom step.
	// The earlier view stays available underneath when zooming back out.
	const auto mapper = m->render_cache.mapper;
	QImage preview(mapper.viewport_size(), QImage::Format_RGBA8888);
	if (preview.isNull()) return;
	preview.fill(Qt::transparent);
	{
		QPainter painter(&preview);
		paintRenderCache(painter, mapper, false);
	}
	m->partial_preview = std::move(preview);
	m->partial_preview_mapper = mapper;
}

// Called with the view mutex held. One queued update can collect several completed panels.
void ImageViewWidget::queueImageUpdate()
{
	if (m->stopping || m->image_update_pending) return;
	m->image_update_pending = true;
	QMetaObject::invokeMethod(this, [this] {
		ImageRenderResult result;
		bool wake_worker;
		{
			std::lock_guard lock(m->mutex);
			m->image_update_pending = false;
			wake_worker = !m->image_rendering_active;
			auto &pending = m->image_result;
			// A brush drag may have started since this update was queued.
			if (!m->stopping && pending.generation == m->image_request.generation && (!m->brush_dragging || !m->image_rendering_active)) {
				result.mapper = pending.mapper;
				result.start_full_render = std::exchange(pending.start_full_render, false);
				result.finish_full_render = std::exchange(pending.finish_full_render, false);
				result.tiles.swap(pending.tiles);
			}
		}
		if (wake_worker) m->cond.notify_all();
		if (result.tiles.empty() && !result.start_full_render && !result.finish_full_render) return;
		
		bool repaint_all = result.start_full_render;
		if (result.start_full_render) {
			if (!result.finish_full_render) preserveRenderCache();
			m->render_cache.image.clear();
		}
		m->render_cache.mapper = result.mapper;
		QRegion dirty;
		for (const auto &tile : result.tiles) {
			m->render_cache.image.paintImage(tile.position, tile.image, tile.image.size(), {});
			if (!repaint_all) dirty += QRect(tile.position, tile.image.size());
		}
		if (result.finish_full_render) {
			repaint_all |= !m->previous_render_cache.image.panels_.empty() || !m->partial_preview.isNull();
			m->previous_render_cache.image.clear();
			m->partial_preview = {};
		}
		if (repaint_all) {
			update();
		} else {
			update(dirty);
		}
	}, Qt::QueuedConnection);
}

void ImageViewWidget::runImageRendering()
{
	auto &request = m->image_request;
	auto canceled = [&] { return request.canceled.load(std::memory_order_relaxed); };
	while (true) {
		RenderView view;
		QRegion dirty;
		bool full;
		uint64_t generation;
		{
			std::unique_lock lock(m->mutex);
			m->cond.wait(lock, [&] { return m->stopping || (request.pending && !m->image_update_pending); });
			if (m->stopping) return;
			generation = request.take(view);
			dirty.swap(m->dirty);
			full = m->redraw_entire_view;
			m->redraw_entire_view = false;
			m->image_rendering_active = true;
			m->image_result = {generation, view.mapper, full, false, {}};
		}

		const QRect visible = visibleCanvas(view);
		const auto &mapper = view.mapper;
		const auto viewport = mapper.viewport_size();
		const QPoint checker_origin = mapper.scrollOffset().toPoint() - QPoint(viewport.width() / 2, viewport.height() / 2);
		std::vector<QRect> tiles;
		if (!visible.isEmpty()) {
			const auto first_x = visible.left() / OFFSCREEN_PANEL_SIZE * OFFSCREEN_PANEL_SIZE;
			const auto first_y = visible.top() / OFFSCREEN_PANEL_SIZE * OFFSCREEN_PANEL_SIZE;
			for (int64_t y = first_y; y <= visible.bottom() && !canceled(); y += OFFSCREEN_PANEL_SIZE) {
				for (int64_t x = first_x; x <= visible.right(); x += OFFSCREEN_PANEL_SIZE) {
					const int w = int(std::min<int64_t>(OFFSCREEN_PANEL_SIZE, int64_t(visible.right()) + 1 - x));
					const int h = int(std::min<int64_t>(OFFSCREEN_PANEL_SIZE, int64_t(visible.bottom()) + 1 - y));
					const QRect tile(int(x), int(y), w, h);
					const auto area = tile.intersected(visible);
					if (full || dirty.intersects(area)) tiles.push_back(area);
				}
			}
		}
		if (canceled()) {
			std::lock_guard lock(m->mutex);
			m->image_rendering_active = false;
			continue;
		}
		auto distance_squared = [&](const QRect &tile) {
			const QRectF area(tile);
			const auto &cursor = view.cursor_in_canvas;
			const QPointF nearest(std::clamp(cursor.x(), area.left(), area.right()), std::clamp(cursor.y(), area.top(), area.bottom()));
			const QPointF delta = cursor - nearest;
			return QPointF::dotProduct(delta, delta);
		};
		std::stable_sort(tiles.begin(), tiles.end(), [&](const QRect &a, const QRect &b) {
			return distance_squared(a) < distance_squared(b);
		});

		std::atomic_bool failed{false};
		auto renderTile = [&](const QRect &tile) -> RenderedTile {
			if (canceled() || failed.load(std::memory_order_relaxed)) return {};
			const QPoint first = mapper.mapToViewportFromCanvas(tile.topLeft()).toPoint();
			const QPoint last = mapper.mapToViewportFromCanvas(QPointF(tile.x() + tile.width(), tile.y() + tile.height())).toPoint();
			const QSize output_size(last.x() - first.x(), last.y() - first.y());
			if (output_size.isEmpty()) return {};

			Canvas::RenderOption opt;
			opt.use_mask = true;
			const auto panel = m->mainwindow->renderToPanel(Canvas::AllLayers, euclase::Format_F32_RGBA, tile, {}, opt, &request.canceled);
			if (canceled()) return {};
			const QImage image = panel.image().toHost().qimage();
			QImage scaled = image.scaled(output_size).convertToFormat(QImage::Format_RGBA8888);
			if (scaled.isNull()) {
				failed.store(true, std::memory_order_relaxed);
				return {};
			}

			const QPoint phase = first + checker_origin;
			for (int y = 0; y < scaled.height() && !canceled(); y++) {
				auto *row = reinterpret_cast<euclase::UInt8RGBA *>(scaled.scanLine(y));
				for (int x = 0; x < scaled.width(); x++) {
					if (row[x].a < 255) {
						const uint8_t shade = (((phase.x() + x) ^ (phase.y() + y)) & 8) ? 255 : 192;
						row[x] = AlphaBlend::blend(euclase::UInt8RGBA(shade, shade, shade, 255), row[x]);
					}
				}
			}
			return {first, std::move(scaled)};
		};

		// Give the workers adjacent priorities and publish in order even when a later tile finishes first.
#pragma omp parallel for num_threads(2) schedule(static, 1) ordered
		for (std::ptrdiff_t i = 0; i < static_cast<std::ptrdiff_t>(tiles.size()); i++) {
			auto rendered = renderTile(tiles[i]);
#pragma omp ordered
			{
				if (!rendered.image.isNull()) {
					std::lock_guard lock(m->mutex);
					if (!canceled()) {
						m->image_result.tiles.push_back(std::move(rendered));
						if (!m->brush_dragging) queueImageUpdate();
					}
				}
			}
		}
		{
			std::lock_guard lock(m->mutex);
			m->image_rendering_active = false;
			if (m->stopping || generation != request.generation) continue;
			if (failed.load(std::memory_order_relaxed)) {
				// Keep unfinished work for the next request without spinning on a failed render.
				m->image_result.tiles.clear();
				m->image_result.start_full_render = false;
				if (full) {
					m->redraw_entire_view = true;
					m->dirty = {};
				} else {
					m->dirty += dirty;
				}
				continue;
			}
			m->image_result.finish_full_render = full;
			if (full || !m->image_result.tiles.empty()) queueImageUpdate();
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
	const int view_w = width();
	const int view_h = height();

	const auto canvas_size = canvasSize();
	const int doc_w = canvas_size.width();
	const int doc_h = canvas_size.height();

	const CoordinateMapper mapper = currentCoordinateMapper();

	QPainter pr_view(this);
	pr_view.fillRect(rect(), BGCOLOR);
	paintRenderCache(pr_view, mapper, true);

	if (doc_w > 0 && doc_h > 0 && scale() == MAX_SCALE && !m->scrolling) {
		const auto first = mapper.mapToCanvasFromViewport({});
		pr_view.save();
		pr_view.setOpacity(0.25);
		pr_view.setBrushOrigin(mapper.mapToViewportFromCanvas({}).toPoint());
		for (int x = int(std::floor(first.x())); ; x++) {
			const int z = int(std::floor(mapper.mapToViewportFromCanvas(QPointF(x, first.y())).x()));
			if (z >= view_w) break;
			pr_view.fillRect(z, 0, 1, view_h, m->horz_stripe_brush);
		}
		for (int y = int(std::floor(first.y())); ; y++) {
			const int z = int(std::floor(mapper.mapToViewportFromCanvas(QPointF(first.x(), y)).y()));
			if (z >= view_h) break;
			pr_view.fillRect(0, z, view_w, 1, m->vert_stripe_brush);
		}
		pr_view.restore();
	}

	const auto &outline = m->selection_outline;
	if (!outline.bitmap.isNull()) {
		pr_view.save();
		pr_view.setClipRegion(QRegion(outline.bitmap).translated(outline.point));
		pr_view.setOpacity(0.5);
		pr_view.fillRect(rect(), stripeBrush());
		pr_view.restore();
	}

	{
		QPointF pt0(0, 0);
		QPointF pt1(doc_w, doc_h);
		pt0 = mapper.mapToViewportFromCanvas(pt0);
		pt1 = mapper.mapToViewportFromCanvas(pt1);
		int visible_x = (int)round(pt0.x());
		int visible_y = (int)round(pt0.y());
		int visible_w = (int)round(pt1.x()) - visible_x;
		int visible_h = (int)round(pt1.y()) - visible_y;
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
			pr_view.fillRect(rect(), BGCOLOR);
			pr_view.restore();
			// 枠線
			pr_view.drawRect(visible_x, visible_y, visible_w, visible_h);
		} else {
			// 背景
			pr_view.fillRect(rect(), BGCOLOR);
		}
	}

	// 範囲指定矩形点滅
	if (m->bounds_visible) {
		double f = m->bounds_animation * (2 * 3.1416) / 10.0;
		BoundsDrawer drawer(&pr_view, mapper, m->bounds_start, m->bounds_end, f);
		drawer.draw(mainwindow()->boundsType());
	}
}



void ImageViewWidget::setToolCursor(QCursor const &cursor)
{
	m->cursor = cursor;
}

void ImageViewWidget::updateToolCursor()
{
	setCursor(m->cursor);
}

void ImageViewWidget::internalUpdateScroll()
{
	QPoint pos = m->d.mouse_pos;
	int delta_x = pos.x() - m->d.mouse_press_pos.x();
	int delta_y = pos.y() - m->d.mouse_press_pos.y();
	scrollImage(m->d.scroll_starting_offset.x() - delta_x, m->d.scroll_starting_offset.y() - delta_y);
}

void ImageViewWidget::doHandScroll()
{
	if (m->left_button) {
		m->scrolling = true;
		internalUpdateScroll();
	}
}

void ImageViewWidget::resizeEvent(QResizeEvent *)
{
	updateScrollBarRange();
	requestUpdateSelectionOutline();
	requestRendering({});
}

bool ImageViewWidget::event(QEvent *event)
{
	if (event->type() == QEvent::UngrabMouse) endBrushDrag();
	return QWidget::event(event);
}

void ImageViewWidget::mousePressEvent(QMouseEvent *e)
{
	m->left_button = (e->buttons() & Qt::LeftButton);
	if (e->button() == Qt::LeftButton) {
		const bool brush = std::holds_alternative<tool::BrushTool>(mainwindow()->currentTool());
		std::lock_guard lock(m->mutex);
		m->brush_dragging = brush;
	}
	if (m->left_button) {
		QPoint pos = mapFromGlobal(QCursor::pos());
		m->d.mouse_press_pos = pos;
		m->d.scroll_starting_offset = m->d.view_scroll_offset;
		mainwindow()->onMouseLeftButtonPress(pos.x(), pos.y());
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

	updateCursorAnchorPos();
	m->d.wheel_delta = 0;

	updateToolCursor();
}

void ImageViewWidget::endBrushDrag()
{
	std::lock_guard lock(m->mutex);
	if (!m->brush_dragging) return;
	m->brush_dragging = false;
	if (!m->image_result.tiles.empty()) queueImageUpdate();
}

void ImageViewWidget::mouseReleaseEvent(QMouseEvent *e)
{
	if (e->button() == Qt::LeftButton) endBrushDrag();
	QPoint pos = mapFromGlobal(QCursor::pos());
	if (m->left_button && hasFocus()) {
		mainwindow()->onMouseLeftButtonRelease(pos.x(), pos.y(), true);

		if (m->scrolling) {
			internalUpdateScroll();
			m->scrolling = false;
		}
	}
	m->left_button = false;
}

void ImageViewWidget::wheelEvent(QWheelEvent *e)
{
	zoomToCursor(scale() * pow(1.001, e->angleDelta().y()));
}

void ImageViewWidget::onTimer()
{
	m->stripe_animation = (m->stripe_animation + 1) & 7;
	m->bounds_animation = (m->bounds_animation + 1) % 10;
	update();
}
