#ifndef LAYERCOMPOSER_H
#define LAYERCOMPOSER_H

#include "SelectionOutlineRenderer.h"
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

class RenderedImage {
public:
	SelectionOutlineBitmap selection_outline_data;
};

Q_DECLARE_METATYPE(RenderedImage)

class LayerComposer : public QObject {
	Q_OBJECT
private:
	struct Private;
	Private *m;
	struct Request {
		QRect rect;
		QPoint center;
	};
	static inline const int INTERNAL_PANEL_SIZE = 256;
protected:
	void run();
	void wait();
	void start();
	void stop();
public:
	LayerComposer(QObject *parent = nullptr);
	~LayerComposer();
	void init(MainWindow *mw);
	void request(QRect const &rect, const QPoint &center, int div);
	void cancel();
	void requestInterruption();
	bool isInterrupted();
	void reset();
	euclase::Image render(int x, int y, int w, int h);
signals:
	void update();
	void done(RenderedImage const &image);
};

#endif // LAYERCOMPOSER_H
