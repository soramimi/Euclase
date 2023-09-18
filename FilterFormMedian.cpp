#include "FilterFormMedian.h"
#include "ui_FilterFormMedian.h"

FilterFormMedian::FilterFormMedian(QWidget *parent)
	: AbstractFilterForm(parent)
	, ui(new Ui::FilterFormMedian)
{
	ui->setupUi(this);
}

FilterFormMedian::~FilterFormMedian()
{
	delete ui;
}

void FilterFormMedian::start()
{
	ui->horizontalSlider->setValue(context()->parameter("amount").toInt());
}

void FilterFormMedian::on_horizontalSlider_valueChanged(int value)
{
	context()->setParameter("amount", value);
	dialog()->updateFilter();
}

