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

	QSize imageSize() const;

	QSize imageScrollRange() const;
	void internalScrollImage(double x, double y, bool render);
	void scrollImage(double x, double y, bool updateview);
	bool setImageScale(double scale, bool updateview);
	QBrush getTransparentBackgroundBrush();
	void setScrollBarRange(QScrollBar *h, QScrollBar *v);
	void updateScrollBarRange();
	void zoomToCursor(double scale);
	void zoomToCenter(double scale);
	void updateCursorAnchorPos();
	void updateCenterAnchorPos();
	QBrush stripeBrush();
	void initBrushes();
	QImage generateOutlineImage(const euclase::Image &selection, bool *abort);
	void internalUpdateScroll();
	void startRenderingThread();
	void stopRenderingThread();
	void runImageRendering();
	void geometryChanged(bool render);
	void clearSelectionOutline();
	void runSelectionRendering();
	void invalidateComposedPanels(const QRect &rect);
	void setRenderRequested(bool f);
	void setScrollOffset(double x, double y);
	CoordinateMapper currentCoordinateMapper() const;
	CoordinateMapper offscreenCoordinateMapper() const;
	void rescaleOffScreen();
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

	void paintViewLater(bool image);

	QBitmap updateSelection_();
	SelectionOutline renderSelectionOutline(bool *abort);
	bool isRectVisible() const;
	void setToolCursor(const QCursor &cursor);
	void doHandScroll();
	void updateToolCursor();

	void clearRenderCache();
	void requestRendering(bool invalidate, const QRect &rect);

	static constexpr QColor BGCOLOR = QColor(240, 240, 240);
private slots:
	void onSelectionOutlineReady(SelectionOutline const &data);
	void onTimer();
signals:
	void notifySelectionOutlineReady(SelectionOutline const &data);
	void scaleChanged(double scale);
};

#endif // IMAGEVIEWWIDGET_H
