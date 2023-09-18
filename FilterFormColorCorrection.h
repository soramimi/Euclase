#ifndef FILTERFORMCOLORCORRECTION_H
#define FILTERFORMCOLORCORRECTION_H

#include "AbstractFilterForm.h"

#include <QDialog>

namespace Ui {
class FilterFormColorCorrection;
}

class FilterFormColorCorrection : public AbstractFilterForm {
	Q_OBJECT
public:
	explicit FilterFormColorCorrection(QWidget *parent = nullptr);
	~FilterFormColorCorrection();
	void start();
private slots:
	void on_horizontalSlider_hue_valueChanged(int hue);
	void on_horizontalSlider_saturation_valueChanged(int sat);
	void on_horizontalSlider_brightness_valueChanged(int bri);
	void on_spinBox_hue_valueChanged(int hue);
	void on_spinBox_saturation_valueChanged(int arg1);
	void on_spinBox_brightness_valueChanged(int bri);
private:
	Ui::FilterFormColorCorrection *ui;
	void setHue(int hue);
	void setSaturation(int sat);
	void setBrightness(int brightness);
};

#endif // FILTERFORMCOLORCORRECTION_H
