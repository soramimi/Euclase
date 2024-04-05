#include "ApplicationSettings.h"
#include "MySettings.h"

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

ApplicationSettings ApplicationSettings::defaultSettings()
{
	return {};
}

void ApplicationSettings::loadSettings(ApplicationSettings *as)
{
	MySettings s;
	
	*as = ApplicationSettings::defaultSettings();
	
	s.beginGroup("Global");
	GetValue<bool>(s, "SaveWindowPosition")                  >> as->remember_and_restore_window_position;
	s.endGroup();
}

void ApplicationSettings::saveSettings(ApplicationSettings const *as)
{
	MySettings s;
	
	s.beginGroup("Global");
	SetValue<bool>(s, "SaveWindowPosition")                  << as->remember_and_restore_window_position;
	s.endGroup();
}


static void loadSettings(ApplicationSettings *as);
static void saveSettings(const ApplicationSettings *as);
