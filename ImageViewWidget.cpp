
#include "ImageViewWidget.h"
#include "Document.h"
#include "ImageViewRenderer.h"
#include "ImageViewWidget.h"
#include "MainWindow.h"
#include "MemoryReader.h"
#include "Photoshop.h"
#include "SelectionOutlineRenderer.h"
#include "charvec.h"
#include "joinpath.h"
#include "misc.h"
#include <QBitmap>
#include <QBuffer>
#include <QDebug>
#include <QFileDialog>
#include <QMenu>
#include <QPainter>
#include <QSvgRenderer>
#include <QWheelEvent>
#include <cmath>
#include <functional>
#include <memory>
#include "TransparentCheckerBrush.h"

using SvgRendererPtr = std::shared_ptr<QSvgRenderer>;

const int MAX_SCALE = 32;
const int MIN_SCALE = 8;

struct ImageViewWidget::Private {
	MainWindow *mainwindow = nullptr;
	QScrollBar *v_scroll_bar = nullptr;
	QScrollBar *h_scroll_bar = nullptr;
	QString mime_type;
	QMutex sync;

	double image_scroll_x = 0;
	double image_scroll_y = 0;
	double image_scale = 1;
	double scroll_origin_x = 0;
	double scroll_origin_y = 0;
	QPoint mouse_pos;
	QPoint mouse_press_pos;
	int wheel_delta = 0;
	QPointF cursor_anchor_pos;
	QPointF center_anchor_pos;
	int top_margin = 1;
	int bottom_margin = 1;

	bool left_button = false;

	ImageViewRenderer *renderer = nullptr;
	RenderedImage rendered_image;
	QRect destination_rect;

	SelectionOutlineRenderer *outline_renderer = nullptr;

	QPixmap transparent_pixmap;

	int stripe_animation = 0;

	bool rect_visible = false;
	QPointF rect_start;
	QPointF rect_end;

	SelectionOutlineBitmap selection_outline;

	QBrush horz_stripe_brush;
	QBrush vert_stripe_brush;

	QCursor cursor;
};

ImageViewWidget::ImageViewWidget(QWidget *parent)
	: QWidget(parent)
	, m(new Private)
{
	setContextMenuPolicy(Qt::DefaultContextMenu);

	m->renderer = new ImageViewRenderer(this);
	connect(m->renderer, &ImageViewRenderer::done, this, &ImageViewWidget::onRenderingCompleted);

	m->outline_renderer = new SelectionOutlineRenderer(this);
	connect(m->outline_renderer, &SelectionOutlineRenderer::done, this, &ImageViewWidget::onSelectionOutlineRenderingCompleted);

	initBrushes();

	setMouseTracking(true);

	startTimer(100);
}

ImageViewWidget::~ImageViewWidget()
{
	stopRendering(true);
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

Document *ImageViewWidget::document()
{
	return m->mainwindow->document();
}

Document const *ImageViewWidget::document() const
{
	return m->mainwindow->document();
}

void ImageViewWidget::bind(MainWindow *mainwindow, QScrollBar *vsb, QScrollBar *hsb)
{
	m->mainwindow = mainwindow;
	m->v_scroll_bar = vsb;
	m->h_scroll_bar = hsb;
}

void ImageViewWidget::stopRendering(bool wait)
{
	m->renderer->abort(wait);
	m->rendered_image = {};
	m->destination_rect = {};
}

QPointF ImageViewWidget::mapFromViewportToDocument(QPointF const &pos)
{
	double cx = width() / 2.0;
	double cy = height() / 2.0;
	double x = (pos.x() - cx + m->image_scroll_x) / m->image_scale;
	double y = (pos.y() - cy + m->image_scroll_y) / m->image_scale;
	return QPointF(x, y);
}

QPointF ImageViewWidget::mapFromDocumentToViewport(QPointF const &pos)
{
	double cx = width() / 2.0;
	double cy = height() / 2.0;
	double x = pos.x() * m->image_scale + cx - m->image_scroll_x;
	double y = pos.y() * m->image_scale + cy - m->image_scroll_y;
	return QPointF(x, y);
}

QMutex *ImageViewWidget::synchronizer()
{
	return &m->sync;
}

void ImageViewWidget::showRect(QPointF const &start, QPointF const &end)
{
	m->rect_start = start;
	m->rect_end = end;
	m->rect_visible = true;
	update();
}

void ImageViewWidget::hideRect()
{
	m->rect_visible = false;
	update();
}

bool ImageViewWidget::isRectVisible() const
{
	return m->rect_visible;
}

QBrush ImageViewWidget::stripeBrush(bool blink)
{
	int mask = blink ? 2 : 4;
	int a = m->stripe_animation;
	QImage image(8, 8, QImage::Format_Indexed8);
	image.setColor(0, qRgb(0, 0, 0));
	image.setColor(1, qRgb(255, 255, 255));
	if (blink) {
		uint8_t v = (a & 4) ? 1: 0;
		for (int y = 0; y < 8; y++) {
			uint8_t *p = image.scanLine(y);
			for (int x = 0; x < 8; x++) {
				p[x] = v;
			}
		}
	} else {
		for (int y = 0; y < 8; y++) {
			uint8_t *p = image.scanLine(y);
			for (int x = 0; x < 8; x++) {
				p[x] = ((a - x - y) & mask) ? 1 : 0;
			}
		}
	}
	return QBrush(image);
}

void ImageViewWidget::internalScrollImage(double x, double y, bool updateview)
{
	m->image_scroll_x = x;
	m->image_scroll_y = y;
	QSizeF sz = imageScrollRange();
	if (m->image_scroll_x < 0) m->image_scroll_x = 0;
	if (m->image_scroll_y < 0) m->image_scroll_y = 0;
	if (m->image_scroll_x > sz.width()) m->image_scroll_x = sz.width();
	if (m->image_scroll_y > sz.height()) m->image_scroll_y = sz.height();

	if (updateview) {
		paintViewLater(true, true);
	}
}

void ImageViewWidget::scrollImage(double x, double y, bool updateview)
{
	internalScrollImage(x, y, updateview);

	if (m->h_scroll_bar) {
		auto b = m->h_scroll_bar->blockSignals(true);
		m->h_scroll_bar->setValue((int)m->image_scroll_x);
		m->h_scroll_bar->blockSignals(b);
	}
	if (m->v_scroll_bar) {
		auto b = m->v_scroll_bar->blockSignals(true);
		m->v_scroll_bar->setValue((int)m->image_scroll_y);
		m->v_scroll_bar->blockSignals(b);
	}
}

void ImageViewWidget::refrectScrollBar()
{
	double e = 0.75;
	double x = m->h_scroll_bar->value();
	double y = m->v_scroll_bar->value();
	if (fabs(x - m->image_scroll_x) < e) x = m->image_scroll_x; // 差が小さいときは値を維持する
	if (fabs(y - m->image_scroll_y) < e) y = m->image_scroll_y;
	internalScrollImage(x, y, true);
}

QSizeF ImageViewWidget::imageScrollRange() const
{
	QSize sz = imageSize();
	int w = int(sz.width() * m->image_scale);
	int h = int(sz.height() * m->image_scale);
	return QSize(w, h);
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
	return document()->size();
}

void ImageViewWidget::setSelectionOutline(const SelectionOutlineBitmap &data)
{
	m->selection_outline = data;
}

void ImageViewWidget::clearSelectionOutline()
{
	m->outline_renderer->abort();
	setSelectionOutline(SelectionOutlineBitmap());
}

void ImageViewWidget::onRenderingCompleted(RenderedImage const &image)
{
	m->rendered_image = image;
	update();
}

void ImageViewWidget::onSelectionOutlineRenderingCompleted(SelectionOutlineBitmap const &data)
{
	setSelectionOutline(data);
	update();
}

void ImageViewWidget::calcDestinationRect()
{
	double cx = width() / 2.0;
	double cy = height() / 2.0;
	double x = cx - m->image_scroll_x;
	double y = cy - m->image_scroll_y;
	QSizeF sz = imageScrollRange();
	m->destination_rect = QRect((int)x, (int)y, (int)sz.width(), (int)sz.height());
}

void ImageViewWidget::paintViewLater(bool image, bool selection_outline)
{
	calcDestinationRect();

	QSize imagesize = imageSize();

	if (image) {
		QPointF pt0 = mapFromViewportToDocument(QPointF(0, 0));
		QPointF pt1 = mapFromViewportToDocument(QPointF(width(), height()));
		int x0 = (int)floor(pt0.x());
		int y0 = (int)floor(pt0.y());
		int x1 = (int)ceil(pt1.x());
		int y1 = (int)ceil(pt1.y());
		x0 = std::max(x0, 0);
		y0 = std::max(y0, 0);
		x1 = std::min(x1, document()->width());
		y1 = std::min(y1, document()->height());
		QRect r(x0, y0, x1 - x0, y1 - y0);
		m->renderer->request(mainwindow(), r);
	}

	if (selection_outline) {
		clearSelectionOutline();
		m->outline_renderer->request(mainwindow(), QRect(0, 0, imagesize.width(), imagesize.height()));
	}
}

void ImageViewWidget::updateCursorAnchorPos()
{
	m->cursor_anchor_pos = mapFromViewportToDocument(mapFromGlobal(QCursor::pos()));
}

void ImageViewWidget::updateCenterAnchorPos()
{
	m->center_anchor_pos = mapFromViewportToDocument(QPointF(width() / 2.0, height() / 2.0));
}

void ImageViewWidget::setImageScale(double scale, bool updateview)
{
	if (scale < 1.0 / MIN_SCALE) scale = 1.0 / MIN_SCALE;
	if (scale > MAX_SCALE) scale = MAX_SCALE;

	m->image_scale = scale;
	emit scaleChanged(m->image_scale);

	if (updateview) {
		paintViewLater(true, true);
	}
}

void ImageViewWidget::scaleFit(double ratio)
{
	QSize sz = imageSize();
	double w = sz.width();
	double h = sz.height();
	if (w > 0 && h > 0) {
		double sx = width() / w;
		double sy = height() / h;
		m->image_scale = (sx < sy ? sx : sy) * ratio;
	}
	updateScrollBarRange();

	updateCursorAnchorPos();

	scrollImage(w * m->image_scale / 2.0, h * m->image_scale / 2.0, true);
}

void ImageViewWidget::zoomToCursor(double scale)
{
	clearSelectionOutline();

	QPoint pos = mapFromGlobal(QCursor::pos());

	setImageScale(scale, false);
	updateScrollBarRange();

	double x = m->cursor_anchor_pos.x() * m->image_scale + width() / 2.0 - (pos.x() + 0.5);
	double y = m->cursor_anchor_pos.y() * m->image_scale + height() / 2.0 - (pos.y() + 0.5);
	scrollImage(x, y, true);

	updateCenterAnchorPos();
}

void ImageViewWidget::zoomToCenter(double scale)
{
	clearSelectionOutline();

	QPointF pos(width() / 2.0, height() / 2.0);
	m->cursor_anchor_pos = mapFromViewportToDocument(pos);

	setImageScale(scale, false);
	updateScrollBarRange();

	double x = m->cursor_anchor_pos.x() * m->image_scale + width() / 2.0 - pos.x();
	double y = m->cursor_anchor_pos.y() * m->image_scale + height() / 2.0 - pos.y();
	scrollImage(x, y, true);

	updateCenterAnchorPos();
}

void ImageViewWidget::scale100()
{
	zoomToCenter(1.0);
}

void ImageViewWidget::zoomIn()
{
	zoomToCenter(m->image_scale * 2);
}

void ImageViewWidget::zoomOut()
{
	zoomToCenter(m->image_scale / 2);
}

SelectionOutlineBitmap ImageViewWidget::renderSelectionOutlineBitmap(bool *abort)
{
	SelectionOutlineBitmap data;
	int dw = document()->width();
	int dh = document()->height();
	if (dw > 0 && dh > 0) {
		QPointF dp0(0, 0);
		QPointF dp1(width(), height());
		dp0 = mapFromViewportToDocument(dp0);
		dp1 = mapFromViewportToDocument(dp1);
		dp0.rx() = floor(std::max(dp0.rx(), (double)0));
		dp0.ry() = floor(std::max(dp0.ry(), (double)0));
		dp1.rx() = ceil(std::min(dp1.rx(), (double)dw));
		dp1.ry() = ceil(std::min(dp1.ry(), (double)dh));
		QPointF vp0 = mapFromDocumentToViewport(dp0);
		QPointF vp1 = mapFromDocumentToViewport(dp1);
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
			selection = document()->renderSelection(QRect(dx, dy, dw, dh), &m->sync, abort);
			if (abort && *abort) return SelectionOutlineBitmap();
			selection = selection.scaled(vw, vh);
			data.point = QPoint(vx, vy);
		}
		if (selection.width() > 0 && selection.height() > 0) {
			QImage image(vw, vh, QImage::Format_Grayscale8);
			image.fill(Qt::white);
			for (int y = 1; y + 1 < vh; y++) {
				if (abort && *abort) return SelectionOutlineBitmap();
				uint8_t const *s0 = selection.scanLine(y - 1);
				uint8_t const *s1 = selection.scanLine(y);
				uint8_t const *s2 = selection.scanLine(y + 1);
				uint8_t *d = image.scanLine(y);
				for (int x = 1; x + 1 < vw; x++) {
					uint8_t v = ~(s0[x - 1] & s0[x] & s0[x + 1] & s1[x - 1] & s1[x + 1] & s2[x - 1] & s2[x] & s2[x + 1]) & s1[x];
					d[x] = (v & 0x80) ? 0 : 255;
				}
			}
			data.bitmap = QBitmap::fromImage(image);
		}
	}
	return data;
}

void ImageViewWidget::paintEvent(QPaintEvent *)
{
	int doc_w = document()->width();
	int doc_h = document()->height();
	if (doc_w > 0 && doc_h > 0) {
		QPainter pr(this);
		pr.fillRect(rect(), QColor(240, 240, 240));

		// 画像
		int img_w = m->destination_rect.width();
		int img_h = m->destination_rect.height();
		if (img_w > 0 && img_h > 0) {
			if (!m->rendered_image.image.isNull()) {
				QImage image = m->rendered_image.image;
				QPointF org = mapFromDocumentToViewport(QPointF(0, 0));
				int ox = (int)floor(org.x() + 0.5);
				int oy = (int)floor(org.y() + 0.5);
				for (int y = 0; y < image.height(); y += 64) {
					for (int x = 0; x < image.width(); x += 64) {
						int src_x0 = m->rendered_image.rect.x() + x;
						int src_y0 = m->rendered_image.rect.y() + y;
						int src_x1 = m->rendered_image.rect.x() + std::min(x + 65, image.width());
						int src_y1 = m->rendered_image.rect.y() + std::min(y + 65, image.height());
						QPointF pt0(src_x0, src_y0);
						QPointF pt1(src_x1, src_y1);
						pt0 = mapFromDocumentToViewport(pt0);
						pt1 = mapFromDocumentToViewport(pt1);
						int dst_x0 = (int)floor(pt0.x() + 0.5);
						int dst_y0 = (int)floor(pt0.y() + 0.5);
						int dst_x1 = (int)floor(pt1.x() + 0.5);
						int dst_y1 = (int)floor(pt1.y() + 0.5);
						if (dst_x0 >= width()) continue;
						if (dst_y0 >= height()) continue;
						if (dst_x1 <= 0) continue;
						if (dst_y1 <= 0) continue;
						QRect sr(x, y, src_x1 - src_x0, src_y1 - src_y0);
						QRect dr(dst_x0, dst_y0, dst_x1 - dst_x0, dst_y1 - dst_y0);
						if (sr.width() > 0 && sr.height() > 0 && dr.width() > 0 && dr.height() > 0) {
							QImage tmpimg(dr.width(), dr.height(), QImage::Format_RGBA8888);
							{
								QPainter pr2(&tmpimg);
								pr2.setBrushOrigin(ox - dr.x(), oy - dr.y());
								pr2.fillRect(tmpimg.rect(), TransparentCheckerBrush::brush());
								pr2.drawImage(QRect(0, 0, dr.width(), dr.height()), image, sr);
							}
							pr.drawImage(dr.x(), dr.y(), tmpimg);
						}
					}
				}
			}
		}

		if (m->image_scale == MAX_SCALE) {
			QPoint org = mapFromDocumentToViewport(QPointF(0, 0)).toPoint();
			QPointF topleft = mapFromViewportToDocument(QPointF(0, 0));
			int topleft_x = (int)floor(topleft.x());
			int topleft_y = (int)floor(topleft.y());
			pr.save();
			pr.setOpacity(0.25);
			pr.setBrushOrigin(org);
			int x = topleft_x;
			while (1) {
				QPointF pt = mapFromDocumentToViewport(QPointF(x, topleft_y));
				int z = (int)floor(pt.x());
				if (z >= width()) break;
				pr.fillRect(z, 0, 1, height(), m->horz_stripe_brush);
				x++;
			}
			int y = topleft_y;
			while (1) {
				QPointF pt = mapFromDocumentToViewport(QPointF(topleft_x, y));
				int z = (int)floor(pt.y());
				if (z >= height()) break;
				pr.fillRect(0, z, width(), 1, m->vert_stripe_brush);
				y++;
			}
			pr.restore();
		}

		// 選択領域点線
		if (!m->selection_outline.bitmap.isNull()) {
			QBrush brush = stripeBrush(false);
			pr.save();
			pr.setClipRegion(QRegion(m->selection_outline.bitmap).translated(m->selection_outline.point));
			pr.setOpacity(0.5);
			pr.fillRect(0, 0, width(), height(), brush);
			pr.restore();
		}

		QBrush blink_brush = stripeBrush(true);

		// 範囲指定矩形点滅
		if (m->rect_visible) {
			pr.setOpacity(0.2);
			double x0 = m->rect_start.x();
			double y0 = m->rect_start.y();
			double x1 = m->rect_end.x();
			double y1 = m->rect_end.y();
			if (x0 > x1) std::swap(x0, x1);
			if (y0 > y1) std::swap(y0, y1);
			QPointF pt;
			pt = mapFromDocumentToViewport(QPointF(x0, y0));
			x0 = floor(pt.x());
			y0 = floor(pt.y());
			pt = mapFromDocumentToViewport(QPointF(x1, y1));
			x1 = floor(pt.x());
			y1 = floor(pt.y());
			misc::drawFrame(&pr, x0, y0, x1 - x0 + 1, y1 - y0 + 1, blink_brush, blink_brush);

			// カーソル
			{
				int x, y;

				pr.fillRect(x0 - 4, y0 - 4, 9, 9, blink_brush);
				pr.fillRect(x1 - 4, y0 - 4, 9, 9, blink_brush);
				pr.fillRect(x0 - 4, y1 - 4, 9, 9, blink_brush);
				pr.fillRect(x1 - 4, y1 - 4, 9, 9, blink_brush);

				x = (x0 + x1) / 2;
				y = y0;
				pr.fillRect(x - 4, y - 4, 9, 9, blink_brush);
				x = (x0 + x1) / 2;
				y = y1;
				pr.fillRect(x - 4, y - 4, 9, 9, blink_brush);
				x = x0;
				y = (y0 + y1) / 2;
				pr.fillRect(x - 4, y - 4, 9, 9, blink_brush);
				x = x1;
				y = (y0 + y1) / 2;
				pr.fillRect(x - 4, y - 4, 9, 9, blink_brush);

				x = (x0 + x1) / 2;
				y = (y0 + y1) / 2;
				pr.fillRect(x - 4, y - 4, 9, 9, blink_brush);
			}
		}

		// 外周
		pr.setRenderHint(QPainter::Antialiasing);
		QPointF pt0(0, 0);
		QPointF pt1(doc_w, doc_h);
		pt0 = mapFromDocumentToViewport(pt0);
		pt1 = mapFromDocumentToViewport(pt1);
		int x = (int)floor(pt0.x() + 0.5);
		int y = (int)floor(pt0.y() + 0.5);
		int w = (int)floor(pt1.x() + 0.5) - x;
		int h = (int)floor(pt1.y() + 0.5) - y;
		pr.drawRect(x, y, w, h);
	}
}

void ImageViewWidget::resizeEvent(QResizeEvent *)
{
	clearSelectionOutline();
	updateScrollBarRange();
	paintViewLater(true, true);
}

void ImageViewWidget::mousePressEvent(QMouseEvent *e)
{
	m->left_button = (e->buttons() & Qt::LeftButton);
	if (m->left_button) {
		QPoint pos = mapFromGlobal(QCursor::pos());
		m->mouse_press_pos = pos;
		m->scroll_origin_x = m->image_scroll_x;
		m->scroll_origin_y = m->image_scroll_y;
		mainwindow()->onMouseLeftButtonPress(pos.x(), pos.y());
	}
}

void ImageViewWidget::mouseMoveEvent(QMouseEvent *)
{
	QPoint pos = mapFromGlobal(QCursor::pos());
	if (m->mouse_pos == pos) return;
	m->mouse_pos = pos;

	setCursor2(Qt::ArrowCursor);

	if (hasFocus()) {
		if (mainwindow()->onMouseMove(pos.x(), pos.y(), m->left_button)) {
			//
		} else {
			setCursor2(Qt::OpenHandCursor);
			if (m->left_button) {
				clearSelectionOutline();
				int delta_x = pos.x() - m->mouse_press_pos.x();
				int delta_y = pos.y() - m->mouse_press_pos.y();
				scrollImage(m->scroll_origin_x - delta_x, m->scroll_origin_y - delta_y, m->left_button);
			}
		}
	}

	m->cursor_anchor_pos = mapFromViewportToDocument(pos);
	m->wheel_delta = 0;

	setCursor(m->cursor);
}

void ImageViewWidget::setCursor2(QCursor const &cursor)
{
	m->cursor = cursor;

}

void ImageViewWidget::mouseReleaseEvent(QMouseEvent *)
{
	QPoint pos = mapFromGlobal(QCursor::pos());
	if (m->left_button && hasFocus()) {
		mainwindow()->onMouseLeftButtonRelase(pos.x(), pos.y(), true);
	}
	m->left_button = false;
}

void ImageViewWidget::wheelEvent(QWheelEvent *e)
{
	double scale = 1;
	double d = e->delta();
	double t = 1.001;
	scale *= pow(t, d);
	zoomToCursor(m->image_scale * scale);
}

void ImageViewWidget::timerEvent(QTimerEvent *)
{
	m->stripe_animation = (m->stripe_animation + 1) & 7;
	update();
}

