#ifndef SELECTIONOUTLINERENDERER_H
#define SELECTIONOUTLINERENDERER_H

#include <QBitmap>
#include <QRect>
#include <QThread>

class MainWindow;

class SelectionOutlineBitmap {
public:
	SelectionOutlineBitmap() = default;
	SelectionOutlineBitmap(QPoint point, QBitmap const &bitmap)
		: point(point)
		, bitmap(bitmap)
	{

	}
	QPoint point;
	QBitmap bitmap;
};
Q_DECLARE_METATYPE(SelectionOutlineBitmap)

class SelectionOutlineRenderer : public QThread {
	Q_OBJECT
private:
	volatile bool requested_ = false;
	MainWindow *mainwindow_;
	QRect rect_;
	bool abort_ = false;
protected:
	void run();
public:
	explicit SelectionOutlineRenderer(QObject *parent = nullptr);
	~SelectionOutlineRenderer() override;
	void request(MainWindow *mw, QRect const &rect);
	void abort();
signals:
	void done(SelectionOutlineBitmap const &data);
};

#endif // SELECTIONOUTLINERENDERER_H
