#ifndef DOCUMENT_H
#define DOCUMENT_H

#include "Canvas.h"

#include <QFileInfo>


class Document {
private:
public:
	QString file_path_;
	QFileInfo info_;
	Canvas canvas_;
	Document() {}
	void setDocumentPath(const QString &path)
	{
		file_path_ = path;
		info_ = QFileInfo(path);
	}
	QSize size() const
	{
		return canvas_.size();
	}
	QString fileName() const
	{
		return info_.fileName();
	}
};

#endif // DOCUMENT_H
