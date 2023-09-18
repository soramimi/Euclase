#include "MySettings.h"
#include "ApplicationGlobal.h"
#include "SettingGeneralForm.h"
#include "ui_SettingGeneralForm.h"

#include <QFileDialog>

SettingGeneralForm::SettingGeneralForm(QWidget *parent) :
	AbstractSettingForm(parent),
	ui(new Ui::SettingGeneralForm)
{
	ui->setupUi(this);
ui->checkBox_save_window_pos->setVisible(false);
}

SettingGeneralForm::~SettingGeneralForm()
{
	delete ui;
}

void SettingGeneralForm::exchange(bool save)
{
	if (save) {
		settings()->remember_and_restore_window_position = ui->checkBox_save_window_pos->isChecked();
		settings()->enable_high_dpi_scaling = ui->checkBox_enable_high_dpi_scaling->isChecked();
	} else {
		ui->checkBox_save_window_pos->setChecked(settings()->remember_and_restore_window_position);
		ui->checkBox_enable_high_dpi_scaling->setChecked(settings()->enable_high_dpi_scaling);
	}
}

