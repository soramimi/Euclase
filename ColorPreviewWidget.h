#ifndef COLORPREVIEWWIDGET_H
#define COLORPREVIEWWIDGET_H

#include <QWidget>

class ColorPreviewWidget : public QWidget {
private:
	QColor primary_color_;
	QColor secondary_color_;
public:
	ColorPreviewWidget(QWidget *parent = 0);

	// QWidget interface
	void setColor(const QColor &primary_color_, const QColor &secondary_color_);
protected:
	void paintEvent(QPaintEvent *event);
};

#endif // COLORPREVIEWWIDGET_H
