#ifndef FILTERDIALOG_H
#define FILTERDIALOG_H

#include <QDialog>
#include <functional>
#include "euclase.h"

class MainWindow;

namespace Ui {
class FilterDialog;
}

class FilterDialog : public QDialog {
	Q_OBJECT
private:
	Ui::FilterDialog *ui;
	struct Private;
	Private *m;
	MainWindow *mainwindow;
public:
	explicit FilterDialog(MainWindow *parent, euclase::Image const &image, std::function<euclase::Image (euclase::Image const &, int param, bool *cancel_request)> const &fn);
	~FilterDialog();
	euclase::Image result();
private slots:
	void on_horizontalSlider_valueChanged(int value);
};

#endif // FILTERDIALOG_H
