#include "ColorPointer.h"

#include <QPainter>
#include <QPainterPath>

void drawColorPointer(QPainter *pr, int x, int y, int r, ColorPointer::Dir dir, QColor color)
{
	QPainterPath path;
	{
		constexpr double sqrt2 = 1.4142135623730950488016887242097;
		const double r2 = r * sqrt2;
		path.moveTo(0, 0);
		path.lineTo(r2 / 2, r2 / 2);
		path.arcTo(-r, r2 - r, r * 2, r * 2, 45, -270);
		path.lineTo(0, 0);
	}
	
	pr->save();
	
	pr->setRenderHint(QPainter::Antialiasing);
	
	QTransform t;
	t.translate(x, y);
	
	t.rotate(dir);
	pr->setTransform(t);
	
	pr->fillPath(path, color);
	pr->setPen(QPen(Qt::black, 3.5));
	pr->drawPath(path);
	pr->setPen(QPen(Qt::white, 1.5));
	pr->drawPath(path);
	
	pr->restore();
}
