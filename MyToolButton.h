#ifndef MYTOOLBUTTON_H
#define MYTOOLBUTTON_H

#include <QToolButton>

class MyToolButton : public QToolButton {
	Q_OBJECT
public:
	MyToolButton(QWidget *parent = nullptr);
	virtual ~MyToolButton() = default;
signals:
	void clicked(MyToolButton *self);
};

#endif // MYTOOLBUTTON_H
