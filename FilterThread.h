#ifndef FILTERTHREAD_H
#define FILTERTHREAD_H

#include <QMutex>
#include <QWaitCondition>
#include <atomic>
#include <qthread.h>
#include "ImageViewRenderer.h"
#include "euclase.h"

enum class FilterStatus {
	Idle,
	Start,
	Ready,
	Busy,
	Done,
};

class FilterThread : public QThread {
	Q_OBJECT
protected:
	void run();
public:
	FilterThread();
	QMutex mutex_;
	QWaitCondition waiter_;
	std::atomic<FilterStatus> status_;
	bool cancel_request = false;
	std::function<euclase::Image (euclase::Image const &, int, bool *cancel_request)> fn;
	euclase::Image source_image;
	euclase::Image result_image;
	int param = 10;
	void setParam(int par, bool cancel);
signals:
	void completed(RenderedImage const &image);
};


#endif // FILTERTHREAD_H
