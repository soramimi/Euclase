#ifndef FILTERFORMBLUR_H
#define FILTERFORMBLUR_H

#include "AbstractFilterForm.h"
#include <QWidget>

namespace Ui {
class FilterFormBlur;
}

class FilterFormBlur : public AbstractFilterForm {
	Q_OBJECT
private:
	Ui::FilterFormBlur *ui;
public:
	explicit FilterFormBlur(QWidget *parent = nullptr);
	~FilterFormBlur();
	void start() override;
private slots:
	void on_horizontalSlider_valueChanged(int value);
};

#endif // FILTERFORMBLUR_H
