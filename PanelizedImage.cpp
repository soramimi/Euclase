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
	panel_dx0 = panel_dx0 - (panel_dx0 & (PANEL_SIZE - 1));
	panel_dy0 = panel_dy0 - (panel_dy0 & (PANEL_SIZE - 1));
	panel_dx1 = panel_dx1 - (panel_dx1 & (PANEL_SIZE - 1));
	panel_dy1 = panel_dy1 - (panel_dy1 & (PANEL_SIZE - 1));
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

void PanelizedImage::renderImage(QPainter *painter, const QPoint &offset, const QRect &srcrect) const
{
	int panel_sx0 = srcrect.x() - offset_.x();
	int panel_sy0 = srcrect.y() - offset_.y();
	int panel_sx1 = panel_sx0 + srcrect.width() - 1;
	int panel_sy1 = panel_sy0 + srcrect.height() - 1;
	panel_sx0 = panel_sx0 - (panel_sx0 & (PANEL_SIZE - 1));
	panel_sy0 = panel_sy0 - (panel_sy0 & (PANEL_SIZE - 1));
	panel_sx1 = panel_sx1 - (panel_sx1 & (PANEL_SIZE - 1));
	panel_sy1 = panel_sy1 - (panel_sy1 & (PANEL_SIZE - 1));
	for (int y = panel_sy0; y <= panel_sy1; y += PANEL_SIZE) {
		for (int x = panel_sx0; x <= panel_sx1; x += PANEL_SIZE) {
			int dx0 = x - panel_sx0;
			int dy0 = y - panel_sy0;
			int dx1 = std::min(dx0 + PANEL_SIZE, srcrect.width());
			int dy1 = std::min(dy0 + PANEL_SIZE, srcrect.height());
			QRect r(x, y, PANEL_SIZE, PANEL_SIZE);
			r = r.intersected({dx0, dy0, dx1 - dx0, dy1 - dy0});
			if (r.width() > 0 && r.height() > 0) {
				Panel *p = findPanel_(&panels_, {x, y});
				if (p) {
					r = r.translated(-x, -y);
					painter->drawImage(offset.x() + dx0, offset.y() + dy0, p->image, r.x(), r.y(), r.width(), r.height());
				}
			}
		}
	}
}
