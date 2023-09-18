#ifndef ABSTRACTFILTERFORM_H
#define ABSTRACTFILTERFORM_H

#include <QWidget>
#include "FilterDialog.h"

class AbstractFilterForm : public QWidget {
	friend class FilterDialog;
	Q_OBJECT
protected:
	FilterDialog *dialog()
	{
		return qobject_cast<FilterDialog *>(window());
	}

	FilterContext *context()
	{
		return dialog()->context();
	}

	virtual void start() {};
public:
	explicit AbstractFilterForm(QWidget *parent = nullptr);
};

#endif // ABSTRACTFILTERFORM_H
