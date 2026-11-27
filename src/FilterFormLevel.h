#ifndef FilterFormLevel_H
#define FilterFormLevel_H

#include "../AbstractFilterForm.h"
#include <QWidget>

namespace Ui {
class FilterFormLevel;
}

class FilterFormLevel : public AbstractFilterForm {
	Q_OBJECT
private:
	Ui::FilterFormLevel *ui;
public:
	explicit FilterFormLevel(QWidget *parent = nullptr);
	~FilterFormLevel();
	void start() override;
private slots:
	void on_horizontalSlider_valueChanged(int value);
};

#endif // FilterFormLevel_H
