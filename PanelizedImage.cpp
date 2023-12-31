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

void PanelizedImage::paintImage(const QPoint &dstpos, const QImage &srcimg, QSize const &scale, const QRect &srcrect)
{
	int panel_dx0 = dstpos.x() - offset_.x();
	int panel_dy0 = dstpos.y() - offset_.y();
	int panel_dx1 = panel_dx0 + scale.width() - 1;
	int panel_dy1 = panel_dy0 + scale.height() - 1;

	const int S1 = PANEL_SIZE - 1;
	panel_dx0 = panel_dx0 & ~S1;
	panel_dy0 = panel_dy0 & ~S1;
	panel_dx1 = (panel_dx1 + S1) & ~S1;
	panel_dy1 = (panel_dy1 + S1) & ~S1;

	std::vector<Panel> newpanels;

	for (int y = panel_dy0; y <= panel_dy1; y += PANEL_SIZE) {
		for (int x = panel_dx0; x <= panel_dx1; x += PANEL_SIZE) {
			int sw = scale.width();
			int sh = scale.height();
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
			if (sx1 > scale.width()) { dx1 -= sx1 - sw; sx1 = sw; }
			if (sy1 > scale.height()) { dy1 -= sy1 - sh; sy1 = sh; }
			int dw = sx1 - sx0;
			int dh = sy1 - sy0;
			if (srcimg.size() == scale) {
				sw = dw;
				sh = dh;
			} else {
				sx0 = sx0 * srcimg.width() / scale.width();
				sy0 = sy0 * srcimg.height() / scale.height();
				sx1 = sx1 * srcimg.width() / scale.width();
				sy1 = sy1 * srcimg.height() / scale.height();
				sw = sx1 - sx0;
				sh = sy1 - sy0;
			}
			if (sw > 0 && sh > 0) {
				Panel *dst = findPanel_(&panels_, {x, y});
				if (!dst) { // なければ追加
					newpanels.push_back({{x, y}, QImage(PANEL_SIZE, PANEL_SIZE, format_)});
					dst = &newpanels.back();
					dst->image.fill(Qt::transparent);
				}
				{
					QPainter pr(&dst->image);
					pr.drawImage(QRect(dx0, dy0, dw, dh), srcimg, QRect(sx0, sy0, sw, sh));
				}
			}
		}
	}

	panels_.insert(panels_.end(), newpanels.begin(), newpanels.end());

	std::sort(panels_.begin(), panels_.end(), [&](Panel const &a, Panel const &b){ // offset でソート
		return misc::compareQPoint(a.offset, b.offset) < 0;
	});
}

void PanelizedImage::renderImage(QPainter *painter, const QPoint &dstpos, const QRect &srcrect) const
{
	for (Panel const &panel : panels_) {
		QRect r(0, 0, PANEL_SIZE, PANEL_SIZE);
		int ox = offset().x() + panel.offset.x();
		int oy = offset().y() + panel.offset.y();
		r = r.intersected(srcrect.translated(-ox, -oy));
		if (!r.isEmpty()) {
			int dx = dstpos.x() + ox + r.x() - srcrect.x();
			int dy = dstpos.y() + oy + r.y() - srcrect.y();
			painter->drawImage(dx, dy, panel.image, r.x(), r.y(), r.width(), r.height());
		}
	}
}
