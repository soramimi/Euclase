#include "TransparentCheckerBrush.h"
#include <stdint.h>


QBrush TransparentCheckerBrush::brush()
{
	static QBrush brush;
	if (brush.style() == Qt::NoBrush) {
		QImage img(16, 16, QImage::Format_Grayscale8);
		for (int y = 0; y < 16; y++) {
			uint8_t *p = img.scanLine(y);
			for (int x = 0; x < 16; x++) {
				p[x] = ((x ^ y) & 8) ? 240 : 192;

			}
		}
		brush = QBrush(img);
	}
	return brush;
}
