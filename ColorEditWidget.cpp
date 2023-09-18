#include "ColorEditWidget.h"
#include "SaturationBrightnessWidget.h"
#include "ui_ColorEditWidget.h"

ColorEditWidget::ColorEditWidget(QWidget *parent) :
	QWidget(parent),
	ui(new Ui::ColorEditWidget)
{
	ui->setupUi(this);

	ui->horizontalSlider_rgb_r->setVisualType(ColorSlider::RGB_R);
	ui->horizontalSlider_rgb_g->setVisualType(ColorSlider::RGB_G);
	ui->horizontalSlider_rgb_b->setVisualType(ColorSlider::RGB_B);
	ui->horizontalSlider_hsv_h->setVisualType(ColorSlider::HSV_H);
	ui->horizontalSlider_hsv_s->setVisualType(ColorSlider::HSV_S);
	ui->horizontalSlider_hsv_v->setVisualType(ColorSlider::HSV_V);

	ui->horizontalSlider_rgb_r->setMinimumHeight(MIN_SLIDER_HEIGHT);
	ui->horizontalSlider_rgb_g->setMinimumHeight(MIN_SLIDER_HEIGHT);
	ui->horizontalSlider_rgb_b->setMinimumHeight(MIN_SLIDER_HEIGHT);
	ui->horizontalSlider_hsv_h->setMinimumHeight(MIN_SLIDER_HEIGHT);
	ui->horizontalSlider_hsv_s->setMinimumHeight(MIN_SLIDER_HEIGHT);
	ui->horizontalSlider_hsv_v->setMinimumHeight(MIN_SLIDER_HEIGHT);

	ui->tabWidget->setCurrentWidget(ui->tab_color_hsv);

}

ColorEditWidget::~ColorEditWidget()
{
	delete ui;
}

void ColorEditWidget::bind(SaturationBrightnessWidget *w)
{
	pickupper_ = w;
	connect(pickupper_, &SaturationBrightnessWidget::changeColor, this, &ColorEditWidget::setColor);
}

SaturationBrightnessWidget *ColorEditWidget::pickupper()
{
	return pickupper_;
}

QColor ColorEditWidget::color() const
{
	return color_;
}

void ColorEditWidget::setColor(QColor color)
{
	color_ = color;

	auto Set = [&](int v, ColorSlider *slider, QSpinBox *spin){
		bool f1 = slider->blockSignals(true);
		slider->setColor(color_);
		slider->setValue(v);
		slider->blockSignals(f1);
		bool f2 = spin->blockSignals(true);
		spin->setValue(v);
		spin->blockSignals(f2);
	};
	Set(color.red(), ui->horizontalSlider_rgb_r, ui->spinBox_rgb_r);
	Set(color.green(), ui->horizontalSlider_rgb_g, ui->spinBox_rgb_g);
	Set(color.blue(), ui->horizontalSlider_rgb_b, ui->spinBox_rgb_b);
	Set(color.hue(), ui->horizontalSlider_hsv_h, ui->spinBox_hsv_h);
	Set(color.saturation(), ui->horizontalSlider_hsv_s, ui->spinBox_hsv_s);
	Set(color.value(), ui->horizontalSlider_hsv_v, ui->spinBox_hsv_v);

	SaturationBrightnessWidget *w = pickupper();
	if (w) {
		bool f = w->blockSignals(true);
		w->setHSV(color.hue(), color.saturation(), color.value());
		w->updateView();
		w->blockSignals(f);
	}

	//	w_preview->setColor(color_, m->secondary_color);
}

void ColorEditWidget::setColorRed(int value)
{
	QColor c = color();
	c = QColor(value, c.green(), c.blue());
	setColor(c);
}

void ColorEditWidget::setColorGreen(int value)
{
	QColor c = color();
	c = QColor(c.red(), value, c.blue());
	setColor(c);
}

void ColorEditWidget::setColorBlue(int value)
{
	QColor c = color();
	c = QColor(c.red(), c.green(), value);
	setColor(c);
}

void ColorEditWidget::setColorHue(int value)
{
	QColor c = color();
	c = QColor::fromHsv(value, c.saturation(), c.value());
	setColor(c);
}

void ColorEditWidget::setColorSaturation(int value)
{
	QColor c = color();
	c = QColor::fromHsv(c.hue(), value, c.value());
	setColor(c);
}

void ColorEditWidget::setColorValue(int value)
{
	QColor c = color();
	c = QColor::fromHsv(c.hue(), c.saturation(), value);
	setColor(c);
}

void ColorEditWidget::on_horizontalSlider_hsv_h_valueChanged(int value)
{
	setColorHue(value);
}

void ColorEditWidget::on_horizontalSlider_hsv_s_valueChanged(int value)
{
	setColorSaturation(value);
}

void ColorEditWidget::on_horizontalSlider_hsv_v_valueChanged(int value)
{
	setColorValue(value);
}

void ColorEditWidget::on_horizontalSlider_rgb_r_valueChanged(int value)
{
	setColorRed(value);
}

void ColorEditWidget::on_horizontalSlider_rgb_g_valueChanged(int value)
{
	setColorGreen(value);
}

void ColorEditWidget::on_horizontalSlider_rgb_b_valueChanged(int value)
{
	setColorBlue(value);
}

void ColorEditWidget::on_spinBox_hsv_h_valueChanged(int arg1)
{
	setColorHue(arg1);
}

void ColorEditWidget::on_spinBox_hsv_s_valueChanged(int arg1)
{
	setColorSaturation(arg1);
}

void ColorEditWidget::on_spinBox_hsv_v_valueChanged(int arg1)
{
	setColorValue(arg1);
}

void ColorEditWidget::on_spinBox_rgb_r_valueChanged(int arg1)
{
	setColorRed(arg1);
}

void ColorEditWidget::on_spinBox_rgb_g_valueChanged(int arg1)
{
	setColorGreen(arg1);
}

void ColorEditWidget::on_spinBox_rgb_b_valueChanged(int arg1)
{
	setColorBlue(arg1);
}
