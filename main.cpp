
#include "ApplicationGlobal.h"
#include "ImageViewRenderingThread.h"
#include "MainWindow.h"
#include "MyApplication.h"
#include "SelectionOutline.h"
#include "joinpath.h"
#include "main.h"
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QPluginLoader>
#include <QStandardPaths>
#include <QScreen>

#ifdef Q_OS_WIN
#include <Windows.h>
#else
#include <dlfcn.h>
#endif

ApplicationSettings ApplicationSettings::defaultSettings()
{
	return {};
}

int main(int argc, char *argv[])
{
	putenv("QT_ASSUME_STDERR_HAS_CONSOLE=1");

#ifdef Q_OS_WIN
	// WindowsのQt6では↓が動いてなさそう
//	QApplication::setAttribute(Qt::AA_EnableHighDpiScaling, false);
//	QApplication::setAttribute(Qt::AA_DisableHighDpiScaling, true);
	QApplication::setAttribute(Qt::AA_DisableHighDpiScaling);
	// ↓これだといける
	putenv("QT_ENABLE_HIGHDPI_SCALING=0");
#endif

	ApplicationGlobal g;
	global = &g;

	global->organization_name = "soramimi.jp";
	global->application_name = "Euclase";
	global->generic_config_dir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
	global->app_config_dir = global->generic_config_dir / global->organization_name / global->application_name;
	global->config_file_path = joinpath(global->app_config_dir, global->application_name + ".ini");
	if (!QFileInfo(global->app_config_dir).isDir()) {
		QDir().mkpath(global->app_config_dir);
	}

	MyApplication a(argc, argv);

#if 0//def USE_CUDA
#ifdef Q_OS_WIN
	if (1) {
		QString path = a.applicationDirPath() / "libEuclaseCUDA.dll";
		HMODULE dll = LoadLibraryA(path.toStdString().c_str());
		CUDAIMAGE_API const *(*init_cudaplugin)(int);
		*(void **)&init_cudaplugin = GetProcAddress(dll, "init_cudaplugin");
		if (init_cudaplugin) {
			global->cuda = init_cudaplugin(sizeof(CUDAIMAGE_API));
		}
	}
#else
	if (1) {
		QString path = a.applicationDirPath() / "libEuclaseCUDA.so";
		void *so = dlopen(path.toStdString().c_str(), RTLD_NOW);
		CUDAIMAGE_API const *(*init_cudaplugin)(int);
		*(void **)&init_cudaplugin = dlsym(so, "init_cudaplugin");
		if (init_cudaplugin) {
			global->cuda = init_cudaplugin(sizeof(CUDAIMAGE_API));
		}
	}
#endif
	if (global->cuda) {
		void (**fns)() = (void (**)())global->cuda;
		for (int i = 0; i < sizeof(CUDAIMAGE_API) / sizeof(void (*)()); i++) {
			if (!fns[i]) {
				global->cuda = nullptr;
				break;
			}
		}
	}
	if (!global->cuda) {
		qDebug() << "Could not use CUDA.";
	}
#endif

	qRegisterMetaType<RenderedData>("RenderedData");

	MainWindow w;
	w.setWindowIcon(QIcon(":/image/icon.png"));
	w.show();

	return a.exec();
}

