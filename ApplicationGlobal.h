#ifndef APPLICATIONGLOBAL_H
#define APPLICATIONGLOBAL_H

#include <QString>



class ApplicationGlobal {
public:
	QString organization_name;
	QString application_name;
	QString generic_config_dir;
	QString app_config_dir;
	QString config_file_path;

	QString open_file_dir;

	ApplicationGlobal();
};

extern ApplicationGlobal *global;

#endif // APPLICATIONGLOBAL_H
