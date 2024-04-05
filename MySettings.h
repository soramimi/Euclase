#ifndef MYSETTINGS_H
#define MYSETTINGS_H

#include <QSettings>

class ApplicationSettings;

class MySettings : public QSettings {
	Q_OBJECT
public:
	explicit MySettings(QObject *parent = 0);
};

QString makeApplicationDataDir();

#endif // MYSETTINGS_H
