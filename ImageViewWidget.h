#ifndef IMAGEVIEWWIDGET_H
#define IMAGEVIEWWIDGET_H

#include <QScrollBar>
#include <QWidget>
#include "MainWindow.h"
#include "SelectionOutlineRenderer.h"

class Document;
class RenderedImage;

class ImageViewWidget : public QWidget {
	Q_OBJECT
private:
	struct Private;
	Private *m;

	MainWindow *mainwindow();

	Document *document();
	Document const *document() const;

//	bool isValidImage() const;
	QSize imageSize() const;

	QSizeF imageScrollRange() const;
	void internalScrollImage(double x, double y, bool updateview);
	void scrollImage(double x, double y, bool updateview);
	void setImageScale(double scale, bool updateview);
	QBrush getTransparentBackgroundBrush();
	void setScrollBarRange(QScrollBar *h, QScrollBar *v);
	void updateScrollBarRange();
	void zoomToCursor(double scale);
	void zoomToCenter(double scale);
	void updateCursorAnchorPos();
	void updateCenterAnchorPos();
	void calcDestinationRect();
	QBrush stripeBrush(bool blink);
	void initBrushes();
protected:
	void resizeEvent(QResizeEvent *) override;
	void paintEvent(QPaintEvent *) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *e);
	void wheelEvent(QWheelEvent *) override;
	void timerEvent(QTimerEvent *);
public:
	explicit ImageViewWidget(QWidget *parent = nullptr);
	~ImageViewWidget() override;

	void bind(MainWindow *m, QScrollBar *vsb, QScrollBar *hsb);

	QPointF mapFromViewportToDocument(const QPointF &pos);
	QPointF mapFromDocumentToViewport(QPointF const &pos);

	QMutex *synchronizer();

	void showRect(const QPointF &start, const QPointF &end);
	void hideRect();

	void refrectScrollBar();

	void scaleFit(double ratio = 1.0);
	void scale100();

	void zoomIn();
	void zoomOut();

	void paintViewLater(bool image, bool selection_outline);

	void setSelectionOutline(SelectionOutlineBitmap const &data);
	void clearSelectionOutline();
	QBitmap updateSelection_();
	SelectionOutlineBitmap renderSelectionOutlineBitmap(bool *abort);
	void stopRendering(bool wait);
	bool isRectVisible() const;
	void setCursor2(const QCursor &cursor);
private slots:
	void onRenderingCompleted(const RenderedImage &image);
	void onSelectionOutlineRenderingCompleted(const SelectionOutlineBitmap &data);
signals:
	void scaleChanged(double scale);
};

#endif // IMAGEVIEWWIDGET_H
