#ifndef MAIN_H
#define MAIN_H

#include <QString>

class MiraCL;

MiraCL *getCL();



class ApplicationSettings {
public:
	bool remember_and_restore_window_position = false;
	bool enable_high_dpi_scaling = false;
	static ApplicationSettings defaultSettings();
};


#endif // MAIN_H
