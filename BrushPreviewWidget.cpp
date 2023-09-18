#include "BrushPreviewWidget.h"
#include "MyApplication.h"
#include <QDebug>
#include <QFile>
#include <QImage>
#include <QPainter>
#include <QTime>
#include <math.h>
#include "RoundBrushGenerator.h"
#include "ApplicationGlobal.h"

MainWindow *BrushPreviewWidget::mainwindow()
{
	return qobject_cast<MainWindow *>(window());
}

BrushPreviewWidget::BrushPreviewWidget(QWidget *parent)
	: QWidget(parent)
{
}

void BrushPreviewWidget::paintEvent(QPaintEvent *)
{
	int w = std::max(0.0, width() / scale_);
	int h = std::max(0.0, height() / scale_);
	w = std::min(w, (int)(brush_.size));
	h = std::min(h, (int)(brush_.size));
	double cx = (w / 2) + 1.5;
	double cy = (h / 2) + 1.5;
	w += 2;
	h += 2;

	RoundBrushGenerator brush(brush_.size, brush_.softness);
	euclase::Image brush_image = brush.image(w, h, cx, cy, Qt::white);

	QImage image(w, h, QImage::Format_RGBA8888);
	{
		image.fill(Qt::black);
		QPainter pr(&image);
		pr.drawImage(0, 0, brush_image.qimage());
	}

	QPainter pr(this);
	int x = width() / 2 - cx * scale_;
	int y = height() / 2 - cy * scale_;
	pr.fillRect(0, 0, width(), height(), Qt::black);
	pr.drawImage(QRect(x, y, w * scale_, h * scale_), image);
}

void BrushPreviewWidget::changeBrush()
{
	mainwindow()->setCurrentBrush(brush_);
	update();
}

double BrushPreviewWidget::brushSize() const
{
	return brush_.size;
}

double BrushPreviewWidget::brushSoftness() const
{
	return brush_.softness;
}

void BrushPreviewWidget::setBrushSize(double v)
{
	brush_.size = v;
	changeBrush();
}

void BrushPreviewWidget::setBrushSoftness(double v)
{
	brush_.softness = v;
	changeBrush();
}

void BrushPreviewWidget::setBrush(Brush const &b)
{
	setBrush_(b);
	changeBrush();
}

void BrushPreviewWidget::setBrush_(Brush const &b)
{
	brush_ = b;
	update();
}

void BrushPreviewWidget::changeScale(double scale)
{
	scale_ = scale;
	update();
}



