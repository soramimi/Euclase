#include "HueWidget.h"
#include "MainWindow.h"

#include <QDebug>
#include <QMouseEvent>
#include <QPainter>

HueWidget::HueWidget(QWidget *parent)
	: QWidget(parent)
{
	hue_ = 0;
}

void HueWidget::setHue(int h)
{
	hue_ = h;
	emit hueChanged(h);
	update();
}

void HueWidget::paintEvent(QPaintEvent *)
{
	int w = width();
	int h = height();
	if (w > 6 && h > 0) {
		QPainter pr(this);
		if (image_.height() != h) {
			image_ = QImage(1, h, QImage::Format_ARGB32);
			for (int i = 0; i < h; i++) {
				QColor color = QColor::fromHsv(i * 360 / h, 255, 255);
				QRgb *p = (QRgb *)image_.scanLine(i);
				*p = qRgb(color.red(), color.green(), color.blue());
			}
			pixmap_ = QPixmap::fromImage(image_);
		}
		if (pixmap_.width() == 1 && pixmap_.height() == h) {
			pr.save();
			pr.setClipRect(3, 1, w - 6, h - 2);
			int y = -(hue_ + 180) * h / 360;
			while (y > 0) y -= h;
			while (y < h) {
				pr.drawPixmap(0, y, w, h, pixmap_);
				y += h;
			}
			pr.restore();
		}
		{
			pr.fillRect(2, 0, w - 5, 1,Qt::black);
			pr.fillRect(2, 1, 1, h - 1,Qt::black);
			pr.fillRect(3, h - 1, w - 5, 1,Qt::black);
			pr.fillRect(w - 3, 0, 1, h - 1,Qt::black);
			int y = height() / 2;
			pr.fillRect(0, y, 5, 1, Qt::black);
			pr.fillRect(w - 5, y, 5, 1, Qt::black);
		}
	}
}

MainWindow *HueWidget::mainwindow()
{
	return qobject_cast<MainWindow *>(window());
}

void HueWidget::emit_hueChanged_()
{
	int h = hue_;
	if (h < 0) {
		h = 360 - (-h % 360);
	} else {
		h %= 360;
	}
	setHue(h);
}

void HueWidget::mousePressEvent(QMouseEvent *e)
{
	if (e->button() == Qt::LeftButton) {
		hue_old_ = hue_;
		QPoint pos = QCursor::pos();
		pos = mapFromGlobal(pos);
		press_pos_ = pos.y();
	}
}

void HueWidget::mouseMoveEvent(QMouseEvent *e)
{
	if (e->buttons() & Qt::LeftButton) {

		QPoint pos = QCursor::pos();
		pos = mapFromGlobal(pos);

		hue_ = hue_old_ + (press_pos_ - pos.y()) * 360 / height();
		emit_hueChanged_();
	}
}

void HueWidget::mouseReleaseEvent(QMouseEvent *)
{
}

void HueWidget::wheelEvent(QWheelEvent *e)
{
	int delta = e->angleDelta().y();
	if (delta < 0) {
		delta /= 120;
		if (delta == 0) delta = -1;
	}
	if (delta > 0) {
		delta /= 120;
		if (delta == 0) delta = 1;
	}
	hue_ = (hue_ + delta + 360) % 360;
	emit_hueChanged_();
}

void HueWidget::mouseDoubleClickEvent(QMouseEvent *)
{
	QPoint pos = QCursor::pos();
	pos = mapFromGlobal(pos);

	hue_ += (pos.y() - height() / 2) * 360 / height();
	emit_hueChanged_();
}



