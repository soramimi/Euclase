#ifndef APPLICATIONGLOBAL_H
#define APPLICATIONGLOBAL_H

#include <QString>

#include "libEuclaseCUDA/libeuclasecuda.h"

class ApplicationGlobal {
public:
	QString organization_name;
	QString application_name;
	QString generic_config_dir;
	QString app_config_dir;
	QString config_file_path;

	double device_pixel_ratio = 1.0;

	QString open_file_dir;

	CUDAIMAGE_API const *cuda = nullptr;

	ApplicationGlobal();
};

extern ApplicationGlobal *global;

#endif // APPLICATIONGLOBAL_H
