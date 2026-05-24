QMAKE_PROJECT_DEPTH = 0

TARGET = Euclase
TEMPLATE = app
CONFIG += c++17
QT += core gui widgets svg

macx:QMAKE_CXX = aarch64-apple-darwin23-g++-14
macx:QMAKE_LINK = aarch64-apple-darwin23-g++-14

DEFINES += USE_EUCLASE_IMAGE_READ_WRITE

CONFIG += nostrip debug_info

CPP_STD = c++17
gcc:QMAKE_CXXFLAGS += -std=$$CPP_STD -Wall -Wextra -Werror=return-type -Werror=trigraphs -Wno-switch -Wno-reorder -Wno-unknown-pragmas

DESTDIR = $$PWD/_bin

DEFINES += USE_QT
DEFINES += USE_CUDA

#win32:LIBS += -lkernel32.lib
!win32:LIBS += -ldl

# macx:LIBS += -L/opt/homebrew/lib -L/opt/homebrew/opt/jpeg/lib

unix:QMAKE_CXXFLAGS += -fopenmp
#-msse4.1
unix:QMAKE_LFLAGS += -fopenmp
msvc:QMAKE_CXXFLAGS += /openmp

msvc {
	DEFINES += _USE_MATH_DEFINES=1
	INCLUDEPATH += C:/vcpkg/installed/x64-windows/include
	LIBS += -LC:/vcpkg/installed/x64-windows/lib -llibpng16 -ljpeg
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
	ApplicationSettings.cpp \
	Bounds.cpp \
	BrushPreviewWidget.cpp \
	BrushSlider.cpp \
	Canvas.cpp \
	ColorDialog.cpp \
	ColorEditWidget.cpp \
	ColorPreviewWidget.cpp \
	ColorSlider.cpp \
	CoordinateMapper.cpp \
	Document.cpp \
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
	MyToolButton.cpp \
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
	fp/fp.cpp \
	joinpath.cpp \
	median.cpp \
	misc.cpp \
	xbrz/xbrz.cpp

HEADERS += \
	AbstractFilterForm.h \
	AbstractSettingForm.h \
	AlphaBlend.h \
	ApplicationGlobal.h \
	ApplicationSettings.h \
	Bounds.h \
	BrushPreviewWidget.h \
	BrushSlider.h \
	Canvas.h \
	ColorDialog.h \
	ColorEditWidget.h \
	ColorPreviewWidget.h \
	ColorSlider.h \
	CoordinateMapper.h \
	Document.h \
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
	MyToolButton.h \
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
	TransparentCheckerBrush.h \
	antialias.h \
	charvec.h \
	euclase.h \
	fp/f16c.h \
	fp/fp.h \
	joinpath.h \
	libEuclaseCUDA/libeuclasecuda.h \
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
