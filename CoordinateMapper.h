#ifndef COORDINATEMAPPER_H
#define COORDINATEMAPPER_H

#include <QPointF>
#include <QSize>

class CoordinateMapper {
private:
	QSize viewport_size_;
	QPointF scroll_offset;
	double scale_ = 1;
public:
	CoordinateMapper() = default;
	CoordinateMapper(QSize const &viewport_size, QPointF const &scroll_offset, double scale);
	QSize viewport_size() const
	{
		return viewport_size_;
	}
	QPointF scrollOffset() const
	{
		return scroll_offset;
	}
	double scale() const
	{
		return scale_;
	}
	QPointF mapToCanvasFromViewport(QPointF const &pos);
	QPointF mapToViewportFromCanvas(QPointF const &pos);
};

#endif // COORDINATEMAPPER_H
