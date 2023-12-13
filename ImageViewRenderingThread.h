#ifndef IMAGEVIEWRENDERINGTHREAD_H
#define IMAGEVIEWRENDERINGTHREAD_H

#include "SelectionOutline.h"
#include "euclase.h"
#include <QBrush>
#include <QImage>
#include <QObject>
#include <QRect>
#include <QThread>
#include <condition_variable>
#include <deque>
#include <thread>

class MainWindow;

class RenderedData {
public:
	SelectionOutline selection_outline;
};
Q_DECLARE_METATYPE(RenderedData)

class ImageViewRenderingThread : public QObject {
	Q_OBJECT
private:
	struct Private;
	Private *m;
protected:
	void run();
	void wait();
	void start();
	void stop();
public:
	ImageViewRenderingThread(QObject *parent = nullptr);
	~ImageViewRenderingThread();
	void init(MainWindow *mw);
	void request(QRect const &rect, const QPoint &center, int div);
	void cancel();
	void requestInterruption();
	bool isInterrupted();
signals:
	void done(RenderedData const &image);
};

#endif // IMAGEVIEWRENDERINGTHREAD_H
