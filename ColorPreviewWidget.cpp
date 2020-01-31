#include "ColorPreviewWidget.h"
#include "misc.h"
#include <QPainter>

ColorPreviewWidget::ColorPreviewWidget(QWidget *parent)
	: QWidget(parent)
{
	primary_color_ = Qt::black;
	secondary_color_ = Qt::white;
}

void ColorPreviewWidget::paintEvent(QPaintEvent *)
{
	QPainter pr(this);

	auto DrawRect = [&](QRect const &r, QColor const &color){
		pr.fillRect(r, color);
		misc::drawFrame(&pr, r.x(), r.y(), r.width(), r.height(), Qt::black, Qt::black);
	};

	DrawRect(rect(), Qt::white);
	DrawRect(rect().adjusted(12, 12, -3, -3), secondary_color_);
	DrawRect(rect().adjusted(3, 3, -12, -12), primary_color_);
}

void ColorPreviewWidget::setColor(QColor const &primary_color, QColor const &secondery_color)
{
	primary_color_ = primary_color;
	secondary_color_ = secondery_color;
	update();
}
