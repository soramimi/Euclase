#include "FilterThread.h"

#include <QDebug>

FilterThread::FilterThread()
{
	status_ = FilterStatus::Idle;
}

void FilterThread::run()
{
	while (1) {
		{
			QMutexLocker lock(&mutex_);
			if (status_ == FilterStatus::Idle) {
				status_ = FilterStatus::Ready;
				waiter_.wait(&mutex_);
				status_ = FilterStatus::Busy;
			}
			cancel_request = false;
		}
		if (isInterruptionRequested()) break;
		if (status_ == FilterStatus::Busy) {
			euclase::Image result = fn(source_image, param, &cancel_request);
			bool done = false;
			{
				QMutexLocker lock(&mutex_);
				if (status_ == FilterStatus::Busy && !cancel_request) {
					result_image = result;
					done = true;
				}
				status_ = FilterStatus::Idle;
			}
			if (done) {
				RenderedImage result;
				result.image = result_image;
				qDebug() << param;
				emit completed(result);
			}
		}
	}
}

void FilterThread::setParam(int par, bool cancel)
{
	QMutexLocker lock(&mutex_);
	if (cancel) {
		cancel_request = true;
	}
	param = par;
	result_image = {};
	status_ = FilterStatus::Start;
	waiter_.wakeAll();
}
