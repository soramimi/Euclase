#include "NewDialog.h"
#include "ui_NewDialog.h"

#include <QClipboard>

NewDialog::NewDialog(QWidget *parent)
	: QDialog(parent)
	, ui(new Ui::NewDialog)
{
	ui->setupUi(this);
	ui->radioButton_new->click();
	ui->lineEdit_width->setText("1920");
	ui->lineEdit_height->setText("1080");
}

NewDialog::~NewDialog()
{
	delete ui;
}

NewDialog::From NewDialog::from() const
{
	if (ui->radioButton_from_clipboard->isChecked()) {
		return From::Clipboard;
	}
	return From::New;
}

QSize NewDialog::validateImageSize(QSize const &size) const
{
	int w = std::min(size.width(), 10000);
	int h = std::min(size.height(), 10000);
	w = std::max(w, 0);
	h = std::max(h, 0);
	return QSize(w, h);
}

void NewDialog::setImageSize(QSize size)
{
	size = validateImageSize(size);
	bool f0 = ui->lineEdit_width->blockSignals(true);
	bool f1 = ui->lineEdit_height->blockSignals(true);
	ui->lineEdit_width->setText(QString::number(size.width()));
	ui->lineEdit_height->setText(QString::number(size.height()));
	ui->lineEdit_height->blockSignals(f1);
	ui->lineEdit_width->blockSignals(f0);
}

QSize NewDialog::imageSize() const
{
	int w = strtoul(ui->lineEdit_width->text().toStdString().c_str(), nullptr, 10);
	int h = strtoul(ui->lineEdit_height->text().toStdString().c_str(), nullptr, 10);
	return validateImageSize(QSize(w, h));
}

void NewDialog::on_radioButton_from_clipboard_clicked()
{
	QImage image = QApplication::clipboard()->image();
	int w = image.width();
	int h = image.height();
	setImageSize(QSize(w, h));
}
