#ifndef IMAGEVIEWWIDGET_H
#define IMAGEVIEWWIDGET_H

#include "CoordinateMapper.h"
#include "MainWindow.h"
#include "SelectionOutline.h"
#include <QRegion>
#include <QScrollBar>
#include <QTimer>
#include <QWidget>

class ImageViewWidget : public QWidget {
	Q_OBJECT
	friend class MainWindow; //@todo remove
private:
	struct Private;
	Private *m;

	static constexpr int OFFSCREEN_PANEL_SIZE = 256;

	QTimer timer_;

	MainWindow *mainwindow();

	QSize canvasSize() const;
	QPointF centerF() const;

	QSize imageScrollRange() const;
	void scrollImage(double x, double y);
	bool setScale(double scale, bool fire_event);
	void setScrollBarRange(QScrollBar *h, QScrollBar *v);
	void updateScrollBarRange();
	bool zoomToCursor(double scale);
	void zoomToCenter(double scale);
	void updateCursorAnchorPos();
	QBrush stripeBrush();
	void initBrushes();
	QImage renderSelectionOutline(const CoordinateMapper &mapper, const QSize &canvas_size,
								  const std::atomic_bool &canceled, QPoint &position);
	void internalUpdateScroll();
	void startRenderingThread();
	void stopRenderingThread();
	void runImageRendering();
	void runSelectionRendering();
	CoordinateMapper currentCoordinateMapper() const;
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

	void showBounds(const QPointF &start, const QPointF &end);
	void hideRect(bool update);

	void refrectScrollBar();

	double scale() const;
	void scaleFit(double ratio = 1.0);
	void scale100();

	void zoomIn();
	void zoomOut();

	void requestUpdateSelectionOutline();

	bool isRectVisible() const;
	void setToolCursor(const QCursor &cursor);
	void doHandScroll();
	void updateToolCursor();

	static constexpr QColor BGCOLOR = QColor(128, 128, 128);

	void clearRenderCache();
	void requestRendering(const QRegion &canvas_region);
private slots:
	void onTimer();
signals:
	void scaleChanged(double scale);
	void updateDocInfo();
};

#endif // IMAGEVIEWWIDGET_H
