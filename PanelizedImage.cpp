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

void PanelizedImage::paintImage(const QPoint &dstpos, const QImage &srcimg, QSize const &scale, QRect const &dstmask)
{
	int panel_dx0 = dstpos.x() - offset_.x();
	int panel_dy0 = dstpos.y() - offset_.y();
	int panel_dx1 = panel_dx0 + scale.width() - 1;
	int panel_dy1 = panel_dy0 + scale.height() - 1;

	const int S1 = PANEL_SIZE - 1;
	panel_dx0 &= ~S1;
	panel_dy0 &= ~S1;
	panel_dx1 &= ~S1;
	panel_dy1 &= ~S1;

	std::vector<Panel> newpanels;

	int zx0 = dstmask.x();
	int zy0 = dstmask.y();
	int zx1 = dstmask.x() + dstmask.width();
	int zy1 = dstmask.y() + dstmask.height();

	for (int y = panel_dy0; y <= panel_dy1; y += PANEL_SIZE) {
		for (int x = panel_dx0; x <= panel_dx1; x += PANEL_SIZE) {
			int sw = scale.width();
			int sh = scale.height();
			int sx0 = 0;
			int sy0 = 0;
			int sx1 = sx0 + sw;
			int sy1 = sy0 + sh;
			int dx0 = dstpos.x();
			int dy0 = dstpos.y();
			int dx1 = dx0 + sw;
			int dy1 = dy0 + sh;
			if (!dstmask.isEmpty()) {
				if (dx0 < zx0) { sx0 += zx0 - dx0; dx0 = zx0; }
				if (dy0 < zy0) { sy0 += zy0 - dy0; dy0 = zy0; }
				if (dx1 > zx1) { sx1 -= dx1 - zx1; dx1 = zx1; }
				if (dy1 > zy1) { sy1 -= dy1 - zy1; dy1 = zy1; }
			}
			const auto ox = offset_.x() + x;
			const auto oy = offset_.y() + y;
			dx0 -= ox;
			dy0 -= oy;
			dx1 -= ox;
			dy1 -= oy;
			if (dx0 < 0) { sx0 -= dx0; dx0 = 0; }
			if (dy0 < 0) { sy0 -= dy0; dy0 = 0; }
			if (dx1 > PANEL_SIZE) { sx1 -= dx1 - PANEL_SIZE; dx1 = PANEL_SIZE; }
			if (dy1 > PANEL_SIZE) { sy1 -= dy1 - PANEL_SIZE; dy1 = PANEL_SIZE; }
			int dw = sx1 - sx0;
			int dh = sy1 - sy0;
			if (dw > 0 && dh > 0) {
				if (srcimg.size() == scale) {
					sw = dw;
					sh = dh;
				} else {
					sx0 = sx0 * srcimg.width() / scale.width();
					sy0 = sy0 * srcimg.height() / scale.height();
					sx1 = sx1 * srcimg.width() / scale.width();
					sy1 = sy1 * srcimg.height() / scale.height();
					sw = std::max(1, sx1 - sx0);
					sh = std::max(1, sy1 - sy0);
				}
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
