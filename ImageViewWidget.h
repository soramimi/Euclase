#ifndef IMAGEVIEWWIDGET_H
#define IMAGEVIEWWIDGET_H

#include <QScrollBar>
#include <QTimer>
#include <QWidget>
#include "MainWindow.h"
#include "SelectionOutline.h"

class Canvas;
class RenderedData;

class ImageViewWidget : public QWidget {
	Q_OBJECT
private:
	struct Private;
	Private *m;

	QTimer timer_;

	MainWindow *mainwindow();

	Canvas *canvas();
	Canvas const *canvas() const;

	QSize imageSize() const;

	QSizeF imageScrollRange() const;
	void internalScrollImage(double x, double y, bool updateview);
	void scrollImage(double x, double y, bool updateview);
	bool setImageScale(double scale, bool updateview);
	QBrush getTransparentBackgroundBrush();
	void setScrollBarRange(QScrollBar *h, QScrollBar *v);
	void updateScrollBarRange();
	void zoomToCursor(double scale);
	void zoomToCenter(double scale);
	void updateCursorAnchorPos();
	void updateCenterAnchorPos();
	void calcDestinationRect();
	QBrush stripeBrush();
	void initBrushes();
	QImage generateOutlineImage(const euclase::Image &selection, bool *abort);
	void internalUpdateScroll();
	void startRenderingThread();
	void stopRenderingThread();
	void runRendering();
	void requestRendering(bool invalidate);
	void geometryChanged();
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

	void setSelectionOutline(SelectionOutline const &data);
	void clearSelectionOutline();
	QBitmap updateSelection_();
	SelectionOutline renderSelectionOutline(bool *abort);
	void stopRendering();
	bool isRectVisible() const;
	void setToolCursor(const QCursor &cursor);
	void doHandScroll();
	void updateToolCursor();

	void clearRenderedPanels();
private slots:
	void onRenderingCompleted(const RenderedData &image);
	void onTimer();
signals:
	void scaleChanged(double scale);
};

#endif // IMAGEVIEWWIDGET_H
