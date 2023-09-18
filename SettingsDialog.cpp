#include "SettingsDialog.h"
#include "ui_SettingsDialog.h"
#include "MySettings.h"
#include "main.h"

#include <QFileDialog>
//#include "misc.h"

static int page_number = 0;

SettingsDialog::SettingsDialog(MainWindow *parent) :
	QDialog(parent),
	ui(new Ui::SettingsDialog)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	mainwindow_ = parent;

	loadSettings();

	QTreeWidgetItem *item;

	auto AddPage = [&](QWidget *page){
		page->layout()->setContentsMargins(0, 0, 0, 0);
		QString name = page->windowTitle();
		item = new QTreeWidgetItem();
		item->setText(0, name);
		item->setData(0, Qt::UserRole, QVariant::fromValue((uintptr_t)(QWidget *)page));
		ui->treeWidget->addTopLevelItem(item);
	};
	AddPage(ui->page_general);

	ui->treeWidget->setCurrentItem(ui->treeWidget->topLevelItem(page_number));
}

SettingsDialog::~SettingsDialog()
{
	delete ui;
}

namespace {

template <typename T> class GetValue {
private:
public:
	MySettings &settings;
	QString name;
	GetValue(MySettings &s, QString const &name)
		: settings(s)
		, name(name)
	{
	}
	void operator >> (T &value)
	{
		value = settings.value(name, value).template value<T>();
	}
};

template <typename T> class SetValue {
private:
public:
	MySettings &settings;
	QString name;
	SetValue(MySettings &s, QString const &name)
		: settings(s)
		, name(name)
	{
	}
	void operator << (T const &value)
	{
		settings.setValue(name, value);
	}
};

} // namespace

void SettingsDialog::loadSettings(ApplicationSettings *as)
{
	MySettings s;

	*as = ApplicationSettings::defaultSettings();

	s.beginGroup("Global");
	GetValue<bool>(s, "SaveWindowPosition")                  >> as->remember_and_restore_window_position;
	s.endGroup();

	s.beginGroup("UI");
	GetValue<bool>(s, "EnableHighDpiScaling")                >> as->enable_high_dpi_scaling;
	s.endGroup();
}

void SettingsDialog::saveSettings(ApplicationSettings const *as)
{
	MySettings s;

	s.beginGroup("Global");
	SetValue<bool>(s, "SaveWindowPosition")                  << as->remember_and_restore_window_position;
	s.endGroup();

	s.beginGroup("UI");
	SetValue<bool>(s, "EnableHighDpiScaling")                << as->enable_high_dpi_scaling;
	s.endGroup();
}

void SettingsDialog::saveSettings()
{
	saveSettings(&set);
}

void SettingsDialog::exchange(bool save)
{
	QList<AbstractSettingForm *> forms = ui->stackedWidget->findChildren<AbstractSettingForm *>();
	for (AbstractSettingForm *form : forms) {
		form->exchange(save);
	}
}

void SettingsDialog::loadSettings()
{
	loadSettings(&set);
	exchange(false);
}

void SettingsDialog::done(int r)
{
	page_number = ui->treeWidget->currentIndex().row();
	QDialog::done(r);
}

void SettingsDialog::accept()
{
	exchange(true);
	saveSettings();
	done(QDialog::Accepted);
}

void SettingsDialog::on_treeWidget_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous)
{
	(void)previous;
	if (current) {
		uintptr_t p = current->data(0, Qt::UserRole).value<uintptr_t>();
		QWidget *w = reinterpret_cast<QWidget *>(p);
		Q_ASSERT(w);
		ui->stackedWidget->setCurrentWidget(w);
	}
}

