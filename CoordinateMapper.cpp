#include "CoordinateMapper.h"

/**
 * @brief CoordinateMapper::CoordinateMapper
 * @param viewport_size
 * @param scroll_offset
 * @param scale
 *
 * This is the constructor for the CoordinateMapper class.
 *
 * @return
 */
CoordinateMapper::CoordinateMapper(const QSize &viewport_size, const QPointF &scroll_offset, double scale)
	: viewport_size_(viewport_size)
	, scroll_offset(scroll_offset)
	, scale_(scale)
{
}

/**
 * @brief CoordinateMapper::mapToCanvasFromViewport
 * @param pos
 * @return
 *
 * This function maps the coordinates of the viewport to the canvas.
 *
 * @return
 */
QPointF CoordinateMapper::mapToCanvasFromViewport(const QPointF &pos) const
{
	double cx = viewport_size_.width() / 2.0;
	double cy = viewport_size_.height() / 2.0;
	double x = (pos.x() - cx + scroll_offset.x()) / scale_;
	double y = (pos.y() - cy + scroll_offset.y()) / scale_;
	return QPointF(x, y);
}

/**
 * @brief CoordinateMapper::mapToViewportFromCanvas
 * @param pos
 * @return
 *
 * This function maps the coordinates of the canvas to the viewport.
 *
 * @return
 */
QPointF CoordinateMapper::mapToViewportFromCanvas(const QPointF &pos) const
{
	double cx = viewport_size_.width() / 2.0;
	double cy = viewport_size_.height() / 2.0;
	double x = pos.x() * scale_ + cx - scroll_offset.x();
	double y = pos.y() * scale_ + cy - scroll_offset.y();
	return QPointF(x, y);
}
