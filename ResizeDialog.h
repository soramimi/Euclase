#ifndef RESIZEDIALOG_H
#define RESIZEDIALOG_H

#include <QDialog>
#include "euclase.h"
// #include "resize.h"

namespace Ui {
class ResizeDialog;
}

class ResizeDialog : public QDialog
{
	Q_OBJECT
private:
	int original_width;
	int original_height;
public:
	explicit ResizeDialog(QWidget *parent = 0);
	~ResizeDialog();

	void setImageSize(const QSize &sz);

	QSize imageSize() const;
	euclase::EnlargeMethod method() const;
private slots:
	void on_lineEdit_width_textChanged(const QString &text);

	void on_lineEdit_height_textChanged(const QString &text);

	void on_checkBox_sync_stateChanged(int arg1);

private:
	Ui::ResizeDialog *ui;
	void sync_x();
	void sync_y();
};

#endif // RESIZEDIALOG_H
