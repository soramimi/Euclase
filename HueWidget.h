#ifndef HUEWIDGET_H
#define HUEWIDGET_H

#include <QWidget>

class MainWindow;

class HueWidget : public QWidget {
	Q_OBJECT
private:
	int hue_;
	int hue_old_;
	int press_pos_;
	QImage image_;
	QPixmap pixmap_;

	MainWindow *mainwindow();

	void emit_hueChanged_();
public:
	explicit HueWidget(QWidget *parent = 0);

	void setHue(int h);

signals:

public slots:

	// QWidget interface
protected:
	void paintEvent(QPaintEvent *);

	// QWidget interface
protected:
	void mousePressEvent(QMouseEvent *);
	void mouseMoveEvent(QMouseEvent *);
	void mouseReleaseEvent(QMouseEvent *);

	// QWidget interface
protected:
	void wheelEvent(QWheelEvent *);

	// QWidget interface
protected:
	void mouseDoubleClickEvent(QMouseEvent *);
signals:
	void hueChanged(int hue_);
};

#endif // HUEWIDGET_H
