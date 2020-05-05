#include "FilterDialog.h"
#include "FilterThread.h"
#include "ui_FilterDialog.h"
#include <QDebug>
#include <QMutex>
#include <QThread>
#include <QWaitCondition>
#include <atomic>
#include "MainWindow.h"

struct FilterDialog::Private{
	FilterThread thread;
	euclase::Image result_image;
};

FilterDialog::FilterDialog(MainWindow *parent, euclase::Image const &image, std::function<euclase::Image (euclase::Image const &image, int param, bool *cancel_request)> const &fn)
	: QDialog(parent)
	, ui(new Ui::FilterDialog)
	, m(new Private)
	, mainwindow(parent)
{
	ui->setupUi(this);

	m->thread.source_image = image;
	m->thread.fn = fn;

	connect(&m->thread, &FilterThread::completed, mainwindow, &MainWindow::filterCompleted);

	m->thread.start();
	while (m->thread.status_ != FilterStatus::Ready) {
		QThread::msleep(1);
	}
	m->thread.setParam(10, false);
#if 0
	while (1) {
		{
			QMutexLocker lock(&m->thread.mutex_);
			m->result_image = m->thread.result_image;
		}
		if (m->thread.status_ == FilterStatus::Ready && !m->result_image.isNull()) {
			break;
		}
		QThread::msleep(10);
	}
#endif
}

FilterDialog::~FilterDialog()
{
	m->thread.cancel_request = true;
	m->thread.requestInterruption();
	m->thread.waiter_.wakeAll();
	m->thread.wait();
	delete m;
	delete ui;
}



euclase::Image FilterDialog::result()
{
	if (m->thread.cancel_request) return {};
	return m->thread.result_image;
}

void FilterDialog::on_horizontalSlider_valueChanged(int value)
{
	m->thread.setParam(value, true);
}
