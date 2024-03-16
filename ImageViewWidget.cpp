
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
#include <mutex>
#include <thread>
#include <condition_variable>

const int MAX_SCALE = 32; // 32x
const int MIN_SCALE = 16; // 1/16x

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

	std::mutex mutex;
	std::condition_variable cond;

	std::thread image_rendering_thread;
	bool render_interrupted = false;
	bool render_invalidate = false;
	bool render_requested = false;
	bool render_canceled = false;
	std::vector<QRect> render_canvas_rects; // in canvas coordinates
	std::vector<QRect> render_canvas_adding_rects;
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

std::mutex &ImageViewWidget::mutexForOffscreen()
{
	return m->mutex;
}

void ImageViewWidget::init(MainWindow *mainwindow, QScrollBar *vsb, QScrollBar *hsb)
{
	m->mainwindow = mainwindow;
	m->v_scroll_bar = vsb;
	m->h_scroll_bar = hsb;
}

static QImage scale_float_to_uint8_rgba(euclase::Image const &src, int w, int h)
{
	if (src.memtype() == euclase::Image::CUDA) {
		euclase::Image tmp(w, h, euclase::Image::Format_8_RGBA, euclase::Image::CUDA);
		global->cuda->scale_float_to_uint8_rgba(w, h, w, tmp.data(), src.width(), src.height(), src.data());
		return tmp.qimage();
	} else {
		QImage qimg = src.qimage();
		if (qimg.isNull()) return {};
		return qimg.scaled(w, h, Qt::IgnoreAspectRatio, Qt::FastTransformation);
	}
}

void ImageViewWidget::runSelectionRendering()
{
	while (1) {
		bool req = false;
		{
			std::unique_lock lock(m->mutex);
			if (m->render_interrupted) break;
			m->cond.wait(lock, [&](){ return m->selection_outline_requested || m->render_interrupted; });
			req = m->selection_outline_requested;
		}
		if (req) {
			m->selection_outline_requested = false;
			SelectionOutline selection_outline = renderSelectionOutline(&m->selection_outline_requested);
			emit notifySelectionOutlineReady(selection_outline);
		}
	}
}

CoordinateMapper ImageViewWidget::currentCoordinateMapper() const
{
	return CoordinateMapper(size(), m->d.view_scroll_offset, scale());
}

CoordinateMapper ImageViewWidget::offscreenCoordinateMapper() const
{
	return m->offscreen1_mapper;
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
		std::lock_guard lock(mutexForOffscreen());
		m->composed_panels_cache.clear();
		m->render_canvas_rects.clear();
		m->render_canvas_adding_rects.clear();
		m->render_interrupted = true;
		m->render_canceled = true;
		m->cond.notify_all();
	}
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
	return canvas()->size();
}

/**
 * @brief ImageViewWidget::center
 *
 * ビューポート座標系での中心座標を返す
 */
QPoint ImageViewWidget::center() const
{
	return QPoint(width() / 2, height() / 2);
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

/**
 * @brief ImageViewWidget::clearRenderCache
 *
 * レンダリングキャッシュをクリアする
 */
void ImageViewWidget::clearRenderCache(bool clear_offscreen, bool lock)
{
	if (lock) {
		std::lock_guard lock(mutexForOffscreen());
		clearRenderCache(clear_offscreen, false);
		return;
	}

	m->offscreen1_mapper = currentCoordinateMapper();
	m->composed_panels_cache.clear();
	m->render_canvas_rects.clear();
	m->render_canvas_adding_rects.clear();
	m->render_requested = true;
	m->render_canceled = true;

	if (clear_offscreen) {
		m->offscreen1.clear();
		m->offscreen2 = {};
	}

	m->cond.notify_all();
}

/**
 * @brief ImageViewWidget::requestUpdateCanvas
 * @param canvasrect キャンバス座標系での更新領域
 * @param lock 排他処理を行う場合はtrue
 *
 * キャンバス座標系での更新領域を追加する
 */
void ImageViewWidget::requestUpdateCanvas(const QRect &canvasrect, bool lock)
{
	if (lock) {
		std::lock_guard lock(mutexForOffscreen());
		requestUpdateCanvas(canvasrect, false);
		return;
	}
	m->render_canvas_adding_rects.push_back(canvasrect);
}

/**
 * @brief ImageViewWidget::requestUpdateView
 * @param viewrect ビューポート座標系での更新領域
 * @param lock 排他処理を行う場合はtrue
 *
 * ビューポート座標系での更新領域を追加する
 */
void ImageViewWidget::requestUpdateView(const QRect &viewrect, bool lock)
{
	QPointF topleft = viewrect.topLeft();
	QPointF bottomright = viewrect.bottomRight();
	topleft = mapToCanvasFromViewport(topleft);
	bottomright = mapToCanvasFromViewport(bottomright);
	int x0 = (int)floor(topleft.x());
	int y0 = (int)floor(topleft.y());
	int x1 = (int)ceil(bottomright.x());
	int y1 = (int)ceil(bottomright.y());
	requestUpdateCanvas(QRect(x0, y0, x1 - x0, y1 - y0), lock);
}

/**
 * @brief ImageViewWidget::requestUpdateEntire
 * @param lock 排他処理を行う場合はtrue
 *
 * 画面全体の更新を予約する
 */
void ImageViewWidget::requestUpdateEntire(bool lock)
{
	if (lock) {
		std::lock_guard lock(mutexForOffscreen());
		requestUpdateEntire(false);
		return;
	}
	clearRenderCache(false, false);
	requestUpdateView({0, 0, width(), height()}, false);
}

/**
 * @brief ImageViewWidget::requestRendering
 * @param canvasrects キャンバス座標系での更新領域
 *
 * キャンバス座標系で更新要求する
 */
void ImageViewWidget::requestRendering(const QRect &canvasrect)
{
	std::lock_guard lock(mutexForOffscreen());
	m->offscreen1_mapper = currentCoordinateMapper();
	if (canvasrect.isEmpty()) {
		requestUpdateEntire(false);
	} else {
		requestUpdateCanvas(canvasrect, false);
	}
	m->render_requested = true;
	m->cond.notify_all();
}

/**
 * @brief ImageViewWidget::internalScrollImage
 * @param x スクロール先のx座標
 * @param y スクロール先のy座標
 * @param differential_update 差分更新する場合はtrue
 *
 * 画面をスクロールする
 */
void ImageViewWidget::internalScrollImage(double x, double y, bool differential_update)
{
	auto old_offset = m->d.view_scroll_offset;

	QSizeF sz = imageScrollRange();
	x = std::min(std::max(x, 0.0), sz.width());
	y = std::min(std::max(y, 0.0), sz.height());
	m->d.view_scroll_offset = QPointF(x, y);

	if (differential_update) { // 差分更新
		if (m->d.view_scroll_offset != old_offset) { // スクロールした
			int delta_x = (int)ceil(old_offset.x() - x); // 右にスクロールするとdelta_xは正
			int delta_y = (int)ceil(old_offset.y() - y); // 下にスクロールするとdelta_yは正
			{
				std::lock_guard lock(mutexForOffscreen());
				m->offscreen1_mapper = currentCoordinateMapper();
				m->render_canceled = true;

				if (delta_x > 0) { // 右にスクロール、左に空白ができる
					requestUpdateView({0, 0, delta_x, height()}, false);
				} else if (delta_x < 0) { // 左にスクロール、右に空白ができる
					requestUpdateView({width() + delta_x, 0, -delta_x, height()}, false);
				}
				if (delta_y > 0) { // 下にスクロール、上に空白ができる
					requestUpdateView({0, 0, width(), delta_y}, false);
				} else if (delta_y < 0) { // 上にスクロール、下に空白ができる
					requestUpdateView({0, height() + delta_y, width(), -delta_y}, false);
				}

				m->render_requested = true;
				m->cond.notify_all();
			}
			update();
		}
	}
}

/**
 * @brief 指定した位置にスクロールする
 * @param x スクロール先のx座標
 * @param y スクロール先のy座標
 * @param differential_update 差分更新するかどうか
 */
void ImageViewWidget::scrollImage(double x, double y, bool differential_update)
{
	internalScrollImage(x, y, differential_update);

	// 選択領域のアウトラインを更新
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
	internalScrollImage(x, y, true);
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
	setScrollBarRange(m->h_scroll_bar, m->v_scroll_bar);
}

/**
 * @brief ImageViewWidget::requestUpdateSelectionOutline
 *
 * 選択領域のアウトラインを更新要求する
 */
void ImageViewWidget::requestUpdateSelectionOutline()
{
	m->selection_outline_requested = true;
	m->cond.notify_all();
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
	scrollImage(pt.x(), pt.y(), false);
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

	scrollImage(w * scale() / 2.0, h * scale() / 2.0, false); // 中心にスクロール

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

/**
 * @brief ImageViewWidget::generateSelectionOutlineImage
 * @return アウトライン画像
 *
 * 選択領域のアウトライン画像を生成する
 */
QImage ImageViewWidget::generateSelectionOutlineImage(QImage const &selection, bool *abort)
{
	Q_ASSERT(selection.format() == QImage::Format_Grayscale8);

	QImage image;
	int w = selection.width();
	int h = selection.height();

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
		QImage selection;
		{
			int dx = int(dp0.x());
			int dy = int(dp0.y());
			int dw = int(dp1.x()) - dx;
			int dh = int(dp1.y()) - dy;
			euclase::Image sel;
			{
				std::lock_guard lock(mainwindow()->mutexForCanvas());
				sel = canvas()->renderSelection(QRect(dx, dy, dw, dh), abort).image(); // 選択領域をレンダリング
			}
			if (abort && *abort) return {};
			// 選択領域をスケーリング
			if (sel.memtype() == euclase::Image::CUDA) { // CUDAメモリの場合はスケーリングをCUDAで行う
				euclase::Image tmp(vw, vh, euclase::Image::Format_8_Grayscale, euclase::Image::CUDA);
				int sw = sel.width();
				int sh = sel.height();
				global->cuda->scale(vw, vh, vw, tmp.data(), sw, sh, sw, sel.data(), 1);
				selection = tmp.qimage();
			} else { // それ以外はホストメモリで行う
				selection = sel.qimage().scaled(vw, vh);
			}
		}
		QImage outline = generateSelectionOutlineImage(selection, abort); // アウトライン画像を生成
		if (outline.isNull()) return {};
		data.bitmap = QBitmap::fromImage(outline);
		data.point = QPoint(vx, vy);
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
		bool requested = false;
		{
			std::unique_lock lock(mutexForOffscreen());
			if (m->render_interrupted) return;
			m->cond.wait(lock, [&](){ return m->render_requested || m->render_interrupted; });
			m->render_canceled = false;
			if (m->render_requested) {
				m->render_requested = false;
				requested = true;
			}
		}
		if (requested) {
			Q_ASSERT(m->offscreen1.offset().x() == 0); // オフスクリーンの原点は常に(0, 0)
			Q_ASSERT(m->offscreen1.offset().y() == 0);

			CoordinateMapper mapper; // オフスクリーン座標系
			{
				std::lock_guard lock(mutexForOffscreen());
				mapper = m->offscreen1_mapper;
				if (m->render_invalidate) {
					m->render_invalidate = false;
					m->offscreen1.clear();
				}
				std::vector<QRect> v;
				std::swap(v, m->render_canvas_adding_rects);
				m->render_canvas_rects.insert(m->render_canvas_rects.end(), v.begin(), v.end());
			}

			const int canvas_w = mainwindow()->canvasWidth();
			const int canvas_h = mainwindow()->canvasHeight();

			const QPointF view_topleft = mapper.mapToViewportFromCanvas(QPointF(0, 0));
			const QPointF view_bottomright = mapper.mapToViewportFromCanvas(QPointF(canvas_w, canvas_h));
			const int view_left = std::max((int)floor(view_topleft.x()), 0);
			const int view_top = std::max((int)floor(view_topleft.y()), 0);
			const int view_right = std::min((int)ceil(view_bottomright.x() + 1), width()); // ceilだけでは足りなことがあるので 1 足す
			const int view_bottom = std::min((int)ceil(view_bottomright.y() + 1), height());

			std::vector<QRect> target_rects;

			{ // レンダリングする領域を決定
				std::lock_guard lock(mutexForOffscreen());
				QPointF topleft = mapper.mapToCanvasFromViewport(QPointF(view_left, view_top));
				QPointF bottomright = mapper.mapToCanvasFromViewport(QPointF(view_right, view_bottom));
				const int S1 = OFFSCREEN_PANEL_SIZE - 1;
				int x0 = (int)topleft.x() & ~S1; // パネルサイズ単位に切り下げ
				int y0 = (int)topleft.y() & ~S1;
				int x1 = (int)bottomright.x() & ~S1;
				int y1 = (int)bottomright.y() & ~S1;
				for (int y = y0; y <= y1; y += OFFSCREEN_PANEL_SIZE) {
					for (int x = x0; x <= x1; x += OFFSCREEN_PANEL_SIZE) {
						QRect target_rect(x, y, OFFSCREEN_PANEL_SIZE, OFFSCREEN_PANEL_SIZE); // 描画する候補の矩形
						for (auto const &requested_rect : m->render_canvas_rects) { // 描画要求された矩形と重なっているかどうか
							if (target_rect.intersects(requested_rect)) {
								target_rects.push_back(target_rect);
								break;
							}
						}
					}
				}
			}

			if (1) { // マウスカーソルから近い順にソート
				QPoint center = mapper.mapToCanvasFromViewport(mapFromGlobal(QCursor::pos())).toPoint();
				std::sort(target_rects.begin(), target_rects.end(), [&](QRect const &a, QRect const &b){
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

			auto canceled = [&](){
				bool f = /*m->render_requested ||*/ m->render_canceled || /*m->render_invalidate ||*/ m->render_interrupted;
				return f;
			};

			size_t panelindex = 0;
			std::atomic_int rectindex = 0;

#pragma omp parallel for num_threads(8)
			for (int i = 0; i < target_rects.size(); i++) {
				if (canceled()) continue;

				// パネル矩形
				QRect const &rect = target_rects[rectindex++];
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
				if (dw < 1 || dh < 1) continue;

				// 描画元座標を確定
				int sx = (int)src_topleft.x();
				int sy = (int)src_topleft.y();
				int sw = (int)src_bottomright.x() - sx;
				int sh = (int)src_bottomright.y() - sy;
				if (sw < 1 || sh < 1) continue;

				// パネル原点を引く
				sx -= x;
				sy -= y;

				euclase::Image image;
				Canvas::Panel newpanel;
				Canvas::RenderOption opt;
				opt.use_mask = true;
				newpanel = m->mainwindow->renderToPanel(Canvas::AllLayers, euclase::Image::Format_F_RGBA, rect, {}, opt, (bool *)&m->render_interrupted);
				if (canceled()) continue;

				newpanel = newpanel->toHost();
				if (canceled()) continue;

				newpanel.setOffset(x, y);
				{
					// パネルキャッシュから検索
					std::lock_guard lock(mutexForOffscreen());
					Canvas::Panel *panel = nullptr;
					QPoint pos(x, y);
					for (size_t j = panelindex; j < m->composed_panels_cache.size(); j++) {
						if (m->composed_panels_cache[j].offset() == pos) {
							std::swap(m->composed_panels_cache[panelindex], m->composed_panels_cache[j]);
							panel = &m->composed_panels_cache[panelindex];
							*panel->imagep() = newpanel.image();
							panelindex++;
							break;
						}
					}
					if (!panel) { // 見つからなかったら、新規パネルを作成
						if (panelindex <= m->composed_panels_cache.size()) { // 別スレッドがcomposed_panels_cacheをclearすることがあるので、範囲チェック
							m->composed_panels_cache.insert(m->composed_panels_cache.begin() + panelindex, newpanel);
							panel = &m->composed_panels_cache[panelindex];
						}
						panelindex++;
					}
				}

				image = cropImage(newpanel.image(), sx, sy, sw, sh); // 画像を切り出す
				if (canceled()) continue;

				// 拡大縮小
				QImage qimg = scale_float_to_uint8_rgba(image, dw, dh);
				if (qimg.isNull()) continue;

				if (canceled()) continue;

				QPoint dpos = QPoint(dx, dy) - center() + m->offscreen1_mapper.scrollOffset().toPoint();

				// 透明部分の市松模様
				for (int iy = 0; iy < qimg.height(); iy++) {
					euclase::OctetRGBA *p = (euclase::OctetRGBA *)qimg.scanLine(iy);
					for (int ix = 0; ix < qimg.width(); ix++) {
						if (p->a < 255) { // 透明部分
							int u = dpos.x() + ix; // 市松模様の座標がずれないように、オフスクリーン系の座標の原点を足す
							int v = dpos.y() + iy;
							uint8_t a = ((u ^ v) & 8) ? 255 : 192; // 市松模様パターン
							euclase::OctetRGBA bg(a, a, a, 255); // 市松模様の背景
							*p = AlphaBlend::blend(bg, *p); // 背景に合成
						}
						p++;
					}
				}

				if (canceled()) continue;

				// オフスクリーンへ描画する
				{
					std::lock_guard lock(mutexForOffscreen());
					if (!canceled()) {
						m->offscreen1.paintImage(dpos, qimg, qimg.size(), {});
					}
				}
			}
			{
				std::lock_guard lock(mutexForOffscreen());

				if (!canceled()) {
					m->render_canvas_rects.clear(); // 描画済みのパネル矩形をクリア
					update();
				}

				// 多すぎるパネルを削除
				size_t n = panelindex * 2;
				if (m->composed_panels_cache.size() > n) {
					m->composed_panels_cache.resize(n);
				}
			}
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

	const int doc_w = canvas()->width();
	const int doc_h = canvas()->height();

	const CoordinateMapper mapper = currentCoordinateMapper();

	// オーバーレイ用オフスクリーンの描画は別スレッドで実行
	std::thread th;
	if (doc_w > 0 && doc_h > 0) {
		th = std::thread([&](){
			Q_ASSERT(doc_w > 0);
			Q_ASSERT(doc_h > 0);
			if (m->offscreen2.width() != view_w || m->offscreen2.height() != view_h) {
				m->offscreen2 = QPixmap(view_w, view_h);
			}
			m->offscreen2.fill(Qt::transparent);
			{
				QPainter pr2(&m->offscreen2);

				// 最大拡大時のグリッド
				if (scale() == MAX_SCALE && !m->scrolling) { // スクロール中はグリッドを描画しない
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
	}

	// ビューポートの描画
	QPainter pr_view(this);

	// 画像のオフスクリーンを描画
	Q_ASSERT(m->offscreen1.offset().x() == 0); // オフスクリーンの原点は常に(0, 0)
	Q_ASSERT(m->offscreen1.offset().y() == 0);
	{
		std::lock_guard lock(mutexForOffscreen());
		auto osmapper = offscreenCoordinateMapper();
		for (PanelizedImage::Panel const &panel : m->offscreen1.panels_) {
			QPoint org = panel.offset - m->offscreen1_mapper.scrollOffset().toPoint() + center();
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

	if (th.joinable()) {
		th.join();
	}

	// オーバーレイを描画
	pr_view.drawPixmap(0, 0, m->offscreen2);

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
	std::lock_guard lock(mutexForOffscreen());

	Q_ASSERT(m->offscreen1.offset().x() == 0); // オフスクリーンの原点は常に(0, 0)
	Q_ASSERT(m->offscreen1.offset().y() == 0);

	auto old_mapper = m->offscreen1_mapper; // 古い座標系
	auto new_mapper = currentCoordinateMapper(); // 新しい座標系

	QPointF old_org(centerF() - old_mapper.scrollOffset()); // 古い座標系の原点
	QPointF new_org(centerF() - new_mapper.scrollOffset()); // 新しい座標系の原点

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

void ImageViewWidget::setToolCursor(QCursor const &cursor)
{
	m->cursor = cursor;
}

void ImageViewWidget::updateToolCursor()
{
	setCursor(m->cursor);
}

void ImageViewWidget::cancelRendering()
{
	std::lock_guard lock(mutexForOffscreen());
	m->render_canceled = true;
	m->cond.notify_all();
}

void ImageViewWidget::internalUpdateScroll()
{
	QPoint pos = m->d.mouse_pos;
	int delta_x = pos.x() - m->d.mouse_press_pos.x();
	int delta_y = pos.y() - m->d.mouse_press_pos.y();
	scrollImage(m->d.scroll_starting_offset.x() - delta_x, m->d.scroll_starting_offset.y() - delta_y, true);
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
	requestUpdateEntire(false);
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
	double d = e->angleDelta().y(); // ホイールの回転量
	double s = scale() * pow(1.001, d); // 拡大率
	if (zoomToCursor(s)) {
		m->delayed_update_counter = 3; // 300ms後に更新する
	}
}

void ImageViewWidget::onSelectionOutlineReady(const SelectionOutline &data)
{
	m->selection_outline = data;
	update();
}

/**
 * @brief ImageViewWidget::onTimer
 *
 * タイマーイベントの処理
 */
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

			requestUpdateEntire(true);
			m->render_requested = true;
			m->cond.notify_all();

			update = false; // 再描画すると表示がガタつくので再描画しない。次のonTimerで再描画される
		}
	}

	if (update) {
		this->update();
	}
}

