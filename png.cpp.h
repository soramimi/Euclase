

#include "euclase.h"
#include <functional>
#include <png.h>
#include <stdio.h>
#include <vector>

#pragma warning(disable:4996)

namespace euclase {

static void png_error(png_struct *png_ptr, png_const_charp msg)
{
	throw msg;
}

static void *io_ptr(png_struct *png_ptr)
{
	return png_get_io_ptr(png_ptr);
}

static bool _write_png(euclase::Image const &src, std::function<int (char const *p, int n)> fn)
{
	if (src.format() != euclase::Image::Format_8_RGB) {
		euclase::Image img = src.convertToFormat(euclase::Image::Format_8_RGB);
		return _write_png(img, fn);
	}

	const int width = src.width();
	const int height = src.height();

	png_struct *png_ptr = NULL;
	png_info *info_ptr = NULL;

	const int           ciBitDepth = 8;
	const int           ciChannels = 3;

	png_uint_32         ulRowBytes;

	// prepare the standard PNG structures

	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, (png_error_ptr)png_error, (png_error_ptr)NULL);
	if (!png_ptr) {
		return false;
	}

	info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr) {
		png_destroy_write_struct(&png_ptr, (png_infopp) NULL);
		return false;
	}

	bool success = false;
	try {
		// initialize the png structure

		png_set_write_fn(png_ptr, &fn, [](png_struct *png, png_byte *p, size_t n){
				auto *fn = (std::function<int (char const *, int)> *)io_ptr(png);
				fn->operator()((char const *)p, n);
			}, nullptr);

		// we're going to write a very simple 3x8 bit RGB image

		png_set_IHDR(png_ptr, info_ptr, width, height, ciBitDepth, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

		// write the file header information

		png_write_info(png_ptr, info_ptr);

		// swap the BGR pixels in the DiData structure to RGB

		png_set_bgr(png_ptr);

		// row_bytes is the width x number of channels

		ulRowBytes = width * ciChannels;

		std::vector<uint8_t> tmp_dst(ulRowBytes);

		{
			int i;
			int pass, num_pass; /* pass variables */

			/* intialize interlace handling.  If image is not interlaced,
					this will set pass to 1 */
			num_pass = png_set_interlace_handling(png_ptr);

			/* loop through passes */
			for (pass = 0; pass < num_pass; pass++) {
				/* loop through image */
				for (i = 0; i < height; i++) {
					uint8_t const *tmp_src = src.scanLine(i);
					for (int i = 0; i < width; i++) {
						tmp_dst[i * 3 + 0] = tmp_src[i * 3 + 0];
						tmp_dst[i * 3 + 1] = tmp_src[i * 3 + 1];
						tmp_dst[i * 3 + 2] = tmp_src[i * 3 + 2];
					}
					png_write_row(png_ptr, &tmp_dst[0]);
				}
			}
		}

		// write the additional chunks to the PNG file (not really needed)

		png_write_end(png_ptr, info_ptr);

		success = true;

	} catch (png_const_charp) {
	}

	png_destroy_write_struct(&png_ptr, &info_ptr);

	return success;
}

bool write_png(euclase::Image const &src, char const *filename)
{
	FILE *fp;
	fp = fopen(filename, "wb");
	if (!fp) {
		return false;
	}

	bool ok = _write_png(src, [&](char const *p, int n){
		return fwrite(p, 1, n, fp) == n;
	});

	fclose(fp);

	return ok;
}

bool write_png(euclase::Image const &src, std::vector<char> *out)
{
	*out = {};
	return _write_png(src, [&](char const *p, int n){
		out->insert(out->end(), p, p + n);
		return true;
	});
}

struct Source {
	int (*method)(void *ptr, size_t len, void *cookie);
	void *cookie;
};

static void _get_bytes(png_struct *png_ptr, png_bytep data, png_size_t length)
{
	Source *info = (Source *)io_ptr(png_ptr);
	info->method(data, length, info->cookie);
}

static bool _load_png(euclase::Image *dst, std::function<int (char *p, int n)> fn)
{
	bool success = false;

	png_struct *png_ptr = NULL;
	png_info *info_ptr = NULL;
	png_byte            pbSig[8];
	int                 iBitDepth;
	int                 iColorType;
	double              dGamma;
	png_color_16       *pBackground;
	png_uint_32         ulChannels;
	png_uint_32         ulRowBytes;
	png_uint_32 width;
	png_uint_32 height;
	unsigned long bgcolor;

	// first check the eight byte PNG signature

	if (fn((char *)pbSig, 8) != 8) {
		return false;
	}
	if (!png_check_sig(pbSig, 8)) {
		return false;
	}

	// create the two png(-info) structures

	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, (png_error_ptr)png_error, (png_error_ptr)NULL);
	if (!png_ptr) {
		return success;
	}

	info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr) {
		png_destroy_read_struct(&png_ptr, NULL, NULL);
		return success;
	}

	try {

		// initialize the png structure

		//				png_set_read_fn(png_ptr, (png_voidp)&source, _get_bytes);
		png_set_read_fn(png_ptr, (png_voidp)&fn, [](png_struct *png, png_byte *p, size_t n){
			auto *fn = (std::function<int (char *, int)> *)io_ptr(png);
			fn->operator()((char *)p, n);
		});

		png_set_sig_bytes(png_ptr, 8);

		// read all PNG info up to image data

		png_read_info(png_ptr, info_ptr);

		// get width, height, bit-depth and color-type

		png_get_IHDR(png_ptr, info_ptr, &width, &height, &iBitDepth, &iColorType, NULL, NULL, NULL);

		// expand images of all color-type and bit-depth to 3x8 bit RGB images
		// let the library process things like alpha, transparency, background

		if (iBitDepth == 16)
			png_set_strip_16(png_ptr);

		if (iColorType == PNG_COLOR_TYPE_PALETTE)
			png_set_expand(png_ptr);

		if (iBitDepth < 8)
			png_set_expand(png_ptr);

		if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
			png_set_expand(png_ptr);

		if (iColorType == PNG_COLOR_TYPE_GRAY || iColorType == PNG_COLOR_TYPE_GRAY_ALPHA)
			png_set_gray_to_rgb(png_ptr);

		// set the background color to draw transparent and alpha images over.
		if (png_get_bKGD(png_ptr, info_ptr, &pBackground)) {
			png_set_background(png_ptr, pBackground, PNG_BACKGROUND_GAMMA_FILE, 1, 1.0);
			int r = (uint8_t)pBackground->red;
			int g = (uint8_t)pBackground->green;
			int b = (uint8_t)pBackground->blue;
			bgcolor = 0xff000000 | (b << 16) | (g << 8) | b;
		} else {
			bgcolor = 0;
		}

		// if required set gamma conversion
		if (png_get_gAMA(png_ptr, info_ptr, &dGamma))
			png_set_gamma(png_ptr, (double) 2.2, dGamma);

		// after the transformations have been registered update info_ptr data

		png_read_update_info(png_ptr, info_ptr);

		// get again width, height and the new bit-depth and color-type

		png_get_IHDR(png_ptr, info_ptr, &width, &height, &iBitDepth, &iColorType, NULL, NULL, NULL);


		// row_bytes is the width x number of channels

		ulRowBytes = png_get_rowbytes(png_ptr, info_ptr);
		ulChannels = png_get_channels(png_ptr, info_ptr);

		if (ulChannels != 3) {
			throw "unsupported channels";
		}

		// now we can allocate memory to store the image

		//dst->CreateImage(width, height, ulChannels, bgcolor);

		std::vector<png_byte> rowbuffer(ulRowBytes);

		{
			png_uint_32 i;
			int pass, j;

			/* save jump buffer and error functions */

			pass = png_set_interlace_handling(png_ptr);

			height = png_get_image_height(png_ptr, info_ptr);

			*dst = euclase::Image(width, height, euclase::Image::Format_8_RGB);
			for (j = 0; j < pass; j++) {
				for (i = 0; i < height; i++) {
					png_read_row(png_ptr, &rowbuffer[0], NULL);
					if (ulChannels == 3) {
						png_byte *srcptr = &rowbuffer[0];
						uint8_t *dstptr = dst->scanLine(i);
						for (unsigned int x = 0; x < width; x++) {
							dstptr[0] = srcptr[0];
							dstptr[1] = srcptr[1];
							dstptr[2] = srcptr[2];
							srcptr += 3;
							dstptr += 3;
						}
					}
				}
			}
		}

		// read the additional chunks in the PNG file (not really needed)

		png_read_end(png_ptr, NULL);

		success = true;		// success

	} catch (png_const_charp) {
	}

	png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

	return success;
}


static int myread(void *ptr, size_t len, void *cookie)
{
	return (int)fread(ptr, 1, len, (FILE *)cookie);
}


bool load_png(euclase::Image *dst, char const *filename)
{
	FILE *fp = fopen(filename, "rb");
	if (!fp) {
		return false;
	}

	bool success = _load_png(dst, [&](char *p, int n){
		return fread(p, 1, n, fp);
	});

	fclose(fp);

	return success;
}

}
