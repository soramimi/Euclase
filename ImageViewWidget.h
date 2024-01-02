#ifndef IMAGEVIEWWIDGET_H
#define IMAGEVIEWWIDGET_H

#include "CoordinateMapper.h"
#include "MainWindow.h"
#include "SelectionOutline.h"
#include <QScrollBar>
#include <QTimer>
#include <QWidget>

class Canvas;

class ImageViewWidget : public QWidget {
	Q_OBJECT
	friend class MainWindow; //@todo remove
private:
	struct Private;
	Private *m;

	static constexpr int OFFSCREEN_PANEL_SIZE = 256;

	QTimer timer_;

	MainWindow *mainwindow();

	Canvas *canvas();
	Canvas const *canvas() const;
	
	QSize canvasSize() const;
	QPoint center() const;
	QPointF centerF() const;

	QSize imageScrollRange() const;
	void internalScrollImage(double x, double y, bool differential_update);
	void scrollImage(double x, double y, bool differential_update);
	bool setScale(double scale, bool fire_event);
	double scale() const;
	void setScrollBarRange(QScrollBar *h, QScrollBar *v);
	void updateScrollBarRange();
	void zoomToCursor(double scale);
	void zoomToCenter(double scale);
	void updateCursorAnchorPos();
	QBrush stripeBrush();
	void initBrushes();
	QImage generateSelectionOutlineImage(const euclase::Image &selection, bool *abort);
	void internalUpdateScroll();
	void startRenderingThread();
	void stopRenderingThread();
	void runImageRendering();
	void runSelectionRendering();
	void setRenderRequested(bool f);
	CoordinateMapper currentCoordinateMapper() const;
	CoordinateMapper offscreenCoordinateMapper() const;
	void rescaleOffScreen();
	void zoomInternal(const QPointF &pos);
	void setScaleAnchorPos(const QPointF &pos);
	QPointF getScaleAnchorPos();
protected:
	void resizeEvent(QResizeEvent *) override;
	void paintEvent(QPaintEvent *) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *e) override;
	void wheelEvent(QWheelEvent *) override;
public:
	explicit ImageViewWidget(QWidget *parent = nullptr);
	~ImageViewWidget() override;

	void init(MainWindow *m, QScrollBar *vsb, QScrollBar *hsb);

	QPointF mapToCanvasFromViewport(const QPointF &pos);
	QPointF mapToViewportFromCanvas(QPointF const &pos);

	void showRect(const QPointF &start, const QPointF &end);
	void hideRect(bool update);

	void refrectScrollBar();

	void scaleFit(double ratio = 1.0);
	void scale100();

	void zoomIn();
	void zoomOut();

	void requestUpdateSelectionOutline();

	QBitmap updateSelection_();
	SelectionOutline renderSelectionOutline(bool *abort);
	bool isRectVisible() const;
	void setToolCursor(const QCursor &cursor);
	void doHandScroll();
	void updateToolCursor();

	static constexpr QColor BGCOLOR = QColor(240, 240, 240);

	void clearRenderCache(bool clear_offscreen, bool lock);
	void requestUpdateEntire(bool lock);
	void requestUpdateView(const QRect &viewrect, bool lock);
	void requestUpdateCanvas(const QRect &canvasrect, bool lock);
	void requestRendering(const QRect &canvasrect);
private slots:
	void onSelectionOutlineReady(SelectionOutline const &data);
	void onTimer();
signals:
	void notifySelectionOutlineReady(SelectionOutline const &data);
	void scaleChanged(double scale);
};

#endif // IMAGEVIEWWIDGET_H
