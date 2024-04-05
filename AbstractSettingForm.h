#ifndef ABSTRACTSETTINGFORM_H
#define ABSTRACTSETTINGFORM_H

#include <QWidget>
#include "ApplicationSettings.h"
#include "SettingsDialog.h"

class MainWindow;

class AbstractSettingForm : public QWidget {
	Q_OBJECT
	friend class SettingsDialog;
protected:
	MainWindow *mainwindow();
	ApplicationSettings *settings();
public:
	AbstractSettingForm(QWidget *parent = 0);
protected:
	virtual void exchange(bool save) = 0;
};

#endif // ABSTRACTSETTINGFORM_H
