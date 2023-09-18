#ifndef BRUSHPREVIEWWIDGET_H
#define BRUSHPREVIEWWIDGET_H

#include "MainWindow.h"
#include "RoundBrushGenerator.h"
#include <QWidget>

class BrushPreviewWidget : public QWidget {
	Q_OBJECT
private:
	Brush brush_;
	double scale_ = 1;
	MainWindow *mainwindow();
	void changeBrush();
	double brushSize() const;
	double brushSoftness() const;
public:
	explicit BrushPreviewWidget(QWidget *parent = 0);

	void setBrushSize(double v);
	void setBrushSoftness(double percent);
	void setBrush(const Brush &b);
	void setBrush_(const Brush &b);
public slots:
	void changeScale(double scale);
protected:
	void paintEvent(QPaintEvent *);
};



#endif // BRUSHPREVIEWWIDGET_H
