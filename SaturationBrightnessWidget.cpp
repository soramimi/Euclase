#include "SaturationBrightnessWidget.h"
#include "MyApplication.h"
#include "MainWindow.h"
#include <QDebug>
#include <QFile>
#include <QPainter>
#include <stdint.h>
#include <QMouseEvent>
#include <omp.h>
#include "ApplicationGlobal.h"

struct SaturationBrightnessWidget::Private {
	int hue = 0;
	int sat = 255;
	int val = 255;
	QImage image;
	QRect rect;
};

SaturationBrightnessWidget::SaturationBrightnessWidget(QWidget *parent)
	: QWidget(parent)
	, m(new Private)
{
}

SaturationBrightnessWidget::~SaturationBrightnessWidget()
{
	delete m;
}

static void drawFrame(QPainter *pr, int x, int y, int w, int h)
{
	if (w < 3 || h < 3) {
		pr->fillRect(x, y, w, h, Qt::black);
	} else {
		pr->fillRect(x, y, w - 1, 1, Qt::black);
		pr->fillRect(x, y + 1, 1, h - 1, Qt::black);
		pr->fillRect(x + w - 1, y, 1, h - 1, Qt::black);
		pr->fillRect(x + 1, y + h - 1, w - 1, 1, Qt::black);
	}
}

QImage SaturationBrightnessWidget::createImage(int w, int h)
{
	QImage image(w, h, QImage::Format_RGBA8888);
	auto cuda = global->cuda;;
	if (cuda) {
		QColor color = QColor::fromHsv(m->hue, 255, 255);
		cudamem_t *cu0 = cuda->malloc(sizeof(uint32_t) * w * h);
		cuda->saturation_brightness(w, h, color.red(), color.green(), color.blue(), cu0);
		cuda->memcpy_dtoh(image.bits(), cu0, sizeof(uint32_t) * w * h);
		cuda->free(cu0);
	} else {
		for (int y = 0; y < h; y++) {
			int bri = 255 - 255 * y / h;
			uint8_t *dst = (uint8_t *)image.scanLine(y);
			for (int x = 0; x < w; x++) {
				int sat = 256 * x / w;
				QColor color = QColor::fromHsv(m->hue, sat, bri);
				dst[4 * x + 0] = color.red();
				dst[4 * x + 1] = color.green();
				dst[4 * x + 2] = color.blue();
				dst[4 * x + 3] = 255;
			}
		}
	}
	return image;
}

MainWindow *SaturationBrightnessWidget::mainwindow()
{
	return qobject_cast<MainWindow *>(window());
}

QRect SaturationBrightnessWidget::bounds() const
{
	int w = width();
	int h = height();
	int n = std::min(w, h) - 32;
	return {16, 16, n, n};
}

void SaturationBrightnessWidget::updatePixmap(bool force)
{
	m->rect = bounds();
	int w = m->rect.width();
	int h = m->rect.height();
	if (w > 1 && h > 1) {
		if ((m->image.width() != w || m->image.height() != h) || force) {
			m->image = createImage(w, h);
		}
	} else {
		m->image = QImage();
	}
}

void SaturationBrightnessWidget::paintEvent(QPaintEvent *)
{
	updatePixmap(false);
	if (!m->image.isNull()) {
		QPainter pr(this);
		int x = m->rect.x();
		int y = m->rect.y();
		int w = m->rect.width();
		int h = m->rect.height();
		pr.fillRect(x - 1, y - 1, w + 2, h + 2, Qt::black);
		pr.drawImage(x, y, m->image);

		float sat = m->sat / 255.0f;
		float val = m->val / 255.0f;
		int cx = sat * m->rect.width() + m->rect.x();
		int cy = (1 - val) * m->rect.height() + m->rect.y();
		const int R = 10;
		pr.setRenderHint(QPainter::Antialiasing);
		pr.setPen(QPen(Qt::black, 4));
		pr.drawEllipse(cx - R, cy - R, R * 2, R * 2);
		pr.setPen(QPen(Qt::white, 2));
		pr.drawEllipse(cx - R, cy - R, R * 2, R * 2);


	}
}

void SaturationBrightnessWidget::press(QPoint const &pos)
{
	float sat = pos.x() - m->rect.x();
	float val = pos.y() - m->rect.y();
	sat = sat / m->rect.width();
	val = 1 - val / m->rect.height();
	m->sat = (int)floorf(std::max(0.0f, std::min(1.0f, sat)) * 255.0f + 0.5f);
	m->val = (int)floorf(std::max(0.0f, std::min(1.0f, val)) * 255.0f + 0.5f);
	emit changeColor(QColor::fromHsv(m->hue, m->sat, m->val));
}

void SaturationBrightnessWidget::mousePressEvent(QMouseEvent *event)
{
	press(event->pos());
}

void SaturationBrightnessWidget::mouseMoveEvent(QMouseEvent *event)
{
	press(event->pos());
}

void SaturationBrightnessWidget::updateView()
{
	updatePixmap(true);
	emit changeColor(QColor::fromHsv(m->hue, m->sat, m->val));
	update();
}

void SaturationBrightnessWidget::setHue(int h, bool update)
{
	if (h < 0) {
		h = 360 - (-h % 360);
	} else {
		h %= 360;
	}
	m->hue = h;
	if (update) updateView();
}

void SaturationBrightnessWidget::setSat(int s, bool update)
{
	m->sat = s;
	if (update) updateView();
}

void SaturationBrightnessWidget::setVal(int v, bool update)
{
	m->val = v;
	if (update) updateView();
}

void SaturationBrightnessWidget::setHSV(int h, int s, int v)
{
	if (s == 0 && h < 0) {
		h = 0;
	}
	setHue(h, false);
	setSat(s, false);
	setVal(v, true);
}
