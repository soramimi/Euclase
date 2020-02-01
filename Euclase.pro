
QT       += core gui widgets svg

TARGET = Euclase
TEMPLATE = app
CONFIG += c++11

DESTDIR = $$PWD/_bin

unix:QMAKE_CXXFLAGS += -fopenmp
unix:QMAKE_LFLAGS += -fopenmp


#INCLUDEPATH += "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v7.5/include"
#LIBS += "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v7.5/lib/Win32/OpenCL.lib"

SOURCES += main.cpp\
	AlphaBlend.cpp \
    BrushSlider.cpp \
	ColorPreviewWidget.cpp \
	Document.cpp \
	ImageViewRenderer.cpp \
        MainWindow.cpp \
    BrushPreviewWidget.cpp \
    MiraCL.cpp \
    MyWidget.cpp \
    MyApplication.cpp \
    HueWidget.cpp \
	NewDialog.cpp \
	RingSlider.cpp \
    SaturationBrightnessWidget.cpp \
	SelectionOutlineRenderer.cpp \
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
    ColorSlider.cpp

HEADERS  += MainWindow.h \
    AlphaBlend.h \
    BrushPreviewWidget.h \
    BrushSlider.h \
    ColorPreviewWidget.h \
    Document.h \
    ImageViewRenderer.h \
    MiraCL.h \
    MyWidget.h \
    NewDialog.h \
    RingSlider.h \
    SelectionOutlineRenderer.h \
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
    ColorSlider.h

FORMS    += MainWindow.ui \
    NewDialog.ui \
    ResizeDialog.ui

RESOURCES += \
    resources.qrc
