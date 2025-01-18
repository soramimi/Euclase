#include "MyToolButton.h"



MyToolButton::MyToolButton(QWidget *parent)
	: QToolButton(parent)
{
	connect(this, &QToolButton::clicked, [this]() {
		emit clicked(this);
	});
}
