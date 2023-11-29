#include "antialias.h"
#include "euclase.h"

#include <QImage>
#include <stdint.h>
#include <functional>
#include <QDebug>

namespace {

class AntialiasRW {
public:
	virtual void reader(int line, uint8_t *out) = 0;
	virtual void writer(int pos, uint8_t v) = 0;
};

void filter3(int length, uint8_t *line0, uint8_t *line1, uint8_t *line2, AntialiasRW *rw)
{
	for (int pos = 0; pos + 1 < length; pos++) {
		if (line1[pos] != line1[pos + 1]) {
			int a = (line1[pos] + line1[pos + 1]) / 2;
			int n1 = 0;
			int n2 = 0;
			if (line1[pos] < a) {
				if (line0[pos + 1] < a || line2[pos + 1] < a) {
					for (int i = pos; i > 0; i--) {
						if (line1[i] > a) break;
						if (line2[i] < a) break;
						n1++;
					}
				}
				if (line0[pos] > a || line2[pos] > a) {
					for (int i = pos + 1; i < length; i++) {
						if (line1[i] < a) break;
						if (line0[i] > a) break;
						n2++;
					}
				}
			} else if (line1[pos + 1] < a) {
				if (line0[pos + 1] > a || line2[pos + 1] > a) {
					for (int i = pos; i > 0; i--) {
						if (line1[i] < a) break;
						if (line0[i] > a) break;
						n1++;
					}
				}
				if (line0[pos] < a || line2[pos] < a) {
					for (int i = pos + 1; i < length; i++) {
						if (line1[i] > a) break;
						if (line2[i] < a) break;
						n2++;
					}
				}
			}
			n1 /= 2;
			if (n1 > 0) {
				int b = line1[pos - n1];
				for (int i = 0; i < n1; i++) {
					rw->writer(pos - i, a + (b - a) * (i + 1) / (n1 + 1));
				}
			}
			n2 /= 2;
			if (n2 > 0) {
				int b = line1[pos + 1 + n2];
				for (int i = 0; i < n2; i++) {
					rw->writer(pos + i + 1, a + (b - a) * (i + 1) / (n2 + 1));
				}
			}
			pos += n2;
		}
	}
}

class BasicAntialias {
protected:
	int width;
	int height;
	int row;
	uint8_t *tmp0;
	uint8_t *tmp1;
	uint8_t *tmp2;
	uint8_t **scanlines;

	void filter2(int length, int rows, AntialiasRW *rw)
	{
		uint8_t *buf0 = tmp0;
		uint8_t *buf1 = tmp1;
		uint8_t *buf2 = tmp2;
		rw->reader(0, buf0);
		rw->reader(0, buf2);
		for (row = 0; row < rows; row++) {
			if (row > 0) {
				std::swap(buf0, buf1);
			}
			if (row + 1 < rows) {
				std::swap(buf1, buf2);
				rw->reader(row + 1, buf2);
			} else {
				buf1 = buf2;
			}
			filter3(length, buf0, buf1, buf2, rw);
			filter3(length, buf2, buf1, buf0, rw);
		}
	}
};

class AntialiasGray8 : public BasicAntialias {
private:
	class RW_H : public AntialiasRW {
	public:
		AntialiasGray8 *a;
		void reader(int line, uint8_t *out)
		{
			memcpy(out, a->scanlines[line], a->width);
		}
		void writer(int pos, uint8_t v)
		{
			a->scanlines[a->row][pos] = v;
		}
		RW_H(AntialiasGray8 *a)
			: a(a)
		{
		}
	};
	class RW_V : public AntialiasRW {
	public:
		AntialiasGray8 *a;
		void reader(int line, uint8_t *out)
		{
			for (int y = 0; y < a->height; y++) {
				out[y] = a->scanlines[y][line];
			}
		}
		void writer(int pos, uint8_t v)
		{
			a->scanlines[pos][a->row] = v;
		}
		RW_V(AntialiasGray8 *a)
			: a(a)
		{
		}
	};
public:
	void filter(euclase::Image *image)
	{
		width = image->width();
		height = image->height();

		int span = width > height ? width : height;
		tmp0 = (uint8_t *)alloca(span);
		tmp1 = (uint8_t *)alloca(span);
		tmp2 = (uint8_t *)alloca(span);

		scanlines = (uint8_t **)alloca(sizeof(uint8_t *) * height);
		for (int y = 0; y < height; y++) {
			scanlines[y] = (uint8_t *)image->scanLine(y);
		}

		RW_H rwh(this);
		RW_V rwv(this);

		filter2(width, height, &rwh);
		filter2(height, width, &rwv);
	}
};

class AntialiasRGB888 : public BasicAntialias {
public:
	int plane;
	class RW_H : public AntialiasRW {
	public:
		AntialiasRGB888 *a;
		void reader(int line, uint8_t *out)
		{
			uint8_t const *src = a->scanlines[line];
			for (int i = 0; i < a->width; i++) {
				out[i] = src[i * 4 + a->plane];
			}
		}
		void writer(int pos, uint8_t v)
		{
			uint8_t *ptr = a->scanlines[a->row] + pos * 4;
			ptr[a->plane] = v;
		}
		RW_H(AntialiasRGB888 *a)
			: a(a)
		{
		}
	};
	class RW_V : public AntialiasRW {
	public:
		AntialiasRGB888 *a;
		void reader(int line, uint8_t *out)
		{
			for (int y = 0; y < a->height; y++) {
				uint8_t const *src = a->scanlines[y] + line * 4;
				out[y] = src[a->plane];
			}
		}
		void writer(int pos, uint8_t v)
		{
			uint8_t *ptr = a->scanlines[pos] + a->row * 4;
			ptr[a->plane] = v;
		}
		RW_V(AntialiasRGB888 *a)
			: a(a)
		{
		}
	};

	void filling(euclase::Image *image)
	{
		int w = image->width();
		int h = image->height();
		for (int y = 0; y < h; y++) {
			uint8_t *p = (uint8_t *)image->scanLine(y);
			for (int x = 0; x < w; x++) {
				int j;
				for (j = x; j < w; j++) {
					if (p[j * 4 + 3] != 0) { // alpha
						break;
					}
				}
				if (j > x) {
					int i = x;
					x = j;
					int m = (i + j) / 2;
					uint8_t r = 128;
					uint8_t g = 128;
					uint8_t b = 128;
					if (i > 0) {
						r = p[(i - 1) * 4 + 0];
						g = p[(i - 1) * 4 + 1];
						b = p[(i - 1) * 4 + 2];
					}
					while (i < m) {
						p[i * 4 + 0] = r;
						p[i * 4 + 1] = g;
						p[i * 4 + 2] = b;
						i++;
					}
					if (j + 1 < w) {
						r = p[j * 4 + 0];
						g = p[j * 4 + 1];
						b = p[j * 4 + 2];
					}
					while (m < j) {
						j--;
						p[j * 4 + 0] = r;
						p[j * 4 + 1] = g;
						p[j * 4 + 2] = b;
					}
				}
			}
		}
	}
public:
	void filter(euclase::Image *image)
	{
		filling(image);

		width = image->width();
		height = image->height();

		int span = width > height ? width : height;
		tmp0 = (uint8_t *)alloca(span);
		tmp1 = (uint8_t *)alloca(span);
		tmp2 = (uint8_t *)alloca(span);

		scanlines = (uint8_t **)alloca(sizeof(uint8_t *) * height);
		for (int y = 0; y < height; y++) {
			scanlines[y] = (uint8_t *)image->scanLine(y);
		}

		RW_H rwh(this);
		RW_V rwv(this);

		plane = 0; // red
		filter2(width, height, &rwh);
		filter2(height, width, &rwv);
		plane++; // green
		filter2(width, height, &rwh);
		filter2(height, width, &rwv);
		plane++; // blue
		filter2(width, height, &rwh);
		filter2(height, width, &rwv);
	}
};

} // namespace

bool filter_antialias(euclase::Image *image)
{
//	qDebug() << image->format();
	if (!image) {
//		qDebug() << "antialias: Null pointer.";
		return false;
	}
	int w = image->width();
	int h = image->height();
	if (w < 1 || h < 1) {
//		qDebug() << "antialias: Empty image.";
		return false;
	}

	if (image->format() == euclase::Image::Format_8_Grayscale) {
		*image = image->toHost();
		AntialiasGray8().filter(image);
		return true;
	}

	if (image->format() == euclase::Image::Format_8_RGBA) {
		*image = image->toHost();
		AntialiasRGB888().filter(image);
		return true;
	}

	if (image->format() == euclase::Image::Format_F_RGBA) {
		*image = image->convert(euclase::Image::Format_8_RGBA);
		if (filter_antialias(image)) {
			*image = image->convert(euclase::Image::Format_F_RGBA);
			return true;
		}
		return false;
	}

	qDebug() << "antialias: Unsupported image format.";
	return false;
}


