#include "PanelizedImage.h"

#include <QPainter>



PanelizedImage::Panel *PanelizedImage::findPanel_(const std::vector<Panel> *panels, const QPoint &offset)
{
	int lo = 0;
	int hi = (int)panels->size(); // 上限の一つ後
	while (lo < hi) {
		int m = (lo + hi) / 2;
		Panel const *p = &panels->at(m);
		Q_ASSERT(p);
		int i = misc::compareQPoint(p->offset, offset);
		if (i == 0) return const_cast<Panel *>(p);
		if (i < 0) {
			lo = m + 1;
		} else {
			hi = m;
		}
	}
	return nullptr;
}

void PanelizedImage::paintImage(const QPoint &dstpos, const QImage &srcimg, const QRect &srcrect)
{
	int panel_dx0 = dstpos.x() - offset_.x();
	int panel_dy0 = dstpos.y() - offset_.y();
	int panel_dx1 = panel_dx0 + srcimg.width() - 1;
	int panel_dy1 = panel_dy0 + srcimg.height() - 1;
	const int S1 = PANEL_SIZE - 1;
	panel_dx0 = panel_dx0 & ~S1;
	panel_dy0 = panel_dy0 & ~S1;
	panel_dx1 = (panel_dx1 + S1) & ~S1;
	panel_dy1 = (panel_dy1 + S1) & ~S1;
	std::vector<Panel> newpanels;
	for (int y = panel_dy0; y <= panel_dy1; y += PANEL_SIZE) {
		for (int x = panel_dx0; x <= panel_dx1; x += PANEL_SIZE) {
			int w = srcimg.width();
			int h = srcimg.height();
			int sx0 = srcrect.x();
			int sy0 = srcrect.y();
			int sx1 = sx0 + srcrect.width();
			int sy1 = sy0 + srcrect.height();
			int dx0 = dstpos.x() - (offset_.x() + x);
			int dy0 = dstpos.y() - (offset_.y() + y);
			int dx1 = dx0 + srcrect.width();
			int dy1 = dy0 + srcrect.width();
			if (sx0 < 0) { dx0 -= sx0; sx0 = 0; }
			if (sy0 < 0) { dy0 -= sy0; sy0 = 0; }
			if (dx0 < 0) { sx0 -= dx0; dx0 = 0; }
			if (dy0 < 0) { sy0 -= dy0; dy0 = 0; }
			if (sx1 > w) { dx1 -= sx1 - w; sx1 = w; }
			if (sy1 > h) { dy1 -= sy1 - h; sy1 = h; }
			w = sx1 - sx0;
			h = sy1 - sy0;
			if (w > 0 && h > 0) {
				Panel *dst = findPanel_(&panels_, {x, y});
				if (!dst) {
					newpanels.push_back({{x, y}, QImage(PANEL_SIZE, PANEL_SIZE, format_)});
					dst = &newpanels.back();
					dst->image.fill(Qt::transparent);
				}
				QPainter pr(&dst->image);
				pr.drawImage(dx0, dy0, srcimg, sx0, sy0, w, h);
			}
		}
	}
	panels_.insert(panels_.end(), newpanels.begin(), newpanels.end());
	std::sort(panels_.begin(), panels_.end(), [&](Panel const &a, Panel const &b){
		return misc::compareQPoint(a.offset, b.offset) < 0;
	});
}

void PanelizedImage::renderImage(QPainter *painter, const QPoint &dstpos, const QRect &srcrect) const
{
	for (Panel const &panel : panels_) {
		QRect r(0, 0, PANEL_SIZE, PANEL_SIZE);
		int x = offset().x() + panel.offset.x();
		int y = offset().y() + panel.offset.y();
		r = r.intersected(srcrect.translated(-x, -y));
		if (r.width() > 0 && r.height() > 0) {
			painter->drawImage(dstpos.x() + x + r.x(), dstpos.y() + y + r.y(), panel.image, r.x(), r.y(), r.width(), r.height());
		}
	}
}
