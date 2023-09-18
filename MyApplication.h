#ifndef MYAPPLICATION_H
#define MYAPPLICATION_H

#include <QApplication>

class MyApplication : public QApplication {
public:
	MyApplication(int &argc, char **argv, int = ApplicationFlags);
};

extern MyApplication *theApp;

#endif // MYAPPLICATION_H
