#include "FilterFormBlur.h"
#include "ui_FilterFormBlur.h"

FilterFormBlur::FilterFormBlur(QWidget *parent)
	: AbstractFilterForm(parent)
	, ui(new Ui::FilterFormBlur)
{
	ui->setupUi(this);
}

FilterFormBlur::~FilterFormBlur()
{
	delete ui;
}

void FilterFormBlur::start()
{
	ui->horizontalSlider->setValue(context()->parameter("amount").toInt());
}

void FilterFormBlur::on_horizontalSlider_valueChanged(int value)
{
	context()->setParameter("amount", value);
	dialog()->updateFilter();
}

