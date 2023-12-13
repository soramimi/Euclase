
#include "ApplicationGlobal.h"
#include "ImageViewRenderingThread.h"
#include "MainWindow.h"
#include <QPainter>

struct ImageViewRenderingThread::Private {
	MainWindow *mainwindow;
	bool interrupted = false;
	bool cancel = false;
	std::mutex mutex;
	std::thread thread;
	std::condition_variable cond;
	bool requested = false;
};


ImageViewRenderingThread::ImageViewRenderingThread(QObject *parent)
	: m(new Private)
{
	start();
}

ImageViewRenderingThread::~ImageViewRenderingThread()
{
	stop();
	delete m;
}

void ImageViewRenderingThread::init(MainWindow *mw)
{
	m->mainwindow = mw;
}

void ImageViewRenderingThread::run()
{
	while (1) {
		bool req = false;
		{
			std::unique_lock lock(m->mutex);
			if (isInterrupted()) break;
			if (!m->requested) {
				m->cond.wait(lock);
			}
			if (m->requested) {
				m->requested = false;
				req = true;
			}
		}
		if (req) {
			m->cancel = false;
			SelectionOutline selection_outline = m->mainwindow->renderSelectionOutline(&m->cancel);
			if (m->cancel) {
				m->cancel = false;
			} else {
				RenderedData ri;
				ri.selection_outline = selection_outline;
				emit done(ri);
			}
		}
	}
}

void ImageViewRenderingThread::start()
{
	m->thread = std::thread([&](){
		run();
	});
}

void ImageViewRenderingThread::wait()
{
	if (m->thread.joinable()) {
		m->thread.join();
	}
}

void ImageViewRenderingThread::stop()
{
	requestInterruption();
	wait();
}

void ImageViewRenderingThread::cancel()
{
	m->cancel = true;
}

void ImageViewRenderingThread::request(const QRect &rect, QPoint const &center, int div)
{
	std::lock_guard lock(m->mutex);
	m->requested = true;
	m->cond.notify_all();
}

void ImageViewRenderingThread::requestInterruption()
{
	std::lock_guard lock(m->mutex);
	m->interrupted = true;
	m->cond.notify_all();
}

bool ImageViewRenderingThread::isInterrupted()
{
	return m->interrupted;
}


