#ifndef NEWDIALOG_H
#define NEWDIALOG_H

#include <QDialog>

namespace Ui {
class NewDialog;
}

class NewDialog : public QDialog {
	Q_OBJECT
public:
	enum class From {
		New,
		Clipboard,
	};
public:
	explicit NewDialog(QWidget *parent = nullptr);
	~NewDialog();
	From from() const;
	void setImageSize(QSize size);
	QSize imageSize() const;
private slots:
	void on_radioButton_from_clipboard_clicked();

private:
	Ui::NewDialog *ui;
	QSize validateImageSize(const QSize &size) const;
};

#endif // NEWDIALOG_H
