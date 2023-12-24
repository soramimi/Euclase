
TARGET = Euclase
TEMPLATE = app
CONFIG += c++17
QT += core gui widgets svg

DESTDIR = $$PWD/_bin

DEFINES += USE_QT=1
DEFINES += USE_CUDA=1

#win32:LIBS += -lkernel32.lib
!win32:LIBS += -ldl

unix:QMAKE_CXXFLAGS += -fopenmp -msse4.1
unix:QMAKE_LFLAGS += -fopenmp
msvc:QMAKE_CXXFLAGS += /openmp

win32 {
	DEFINES += _USE_MATH_DEFINES=1
	INCLUDEPATH += C:/develop/libimage/libpng/lpng1640
	INCLUDEPATH += C:/develop/libimage/libjpeg/jpeg-6b
	LIBS += C:/develop/libimage/libpng/out/x64/Release/libjpeg.lib
	LIBS += C:/develop/libimage/libjpeg/out/x64/Release/libjpeg.lib
	LIBS += C:/develop/libimage/libz/out/x64/Release/libz.lib
}
!win32 {
	LIBS += -lpng -ljpeg
}




#INCLUDEPATH += "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v7.5/include"

#win32 {
#	RC_FILE = windows/Euclase.rc
#}

SOURCES += main.cpp\
	AbstractFilterForm.cpp \
	AbstractSettingForm.cpp \
	AlphaBlend.cpp \
	ApplicationGlobal.cpp \
	BrushPreviewWidget.cpp \
	BrushSlider.cpp \
	Canvas.cpp \
	ColorDialog.cpp \
	ColorEditWidget.cpp \
	ColorPreviewWidget.cpp \
	ColorSlider.cpp \
	CoordinateMapper.cpp \
	FilterDialog.cpp \
	FilterFormBlur.cpp \
	FilterFormColorCorrection.cpp \
	FilterFormMedian.cpp \
	FilterStatus.cpp \
	HueWidget.cpp \
	ImageViewWidget.cpp \
	MainWindow.cpp \
	MemoryReader.cpp \
	MyApplication.cpp \
	MySettings.cpp \
	NewDialog.cpp \
	PanelizedImage.cpp \
	Photoshop.cpp \
	ResizeDialog.cpp \
	RingSlider.cpp \
	RoundBrushGenerator.cpp \
	SaturationBrightnessWidget.cpp \
	SelectionOutline.cpp \
	SettingGeneralForm.cpp \
	SettingsDialog.cpp \
	TransparentCheckerBrush.cpp \
	antialias.cpp \
	charvec.cpp \
	euclase.cpp \
	joinpath.cpp \
	median.cpp \
	misc.cpp \
	xbrz/xbrz.cpp

HEADERS += \
	AbstractFilterForm.h \
	AbstractSettingForm.h \
	AlphaBlend.h \
	ApplicationGlobal.h \
	BrushPreviewWidget.h \
	BrushSlider.h \
	Canvas.h \
	ColorDialog.h \
	ColorEditWidget.h \
	ColorPreviewWidget.h \
	ColorSlider.h \
	CoordinateMapper.h \
	FilterDialog.h \
	FilterFormBlur.h \
	FilterFormColorCorrection.h \
	FilterFormMedian.h \
	FilterStatus.h \
	HueWidget.h \
	ImageViewWidget.h \
	MainWindow.h \
	MemoryReader.h \
	MyApplication.h \
	MySettings.h \
	NewDialog.h \
	PanelizedImage.h \
	Photoshop.h \
	ResizeDialog.h \
	RingSlider.h \
	RoundBrushGenerator.h \
	SaturationBrightnessWidget.h \
	SelectionOutline.h \
	SettingGeneralForm.h \
	SettingsDialog.h \
	TestPlugin/src/TestInterface.h \
	TransparentCheckerBrush.h \
	antialias.h \
	charvec.h \
	euclase.h \
	joinpath.h \
	libEuclaseCUDA/libeuclasecuda.h \
	main.h \
	median.h \
	misc.h \
	uninitialized_vector.h \
	xbrz/xbrz.h \
	xbrz/xbrz_config.h \
	xbrz/xbrz_tools.h

FORMS += \
	ColorDialog.ui \
	ColorEditWidget.ui \
	FilterDialog.ui \
	FilterFormBlur.ui \
	FilterFormColorCorrection.ui \
	FilterFormMedian.ui \
	MainWindow.ui \
	NewDialog.ui \
	ResizeDialog.ui \
	SettingGeneralForm.ui \
	SettingsDialog.ui \

RESOURCES += \
	resources.qrc

DISTFILES += libEuclaseCUDA/libeuclasecuda.cu
