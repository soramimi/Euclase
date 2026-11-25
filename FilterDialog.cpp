
#include "FilterDialog.h"
#include "FilterStatus.h"
#include "ui_FilterDialog.h"
#include <QDateTime>
#include <QPainter>
#include "MainWindow.h"
#include <atomic>
#include <thread>
#include <utility>

struct FilterDialog::Private {
	QDateTime start_filter;
	std::thread thread;
	std::atomic_bool finished{false};
	std::atomic_bool cancel{false};
	std::atomic<float> worker_progress{0.0f};
	FilterFunction filter_fn;
	FilterContext context;
	euclase::Image result_image;
	float progress = 0;
};

FilterDialog::FilterDialog(MainWindow *parent, FilterContext &&context, AbstractFilterForm *form, const FilterFunction &fn)
	: QDialog(parent)
	, ui(new Ui::FilterDialog)
	, m(new Private)
	, mainwindow(parent)
{
	ui->setupUi(this);

	m->filter_fn = fn;
	m->context = std::move(context);

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
	m->cancel = true;
	if (m->thread.joinable()) {
		m->thread.join();
	}
	delete m;
	delete ui;
}

FilterContext *FilterDialog::context()
{
	return &m->context;
}

void FilterDialog::updateFilter()
{
	ui->pushButton_ok->setEnabled(false);
	m->cancel = true;
	setProgress(0);
	m->start_filter = QDateTime::currentDateTime().addMSecs(100);
}

void FilterDialog::startFilter()
{
	m->cancel = false;
	m->finished = false;
	m->worker_progress = 0.0f;
	m->thread = std::thread([this, input = m->context](){
		FilterStatus status(&m->cancel, &m->worker_progress);
		euclase::Image result = m->filter_fn(&input, &status);
		if (!m->cancel) {
			result.memconvert(input.sourceImage().memtype());
		}
		m->result_image = result;
		m->finished = true;
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

void FilterDialog::timerEvent(QTimerEvent *)
{
	if (m->thread.joinable()) {
		if (!m->finished) {
			if (!m->cancel) {
				setProgress(m->worker_progress);
			}
			return;
		}
		m->thread.join();
		setProgress(0);
		if (!m->start_filter.isValid() && m->result_image && mainwindow->setFilteredImage(m->result_image, false)) {
			updateImageView();
			ui->pushButton_ok->setEnabled(true);
		}
	}

	if (m->start_filter.isValid() && m->start_filter <= QDateTime::currentDateTime()) {
		m->start_filter = {};
		startFilter();
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

void FilterDialog::done(int r)
{
	const bool apply = r == QDialog::Accepted;
	if (apply && !ui->pushButton_ok->isEnabled()) {
		return;
	}
	emit end(apply);
}
	




void FilterDialog::on_pushButton_cancel_clicked()
{
	done(QDialog::Rejected);
}

