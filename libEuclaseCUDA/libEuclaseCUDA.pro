
TEMPLATE = lib
win32:TARGET = libEuclaseCUDA
!win32:TARGET = EuclaseCUDA

DESTDIR = $$PWD/../_bin

win32 {
    libeuclasecuda.target = libeuclasecuda
    libeuclasecuda.commands = cd $$PWD && winmake.bat
    QMAKE_EXTRA_TARGETS += libeuclasecuda
    PRE_TARGETDEPS += libeuclasecuda
	LIBS += $$PWD/libeuclasecuda.obj
	LIBS += -L"C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.1/lib/x64"
}
!win32 {
    libeuclasecuda.target = libeuclasecuda
    libeuclasecuda.commands = cd $$PWD && make libeuclasecuda.o
    QMAKE_EXTRA_TARGETS += libeuclasecuda
    PRE_TARGETDEPS += libeuclasecuda
	LIBS += $$PWD/libeuclasecuda.o
	LIBS += -L/opt/cuda/targets/x86_64-linux/lib
}

LIBS += -lcudart

HEADERS += libeuclasecuda.h

DISTFILES += \
	libeuclasecuda.cu

