#include "ColorDialog.h"
#include "ui_ColorDialog.h"

ColorDialog::ColorDialog(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::ColorDialog)
{
	ui->setupUi(this);

	ui->widget_2->bind(ui->widget);
	ui->widget_2->setColor(Qt::red);
}

ColorDialog::~ColorDialog()
{
	delete ui;
}


































