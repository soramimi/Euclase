#ifndef FILTERFORMMEDIAN_H
#define FILTERFORMMEDIAN_H

#include "AbstractFilterForm.h"
#include <QWidget>

namespace Ui {
class FilterFormMedian;
}

class FilterFormMedian : public AbstractFilterForm {
	Q_OBJECT
private:
	Ui::FilterFormMedian *ui;
public:
	explicit FilterFormMedian(QWidget *parent = nullptr);
	~FilterFormMedian();
	void start() override;
private slots:
	void on_horizontalSlider_valueChanged(int value);
};

#endif // FILTERFORMMEDIAN_H
