#ifndef SELECTIONOUTLINE_H
#define SELECTIONOUTLINE_H

#include <QBitmap>

class SelectionOutline {
public:
	SelectionOutline() = default;
	SelectionOutline(QPoint point, QBitmap const &bitmap)
		: point(point)
		, bitmap(bitmap)
	{
	}
	QPoint point;
	QBitmap bitmap;
};
Q_DECLARE_METATYPE(SelectionOutline)

#endif // SELECTIONOUTLINE_H
