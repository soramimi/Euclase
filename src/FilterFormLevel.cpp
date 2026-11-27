#include "FilterFormLevel.h"
#include "ui_FilterFormLevel.h"

FilterFormLevel::FilterFormLevel(QWidget *parent)
	: AbstractFilterForm(parent)
	, ui(new Ui::FilterFormLevel)
{
	ui->setupUi(this);
}

FilterFormLevel::~FilterFormLevel()
{
	delete ui;
}

void FilterFormLevel::start()
{
	ui->horizontalSlider->setValue(context()->parameter("amount").toInt());
}

void FilterFormLevel::on_horizontalSlider_valueChanged(int value)
{
	context()->setParameter("amount", value / 100.0);
	dialog()->updateFilter();
}

