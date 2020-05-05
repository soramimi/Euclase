
QT       += core gui widgets svg

TARGET = Euclase
TEMPLATE = app
CONFIG += c++14

DESTDIR = $$PWD/_bin

unix:QMAKE_CXXFLAGS += -fopenmp
unix:QMAKE_LFLAGS += -fopenmp
msvc:QMAKE_CXXFLAGS += /openmp


#INCLUDEPATH += "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v7.5/include"
#LIBS += "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v7.5/lib/Win32/OpenCL.lib"

SOURCES += main.cpp\
	AbstractSettingForm.cpp \
	AlphaBlend.cpp \
	ApplicationGlobal.cpp \
    BrushSlider.cpp \
	ColorPreviewWidget.cpp \
	Document.cpp \
	FilterDialog.cpp \
	FilterThread.cpp \
	ImageViewRenderer.cpp \
        MainWindow.cpp \
    BrushPreviewWidget.cpp \
    MiraCL.cpp \
	MySettings.cpp \
    MyWidget.cpp \
    MyApplication.cpp \
    HueWidget.cpp \
	NewDialog.cpp \
	RingSlider.cpp \
    SaturationBrightnessWidget.cpp \
	SelectionOutlineRenderer.cpp \
	SettingGeneralForm.cpp \
	SettingsDialog.cpp \
	TransparentCheckerBrush.cpp \
	antialias.cpp \
	euclase.cpp \
	median.cpp \
    misc.cpp \
    RoundBrushGenerator.cpp \
    ResizeDialog.cpp \
    ImageViewWidget.cpp \
    MemoryReader.cpp \
    Photoshop.cpp \
    charvec.cpp \
    joinpath.cpp \
	resize.cpp \
    ColorSlider.cpp \
	xbrz/xbrz.cpp

HEADERS  += MainWindow.h \
    AbstractSettingForm.h \
    AlphaBlend.h \
    ApplicationGlobal.h \
    BrushPreviewWidget.h \
    BrushSlider.h \
    ColorPreviewWidget.h \
    Document.h \
    FilterDialog.h \
    FilterThread.h \
    ImageViewRenderer.h \
    MiraCL.h \
    MySettings.h \
    MyWidget.h \
    NewDialog.h \
    RingSlider.h \
    SelectionOutlineRenderer.h \
    SettingGeneralForm.h \
    SettingsDialog.h \
    TransparentCheckerBrush.h \
    antialias.h \
    euclase.h \
    main.h \
    MyApplication.h \
    HueWidget.h \
    SaturationBrightnessWidget.h \
    median.h \
    misc.h \
    RoundBrushGenerator.h \
    ResizeDialog.h \
    ImageViewWidget.h \
    MemoryReader.h \
    Photoshop.h \
    charvec.h \
    joinpath.h \
    resize.h \
    ColorSlider.h \
    xbrz/xbrz.h \
    xbrz/xbrz_config.h \
    xbrz/xbrz_tools.h

FORMS    += MainWindow.ui \
    FilterDialog.ui \
    NewDialog.ui \
    ResizeDialog.ui \
    SettingGeneralForm.ui \
    SettingsDialog.ui

RESOURCES += \
    resources.qrc
