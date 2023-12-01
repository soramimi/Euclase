#include "ResizeDialog.h"
#include "ui_ResizeDialog.h"

ResizeDialog::ResizeDialog(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::ResizeDialog)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	ui->checkBox_sync->setChecked(true);
	ui->radioButton_bicubic->click();
}

ResizeDialog::~ResizeDialog()
{
	delete ui;
}

void ResizeDialog::setImageSize(QSize const &sz)
{
	original_width = sz.width();
	original_height = sz.height();
	ui->lineEdit_width->setText(QString::number(original_width));
	ui->lineEdit_height->setText(QString::number(original_height));
	ui->lineEdit_width->setFocus();
	ui->lineEdit_width->selectAll();
}

QSize ResizeDialog::imageSize() const
{
	int w = ui->lineEdit_width->text().toUInt();
	int h = ui->lineEdit_height->text().toUInt();
	return QSize(w, h);
}

euclase::EnlargeMethod ResizeDialog::method() const
{
	if (ui->radioButton_bilinear->isChecked()) return euclase::EnlargeMethod::Bilinear;
	if (ui->radioButton_bicubic->isChecked()) return euclase::EnlargeMethod::Bicubic;
	return euclase::EnlargeMethod::NearestNeighbor;
}

void ResizeDialog::sync_x()
{
	if (signalsBlocked()) return;
	unsigned int w = ui->lineEdit_width->text().toInt();
	if (w > 0 && ui->checkBox_sync->isChecked()) {
		unsigned int h = original_height * w / original_width;
		if (h < 1) h = 1;
		blockSignals(true);
		ui->lineEdit_height->setText(QString::number(h));
		blockSignals(false);
	}
}

void ResizeDialog::sync_y()
{
	if (signalsBlocked()) return;
	unsigned int h = ui->lineEdit_height->text().toInt();
	if (h > 0 && ui->checkBox_sync->isChecked()) {
		int w = original_width * h / original_height;
		if (w < 1) w = 1;
		blockSignals(true);
		ui->lineEdit_width->setText(QString::number(w));
		blockSignals(false);
	}
}

void ResizeDialog::on_lineEdit_width_textChanged(const QString &text)
{
	sync_x();
}

void ResizeDialog::on_lineEdit_height_textChanged(const QString &text)
{
	sync_x();
}

void ResizeDialog::on_checkBox_sync_stateChanged(int)
{
	sync_x();
}
