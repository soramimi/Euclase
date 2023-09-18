#include "misc.h"
#include <QDebug>
#include <QFileInfo>
#include <QPainter>
#include <QProcess>
#include <QWidget>
#include <QContextMenuEvent>
#include <vector>
#include "joinpath.h"
#include "misc.h"

QString misc::getApplicationDir()
{
	QString path = QApplication::applicationFilePath();
	int i = path.lastIndexOf('\\');
	int j = path.lastIndexOf('/');
	if (i < j) i = j;
	if (i > 0) path = path.mid(0, i);
	return path;
}

QStringList misc::splitLines(QByteArray const &ba, std::function<QString(char const *ptr, size_t len)> const &tos)
{
	QStringList list;
	char const *begin = ba.data();
	char const *end = begin + ba.size();
	char const *ptr = begin;
	char const *left = ptr;
	while (1) {
		ushort c = 0;
		if (ptr < end) {
			c = *ptr;
		}
		if (c == '\n' || c == '\r' || c == 0) {
			list.push_back(tos(left, ptr - left));
			if (c == 0) break;
			if (c == '\n') {
				ptr++;
			} else if (c == '\r') {
				ptr++;
				if (ptr < end && *ptr == '\n') {
					ptr++;
				}
			}
			left = ptr;
		} else {
			ptr++;
		}
	}
	return list;
}

QStringList misc::splitLines(QString const &text)
{
	QStringList list;
	ushort const *begin = text.utf16();
	ushort const *end = begin + text.size();
	ushort const *ptr = begin;
	ushort const *left = ptr;
	while (1) {
		ushort c = 0;
		if (ptr < end) {
			c = *ptr;
		}
		if (c == '\n' || c == '\r' || c == 0) {
			list.push_back(QString::fromUtf16(left, ptr - left));
			if (c == 0) break;
			if (c == '\n') {
				ptr++;
			} else if (c == '\r') {
				ptr++;
				if (ptr < end && *ptr == '\n') {
					ptr++;
				}
			}
			left = ptr;
		} else {
			ptr++;
		}
	}
	return list;
}

void misc::splitLines(char const *begin, char const *end, std::vector<std::string> *out, bool keep_newline)
{
	char const *ptr = begin;
	char const *left = ptr;
	while (1) {
		char c = 0;
		if (ptr < end) {
			c = *ptr;
		}
		if (c == '\n' || c == '\r' || c == 0) {
			char const *right = ptr;
			if (c == '\n') {
				ptr++;
			} else if (c == '\r') {
				ptr++;
				if (ptr < end && *ptr == '\n') {
					ptr++;
				}
			}
			if (keep_newline) {
				right = ptr;
			}
			out->push_back(std::string(left, right - left));
			if (c == 0) break;
			left = ptr;
		} else {
			ptr++;
		}
	}
}

void misc::splitLines(std::string const &text, std::vector<std::string> *out, bool need_crlf)
{
	char const *begin = text.c_str();
	char const *end = begin + text.size();
	splitLines(begin, end, out, need_crlf);
}

QStringList misc::splitWords(QString const &text)
{
	QStringList list;
	ushort const *begin = text.utf16();
	ushort const *end = begin + text.size();
	ushort const *ptr = begin;
	ushort const *left = ptr;
	while (1) {
		ushort c = 0;
		if (ptr < end) {
			c = *ptr;
		}
		if (QChar::isSpace(c) || c == 0) {
			if (left < ptr) {
				list.push_back(QString::fromUtf16(left, ptr - left));
			}
			if (c == 0) break;
			ptr++;
			left = ptr;
		} else {
			ptr++;
		}
	}
	return list;
}

QString misc::getFileName(QString const &path)
{
	int i = path.lastIndexOf('/');
	int j = path.lastIndexOf('\\');
	if (i < j) i = j;
	if (i >= 0) {
		return path.mid(i + 1);
	}
	return path;
}

QString misc::makeDateTimeString(QDateTime const &dt)
{
	if (dt.isValid()) {
#if 0
		char tmp[100];
		sprintf(tmp, "%04u-%02u-%02u %02u:%02u:%02u"
				, dt.date().year()
				, dt.date().month()
				, dt.date().day()
				, dt.time().hour()
				, dt.time().minute()
				, dt.time().second()
				);
		return tmp;
#elif 0
		QString s = dt.toLocalTime().toString(Qt::DefaultLocaleShortDate);
		return s;
#else
		QString s = dt.toLocalTime().toString(Qt::ISODate);
		s.replace('T', ' ');
		return s;
#endif
	}
	return QString();
}

bool misc::starts_with(std::string const &str, std::string const &with)
{
	return strncmp(str.c_str(), with.c_str(), with.size()) == 0;
}

std::string misc::mid(std::string const &str, int start, int length)
{
	int size = (int)str.size();
	if (length < 0) length = size;

	length += start;
	if (start < 0) start = 0;
	if (length < 0) length = 0;
	if (start > size) start = size;
	if (length > size) length = size;
	length -= start;

	return std::string(str.c_str() + start, length);
}

#ifdef _WIN32
QString misc::normalizePathSeparator(QString const &str)
{
	if (!str.isEmpty()) {
		ushort const *s = str.utf16();
		size_t n = str.size();
		std::vector<ushort> v;
		v.reserve(n);
		for (size_t i = 0; i < n; i++) {
			ushort c = s[i];
			if (c == '/') {
				c = '\\';
			}
			v.push_back(c);
		}
		ushort const *p = &v[0];
		return QString::fromUtf16(p, (int)n);
	}
	return QString();
}
#else
QString misc::normalizePathSeparator(QString const &s)
{
	return s;
}
#endif

QString misc::joinWithSlash(QString const &left, QString const &right)
{
	if (!left.isEmpty() && !right.isEmpty()) {
		return joinpath(left, right);
	}
	return !left.isEmpty() ? left : right;
}

void misc::setFixedSize(QWidget *w)
{
	Qt::WindowFlags flags = w->windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	flags |= Qt::MSWindowsFixedSizeDialogHint;
	w->setWindowFlags(flags);
	w->setFixedSize(w->size());
}

void misc::drawFrame(QPainter *pr, int x, int y, int w, int h, QBrush color_topleft, QBrush color_bottomright)
{
	if (w < 3 || h < 3) {
		if (w > 0 && h > 0) {
			pr->fillRect(x, y, w, h, color_topleft);
		}
	} else {
		pr->fillRect(x, y, w - 1, 1, color_topleft);
		pr->fillRect(x, y + 1, 1, h -1, color_topleft);
		pr->fillRect(x + w - 1, y, 1, h -1, color_bottomright);
		pr->fillRect(x + 1, y + h - 1, w - 1, 1, color_bottomright);
	}
}

void misc::dump(uint8_t const *ptr, size_t len)
{
	if (ptr && len > 0) {
		size_t pos = 0;
		while (pos < len) {
			char tmp[100];
			char *dst = tmp;
			sprintf(dst, "%08llX ", ((unsigned long long)(pos)));
			dst += 9;
			for (int i = 0; i < 16; i++) {
				if (pos + i < len) {
					sprintf(dst, "%02X ", ptr[pos + i] & 0xff);
				} else {
					sprintf(dst, "   ");
				}
				dst += 3;
			}
			for (int i = 0; i < 16; i++) {
				int c = ' ';
				if (pos < len) {
					c = ptr[pos] & 0xff;
					if (!isprint(c)) {
						c = '.';
					}
					pos++;
				}
				*dst = c;
				dst++;
			}
			*dst = 0;
			qDebug() << tmp;
		}
	}
}

void misc::dump(QByteArray const *in)
{
	size_t len = 0;
	uint8_t const *ptr = nullptr;
	if (in) {
		len = in->size();
		if (len > 0) {
			ptr = (uint8_t const *)in->data();
		}
	}
	dump(ptr, len);
}

QString misc::determinFileType(QString const &filecommand, QString const &path, bool mime, std::function<void (QString const &, QByteArray *)> const &callback)
{ // ファイルタイプを調べる
	if (QFileInfo(filecommand).isExecutable()) {
		QString const &file = filecommand;
		QString mgc;
#ifdef Q_OS_WIN
		int i = file.lastIndexOf('/');
		int j = file.lastIndexOf('\\');
		if (i < j) i = j;
		if (i >= 0) {
			mgc = file.mid(0, i + 1) + "magic.mgc";
			if (QFileInfo(mgc).isReadable()) {
				// ok
			} else {
				mgc = QString();
			}
		}
#endif
		QString cmd;
		if (mgc.isEmpty()) {
			cmd = "\"%1\"";
			cmd = cmd.arg(file);
		} else {
			cmd = "\"%1\" -m \"%2\"";
			cmd = cmd.arg(file).arg(mgc);
			cmd = cmd.replace('\\', '/');
		}
		if (mime) {
			cmd += " --mime";
		}
		cmd += " --brief ";
		if (path == "-") {
			cmd += path;
		} else {
			cmd += QString("\"%1\"").arg(path);
		}
		cmd = misc::normalizePathSeparator(cmd);

		// run file command

		QByteArray ba;

		callback(cmd, &ba);

		// parse file type

		if (!ba.isEmpty()) {
			QString s = QString::fromUtf8(ba).trimmed();
			QStringList list = s.split(';', Qt::SkipEmptyParts);
			if (!list.isEmpty()) {
				QString mimetype = list[0].trimmed();
				return mimetype;
			}
		}

	} else {
		qDebug() << "No executable 'file' command";
	}
	return QString();
}

std::string misc::makeProxyServerURL(std::string text)
{
	if (!text.empty() && !strstr(text.c_str(), "://")) {
		text = "http://" + text;
		if (text[text.size() - 1] != '/') {
			text += '/';
		}
	}
	return text;
}

QString misc::makeProxyServerURL(QString text)
{
	if (!text.isEmpty() && text.indexOf("://") < 0) {
		text = "http://" + text;
		if (text[text.size() - 1] != '/') {
			text += '/';
		}
	}
	return text;
}

QPoint misc::contextMenuPos(QWidget *w, QContextMenuEvent *e)
{
	if (e && e->reason() == QContextMenuEvent::Mouse) {
		return QCursor::pos() + QPoint(8, -8);
	}
	return w->mapToGlobal(QPoint(4, 4));
}

bool misc::isExecutable(QString const &cmd)
{
	QFileInfo info(cmd);
	return info.isExecutable();
}
