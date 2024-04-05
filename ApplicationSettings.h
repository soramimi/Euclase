#ifndef APPLICATIONSETTINGS_H
#define APPLICATIONSETTINGS_H

#define ORGANIZATION_NAME "soramimi.jp"
#define APPLICATION_NAME "Guitar"

class ApplicationSettings {
public:
	
	bool remember_and_restore_window_position = false;
	
	static ApplicationSettings defaultSettings();
	static void loadSettings(ApplicationSettings *as);
	static void saveSettings(const ApplicationSettings *as);
	
	void load()
	{
		loadSettings(this);
	}
	
	void save() const
	{
		saveSettings(this);
	}
};

#endif // APPLICATIONSETTINGS_H
