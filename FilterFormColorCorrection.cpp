#include "FilterFormColorCorrection.h"
#include "ui_FilterFormColorCorrection.h"

FilterFormColorCorrection::FilterFormColorCorrection(QWidget *parent)
	: AbstractFilterForm(parent)
	, ui(new Ui::FilterFormColorCorrection)
{
	ui->setupUi(this);
}

FilterFormColorCorrection::~FilterFormColorCorrection()
{
	delete ui;
}

void FilterFormColorCorrection::start()
{
	context()->setParameter("hue", 0);
	context()->setParameter("saturation", 0);
	context()->setParameter("value", 0);

}

void FilterFormColorCorrection::on_horizontalSlider_hue_valueChanged(int hue)
{
	setHue(hue);
}


void FilterFormColorCorrection::on_horizontalSlider_saturation_valueChanged(int sat)
{
	setSaturation(sat);
}


void FilterFormColorCorrection::on_horizontalSlider_brightness_valueChanged(int bri)
{
	setBrightness(bri);
}


void FilterFormColorCorrection::on_spinBox_hue_valueChanged(int hue)
{
	setHue(hue);
}


void FilterFormColorCorrection::on_spinBox_saturation_valueChanged(int sat)
{
	setSaturation(sat);
}


void FilterFormColorCorrection::on_spinBox_brightness_valueChanged(int bri)
{
	setBrightness(bri);
}

void FilterFormColorCorrection::setHue(int hue)
{
	constexpr auto name = "hue";
	auto old = context()->parameter(name);
	if (hue != old) {
		{
			auto b = ui->horizontalSlider_hue->blockSignals(true);
			ui->horizontalSlider_hue->setValue(hue);
			ui->horizontalSlider_hue->blockSignals(b);
		}
		{
			auto b = ui->spinBox_hue->blockSignals(true);
			ui->spinBox_hue->setValue(hue);
			ui->spinBox_hue->blockSignals(b);
		}
		context()->setParameter(name, hue);
		dialog()->updateFilter();
	}
}

void FilterFormColorCorrection::setSaturation(int sat)
{
	constexpr auto name = "saturation";
	auto old = context()->parameter(name);
	if (sat != old) {
		{
			auto b = ui->horizontalSlider_saturation->blockSignals(true);
			ui->horizontalSlider_saturation->setValue(sat);
			ui->horizontalSlider_saturation->blockSignals(b);
		}
		{
			auto b = ui->spinBox_saturation->blockSignals(true);
			ui->spinBox_saturation->setValue(sat);
			ui->spinBox_saturation->blockSignals(b);
		}
		context()->setParameter(name, sat);
		dialog()->updateFilter();
	}
}

void FilterFormColorCorrection::setBrightness(int brightness)
{
	constexpr auto name = "brightness";
	auto old = context()->parameter(name);
	if (brightness != old) {
		{
			auto b = ui->horizontalSlider_brightness->blockSignals(true);
			ui->horizontalSlider_brightness->setValue(brightness);
			ui->horizontalSlider_brightness->blockSignals(b);
		}
		{
			auto b = ui->spinBox_brightness->blockSignals(true);
			ui->spinBox_brightness->setValue(brightness);
			ui->spinBox_brightness->blockSignals(b);
		}
		context()->setParameter(name, brightness);
		dialog()->updateFilter();
	}
}

