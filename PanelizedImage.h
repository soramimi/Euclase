#ifndef PANELIZEDIMAGE_H
#define PANELIZEDIMAGE_H

#include <QImage>
#include "misc.h"


class PanelizedImage {
	friend class ImageViewWidget;
private:
	class Panel {
	public:
		QPoint offset;
		QImage image;
		Panel() = default;
		Panel(QPoint const &offset, QImage const &image)
			: offset(offset)
			, image(image)
		{
		}
		bool operator == (Panel const &t) const
		{
			return offset == t.offset;
		}
		bool operator < (Panel const &t) const
		{
			return misc::compareQPoint(offset, t.offset) < 0;
		}
	};
	static constexpr int PANEL_SIZE = 256; // must be power of two
	QPoint offset_;
	QImage::Format format_ = QImage::Format_RGBA8888;
	std::vector<Panel> panels_;
	static Panel *findPanel_(std::vector<Panel> const *panels, QPoint const &offset);
public:
	void setImageMode(QImage::Format format)
	{
		format_ = format;
	}
	void setOffset(QPoint const &offset)
	{
		offset_ = offset;
	}
	QPoint offset() const
	{
		return offset_;
	}
	void clear()
	{
		panels_.clear();
	}
	void paintImage(QPoint const &dstpos, QImage const &srcimg, const QRect &scaled_rect, QRect const &srcrect);
	void renderImage(QPainter *painter, QPoint const &dstpos, QRect const &srcrect) const;
};

#endif // PANELIZEDIMAGE_H
