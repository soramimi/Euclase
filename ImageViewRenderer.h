#ifndef IMAGEVIEWRENDERER_H
#define IMAGEVIEWRENDERER_H

#include <QBrush>
#include <QImage>
#include <QObject>
#include <QRect>
#include <QThread>
#include <deque>
#include "Document.h"

class MainWindow;

class RenderedImage {
public:
	QRect rect;
	Document::Image image;
};
Q_DECLARE_METATYPE(RenderedImage)

class ImageViewRenderer : public QThread {
	Q_OBJECT
private:
	volatile bool requested_ = false;
	MainWindow *mainwindow_;
	QRect rect_;
	bool abort_ = false;
protected:
	void run();
public:
	explicit ImageViewRenderer(QObject *parent = nullptr);
	~ImageViewRenderer();
	void request(MainWindow *mw, QRect const &rect);
	void abort(bool wait);
signals:
	void done(RenderedImage const &image);
};

#endif // IMAGEVIEWRENDERER_H
