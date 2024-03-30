#ifndef FILTERDIALOG_H
#define FILTERDIALOG_H

#include <QDialog>
#include <QVariant>
#include <functional>
#include "euclase.h"

class MainWindow;
class AbstractFilterForm;

namespace Ui {
class FilterDialog;
}

class FilterContext {
private:
	std::map<QString, QVariant> parameters_;
	bool cancel_ = false;
	float progress_ = 0.0f;
	euclase::Image source_image_;
public:
	void setSourceImage(euclase::Image const &image)
	{
		source_image_ = image;

	}
	euclase::Image const &sourceImage() const
	{
		return source_image_;
	}
	void setParameter(QString const &name, QVariant const &val)
	{
		parameters_[name] = val;
	}
	QVariant parameter(QString const &name, QVariant def = {}) const
	{
		auto it = parameters_.find(name);
		if (it != parameters_.end()) {
			return it->second;
		}
		return def;
	}
	float *progress_ptr()
	{
		return &progress_;
	}
	bool *cancel_ptr()
	{
		return &cancel_;
	}
};

typedef std::function<euclase::Image (FilterContext *)> FilterFunction;

class FilterDialog : public QDialog {
	Q_OBJECT
private:
	Ui::FilterDialog *ui;
	struct Private;
	Private *m;
	MainWindow *mainwindow;
	void startFilter();
	void stopFilter(bool join);
	void setProgress(float value);
	void updateImageView();
public:
	explicit FilterDialog(MainWindow *parent, FilterContext *context, AbstractFilterForm *form, FilterFunction const &fn);
	~FilterDialog();
	void updateFilter();
	euclase::Image result();
	bool isPreviewEnabled() const;
	FilterContext *context();
private slots:
	void on_checkBox_preview_stateChanged(int arg1);
protected:
	void timerEvent(QTimerEvent *event);
	void paintEvent(QPaintEvent *event);
};

#endif // FILTERDIALOG_H
