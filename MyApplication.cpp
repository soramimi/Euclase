#include "MyApplication.h"

MyApplication *theApp;


MyApplication::MyApplication(int &argc, char **argv, int flags)
	: QApplication(argc, argv, flags)
{
	theApp = this;
}
