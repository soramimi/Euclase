#ifndef SATURATIONBRIGHTNESSWIDGET_H
#define SATURATIONBRIGHTNESSWIDGET_H

#include <QWidget>
#include <QPixmap>

class MainWindow;

class SaturationBrightnessWidget : public QWidget {
	Q_OBJECT
private:
	struct Private;
	Private *m;
	MainWindow *mainwindow();
	void updatePixmap(bool force);
	QImage createImage(int w, int h);
	void press(const QPoint &pos);
	QRect bounds() const;
protected:
	void paintEvent(QPaintEvent *);
	void mousePressEvent(QMouseEvent *event);
	void mouseMoveEvent(QMouseEvent *event);
public:
	explicit SaturationBrightnessWidget(QWidget *parent = 0);
	~SaturationBrightnessWidget();
	void updateView();
	void setHue(int h, bool update);
	void setSat(int s, bool update);
	void setVal(int v, bool update);
	void setHSV(int h, int s, int v);
signals:
	void changeColor(const QColor &color);

};

#endif // SATURATIONBRIGHTNESSWIDGET_H
