#ifndef COLORPOINTER_H
#define COLORPOINTER_H

#include <QColor>

class QPainter;

namespace ColorPointer {

enum Dir {
	Up = 0,
	Left = -90,
	Down = 180,
	Right = 90
};

}

void drawColorPointer(QPainter *pr, int x, int y, int r, ColorPointer::Dir dir, QColor color);

#endif // COLORPOINTER_H
