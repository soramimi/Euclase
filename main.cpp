#include "MainWindow.h"
#include "MyApplication.h"
#include "main.h"
#include "MiraCL.h"
#include "AlphaBlend.h"
#include "SelectionOutlineRenderer.h"
#include "ImageViewRenderer.h"
#include "MySettings.h"
#include "ApplicationGlobal.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include "joinpath.h"

ApplicationSettings ApplicationSettings::defaultSettings()
{
	ApplicationSettings s;
	return s;
}

static bool isHighDpiScalingEnabled()
{
	MySettings s;
	s.beginGroup("UI");
	QVariant v = s.value("EnableHighDpiScaling");
	return !v.isNull() && v.toBool();
}

int main(int argc, char *argv[])
{
	ApplicationGlobal g;
	global = &g;

	MyApplication a(argc, argv);

	global->organization_name = "soramimi.jp";
	global->application_name = "Euclase";
	global->generic_config_dir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
	global->app_config_dir = global->generic_config_dir / global->organization_name / global->application_name;
	global->config_file_path = joinpath(global->app_config_dir, global->application_name + ".ini");
	if (!QFileInfo(global->app_config_dir).isDir()) {
		QDir().mkpath(global->app_config_dir);
	}

#if (QT_VERSION < QT_VERSION_CHECK(5, 6, 0))
	qDebug() << "High DPI scaling is not supported";
#else
	if (isHighDpiScalingEnabled()) {
		QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
	} else {
		QCoreApplication::setAttribute(Qt::AA_DisableHighDpiScaling);
	}
	QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

#if USE_OPENCL
	getCL()->open();
#endif

	qRegisterMetaType<RenderedImage>("RenderedImage");
	qRegisterMetaType<SelectionOutlineBitmap>("SelectionOutlineBitmap");

	MainWindow w;
	w.show();

	return a.exec();
}

#if USE_OPENCL
MiraCL *getCL()
{
	return theApp->getCL();
}
#endif


