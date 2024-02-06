
#include "FilterDialog.h"
#include "ui_FilterDialog.h"
#include <QDateTime>
#include <QDebug>
#include <QMutex>
#include <QPainter>
#include <QTimer>
#include "MainWindow.h"
#include <QWaitCondition>
#include <thread>
#include <utility>

struct FilterDialog::Private {
	QDateTime start_filter;
	int timer_id = 0;
	bool busy = false;
	bool done = false;
	bool update = false;
	std::thread thread;
	FilterFunction filter_fn;
	FilterContext *context;
	euclase::Image result_image;
	float progress = 0;
};

FilterDialog::FilterDialog(MainWindow *parent, FilterContext *context, AbstractFilterForm *form, const FilterFunction &fn)
	: QDialog(parent)
	, ui(new Ui::FilterDialog)
	, m(new Private)
	, mainwindow(parent)
{
	ui->setupUi(this);

	m->filter_fn = fn;
	m->context = context;

	if (form) {
		form->setParent(ui->stackedWidget);
		ui->stackedWidget->addWidget(form);
		ui->stackedWidget->setCurrentWidget(form);
		form->start();
	}

	ui->checkBox_preview->setChecked(true);
	mainwindow->setPreviewLayerEnable(ui->checkBox_preview->isChecked());

	setProgress(0);

	startTimer(100);

	updateFilter();
}

FilterDialog::~FilterDialog()
{
	stopFilter(true); // スレッド停止（完了まで待つ）
	delete m;
	delete ui;
}

FilterContext *FilterDialog::context()
{
	return m->context;
}

void FilterDialog::stopFilter(bool join)
{
	*m->context->progress_ptr() = 0.0f;
	*m->context->cancel_ptr() = true;
	if (join) {
		if (m->thread.joinable()) {
			m->thread.join();
		}
	}
	m->busy = false;
	m->done = false;
	m->update = false;
}

void FilterDialog::updateFilter()
{
	stopFilter(false);
	m->start_filter = QDateTime::currentDateTime().addMSecs(100);
}

void FilterDialog::startFilter()
{
	m->thread = std::thread([&](){
		auto memtype = m->context->sourceImage().memtype();
		m->result_image = m->filter_fn(m->context);
		if (!*m->context->cancel_ptr()) {
			m->result_image.memconvert(memtype);
			m->done = true;
			m->update = true;
		}
		m->busy = false;
	});
}

void FilterDialog::setProgress(float value)
{
	m->progress = value;
	update();
}

void FilterDialog::updateImageView()
{
	mainwindow->setPreviewLayerEnable(ui->checkBox_preview->isChecked());
	mainwindow->updateImageViewEntire();
}

void FilterDialog::timerEvent(QTimerEvent *event)
{
	{
		float value = *m->context->progress_ptr();
		setProgress(value);
	}

	if (m->busy) return;

	if (m->update) {
		m->update = false;
		mainwindow->setFilteredImage(m->result_image);
		updateImageView();
	}

	if (m->start_filter.isValid()) {
		QDateTime now = QDateTime::currentDateTime();
		if (m->start_filter <= now) {
			m->start_filter = {};
			stopFilter(true);
			m->busy = true;
			*m->context->cancel_ptr() = false;
			startFilter();
		}
	}
}

void FilterDialog::paintEvent(QPaintEvent *event)
{
	QDialog::paintEvent(event);
	if (m->progress > 0 && m->progress < 1) {
		QPainter pr(this);
		int w = width();
		int x = int(std::max(0.0f, std::min(1.0f, m->progress)) * w);
		int h = height();
		int z = 4;
		pr.fillRect(0, h - z - 1, w, z + 1, QColor(96, 96, 96));
		pr.fillRect(0, h - z, x, z, QColor(0, 240, 0));
	}
}

euclase::Image FilterDialog::result()
{
	return m->result_image;
}

bool FilterDialog::isPreviewEnabled() const
{
	return ui->checkBox_preview->isChecked();
}

void FilterDialog::on_checkBox_preview_stateChanged(int arg1)
{
	updateImageView();
}

