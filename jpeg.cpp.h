
// #include "image/euclase.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <functional>
#include <vector>
#include "jpeglib.h"
#include "jerror.h"

#define SIZEOF(object)	((size_t) sizeof(object))
typedef uint8_t JOCTET;

namespace euclase {

struct DESTINATION {
	std::function<int (char const *p, int n, void *cookie)> fn;
	void *cookie;
};


static inline int MYWRITE(DESTINATION *dest, void const *ptr, size_t len)
{
	return dest->fn((char const *)ptr, len, dest->cookie);
}




/* Expanded data destination object for stdio output */

typedef struct {
	struct jpeg_destination_mgr pub; /* public fields */

	DESTINATION *outfile;		/* target stream */
	JOCTET *buffer;		/* start of buffer */
} my_destination_mgr;

typedef my_destination_mgr * my_dest_ptr;

#define OUTPUT_BUF_SIZE  4096	/* choose an efficiently fwrite'able size */

//
METHODDEF(void)
init_destination (j_compress_ptr cinfo)
{
	my_dest_ptr dest = (my_dest_ptr) cinfo->dest;

	/* Allocate the output buffer --- it will be released when done with image */
	dest->buffer = (JOCTET *)
		(*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_IMAGE,
								   OUTPUT_BUF_SIZE * SIZEOF(JOCTET));

	dest->pub.next_output_byte = dest->buffer;
	dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;
}

//
METHODDEF(boolean)
empty_output_buffer (j_compress_ptr cinfo)
{
	my_dest_ptr dest = (my_dest_ptr) cinfo->dest;

	if (MYWRITE(dest->outfile, dest->buffer, OUTPUT_BUF_SIZE) !=
		(size_t) OUTPUT_BUF_SIZE)
		ERREXIT(cinfo, JERR_FILE_WRITE);

	dest->pub.next_output_byte = dest->buffer;
	dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;

	return TRUE;
}

//
METHODDEF(void)
term_destination (j_compress_ptr cinfo)
{
	my_dest_ptr dest = (my_dest_ptr) cinfo->dest;
	size_t datacount = OUTPUT_BUF_SIZE - dest->pub.free_in_buffer;

	/* Write any data remaining in the buffer */
	if (datacount > 0) {
		if (MYWRITE(dest->outfile, dest->buffer, datacount) != datacount)
			ERREXIT(cinfo, JERR_FILE_WRITE);
	}
}

//
METHODDEF(void)
jpeg_my_dest (j_compress_ptr cinfo, DESTINATION * outfile)
{
	my_dest_ptr dest;

	if (cinfo->dest == NULL) {	/* first time for this JPEG object? */
		cinfo->dest = (struct jpeg_destination_mgr *)(*cinfo->mem->alloc_small)((j_common_ptr) cinfo, JPOOL_PERMANENT, SIZEOF(my_destination_mgr));
	}

	dest = (my_dest_ptr) cinfo->dest;
	dest->pub.init_destination = init_destination;
	dest->pub.empty_output_buffer = empty_output_buffer;
	dest->pub.term_destination = term_destination;
	dest->outfile = outfile;
}




static bool _write_jpeg(euclase::Image const &src, int quality, DESTINATION *dest)
{
	if (src.format() != Image::Format_8_RGB) {
		Image src2 = src.convertToFormat(euclase::Image::Format_8_RGB);
		return _write_jpeg(src2, quality, dest);
	}

	int width = src.width();
	int height = src.height();

	/* This struct contains the JPEG compression parameters and pointers to
			* working space (which is allocated as needed by the JPEG library).
			* It is possible to have several such structures, representing multiple
			* compression/decompression processes, in existence at once.  We refer
			* to any one struct (and its associated working data) as a "JPEG object".
			*/
	struct jpeg_compress_struct cinfo;
	/* This struct represents a JPEG error handler.  It is declared separately
			* because applications often want to supply a specialized error handler
			* (see the second half of this file for an example).  But here we just
			* take the easy way out and use the standard error handler, which will
			* print a message on stderr and call exit() if compression fails.
			* Note that this struct must live as long as the main JPEG parameter
			* struct, to avoid dangling-pointer problems.
			*/
	struct jpeg_error_mgr jerr;
	/* More stuff */
	JSAMPROW row_pointer[1];	/* pointer to JSAMPLE row[s] */
	int row_stride;		/* physical row width in image buffer */

	/* Step 1: allocate and initialize JPEG compression object */

	/* We have to set up the error handler first, in case the initialization
			* step fails.  (Unlikely, but it could happen if you are out of memory.)
			* This routine fills in the contents of struct jerr, and returns jerr's
			* address which we place into the link field in cinfo.
			*/
	cinfo.err = jpeg_std_error(&jerr);
	/* Now we can initialize the JPEG compression object. */
	jpeg_create_compress(&cinfo);

	/* Step 2: specify data destination (eg, a file) */
	/* Note: steps 2 and 3 can be done in either order. */

	/* Here we use the library-supplied code to send compressed data to a
			* stdio stream.  You can also write your own code to do something else.
			* VERY IMPORTANT: use "b" option to fopen() if you are on a machine that
			* requires it in order to write binary files.
			*/

	jpeg_my_dest(&cinfo, dest);

	/* Step 3: set parameters for compression */

	/* First we supply a description of the input image.
			* Four fields of the cinfo struct must be filled in:
			*/
	cinfo.image_width = width; 	/* image width and height, in pixels */
	cinfo.image_height = height;
	cinfo.input_components = 3;		/* # of color components per pixel */
	cinfo.in_color_space = JCS_RGB; 	/* colorspace of input image */
	/* Now use the library's routine to set default compression parameters.
			* (You must set at least cinfo.in_color_space before calling this,
			* since the defaults depend on the source color space.)
			*/
	jpeg_set_defaults(&cinfo);
	/* Now you can set any non-default parameters you wish to.
			* Here we just illustrate the use of quality (quantization table) scaling:
			*/
	jpeg_set_quality(&cinfo, quality, TRUE /* limit to baseline-JPEG values */);

	/* Step 4: Start compressor */

	/* TRUE ensures that we will write a complete interchange-JPEG file.
			* Pass TRUE unless you are very sure of what you're doing.
			*/
	jpeg_start_compress(&cinfo, TRUE);

	/* Step 5: while (scan lines remain to be written) */
	/*           jpeg_write_scanlines(...); */

	/* Here we use the library's state variable cinfo.next_scanline as the
			* loop counter, so that we don't have to keep track ourselves.
			* To keep things simple, we pass one scanline per call; you can pass
			* more if you wish, though.
			*/
	row_stride = width * 3;	/* JSAMPLEs per row in image_buffer */

	//			std::vector<uint8_t> tmp_src(width * 3);
	std::vector<uint8_t> tmp_rgb(row_stride);

	while (cinfo.next_scanline < cinfo.image_height) {
		uint8_t const *tmp_src = src.scanLine(cinfo.next_scanline);
		//				src->render(0, cinfo.next_scanline, width, 1, &tmp_src[0]);
		for (int i = 0; i < width; i++) {
			tmp_rgb[i * 3 + 0] = tmp_src[i * 3 + 0];
			tmp_rgb[i * 3 + 1] = tmp_src[i * 3 + 1];
			tmp_rgb[i * 3 + 2] = tmp_src[i * 3 + 2];
		}
		/* jpeg_write_scanlines expects an array of pointers to scanlines.
				* Here the array is only one element long, but you could pass
				* more than one scanline at a time if that's more convenient.
				*/
		row_pointer[0] = &tmp_rgb[0]; //& image_buffer[cinfo.next_scanline * row_stride];
		(void) jpeg_write_scanlines(&cinfo, row_pointer, 1);
	}

	/* Step 6: Finish compression */

	jpeg_finish_compress(&cinfo);

	/* Step 7: release JPEG compression object */

	/* This is an important step since it will release a good deal of memory. */
	jpeg_destroy_compress(&cinfo);

	/* And we're done! */

	return true;
}

bool write_jpeg(std::vector<uint8_t> *vec, euclase::Image const &src)
{
	DESTINATION dest;
	dest.fn = [](char const *p, int n, void *cookie){
		auto *vec = (std::vector<uint8_t> *)cookie;
		vec->insert(vec->end(), p, p + n);
		return n;
	};
	dest.cookie = vec;

	const int quality = 75;
	bool success = _write_jpeg(src, quality, &dest);

	return success;
}


bool write_jpeg(euclase::Image const &src, char const *filename)
{
	FILE *outfile;		/* target file */
	if ((outfile = fopen(filename, "wb")) == NULL) {
		fprintf(stderr, "can't open %s\n", filename);
		return false;
	}

	DESTINATION dest;
	dest.fn = [](char const *p, int n, void *cookie){
		FILE *fp = (FILE *)cookie;
		return fwrite(p, 1, n, fp);
	};
	dest.cookie = outfile;

	const int quality = 75;
	bool success = _write_jpeg(src, quality, &dest);

	fclose(outfile);

	return success;
}








struct SOURCE {
	std::function<int (char const *p, int n, void *cookie)> fn;
	void *cookie;
};


static inline int MYWRITE(SOURCE *src, void const *ptr, size_t len)
{
	return src->fn((char const *)ptr, len, src->cookie);
}




/* Expanded data source object for stdio output */

typedef struct {
	struct jpeg_source_mgr pub; /* public fields */

	SOURCE *outfile;		/* target stream */
//	JOCTET *buffer;		/* start of buffer */
} my_source_mgr;

typedef my_source_mgr * my_src_ptr;

#define OUTPUT_BUF_SIZE  4096	/* choose an efficiently fwrite'able size */

//
METHODDEF(void)
init_source (j_decompress_ptr cinfo)
{
	my_src_ptr src = (my_src_ptr)cinfo->src;

	/* Allocate the output buffer --- it will be released when done with image */
//	src->buffer = (JOCTET *)
//		(*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_IMAGE,
//								   OUTPUT_BUF_SIZE * SIZEOF(JOCTET));
	src->pub.next_input_byte = nullptr;
	src->pub.bytes_in_buffer = 0;
}

//
METHODDEF(boolean)
fill_input_buffer(j_decompress_ptr cinfo)
{
	my_src_ptr src = (my_src_ptr)cinfo->src;

}

METHODDEF(void)
skip_input_data(j_decompress_ptr cinfo, long num_bytes)
{
	my_src_ptr src = (my_src_ptr)cinfo->src;

}

METHODDEF(boolean)
resync_to_restart(j_decompress_ptr cinfo, int desired)
{
	my_src_ptr src = (my_src_ptr)cinfo->src;

}

//
METHODDEF(void)
term_source (j_decompress_ptr cinfo)
{
	my_src_ptr src = (my_src_ptr) cinfo->src;
//	size_t datacount = OUTPUT_BUF_SIZE - src->pub.free_in_buffer;

	/* Write any data remaining in the buffer */
//	if (datacount > 0) {
//		if (MYWRITE(src->outfile, src->buffer, datacount) != datacount)
//			ERREXIT(cinfo, JERR_FILE_WRITE);
//	}
}

//
METHODDEF(void)
jpeg_my_src(j_decompress_ptr cinfo, SOURCE * outfile)
{
	my_src_ptr src;

	if (cinfo->src == NULL) {	/* first time for this JPEG object? */
		cinfo->src = (struct jpeg_source_mgr *)(*cinfo->mem->alloc_small)((j_common_ptr) cinfo, JPOOL_PERMANENT, SIZEOF(my_destination_mgr));
	}

	src = (my_src_ptr) cinfo->src;
	src->pub.init_source = init_source;
	src->pub.fill_input_buffer = fill_input_buffer;
	src->pub.skip_input_data = skip_input_data;
	src->pub.resync_to_restart = resync_to_restart;
	src->pub.term_source = term_source;
	src->outfile = outfile;
}










void my_error_exit(j_common_ptr cinfo)
{
	throw 0;
}

static int min(int a, int b)
{
	return a < b ? a : b;
}


bool _load_jpeg(euclase::Image *dst, char const *filename)
{


	/* This struct contains the JPEG decompression parameters and pointers to
			* working space (which is allocated as needed by the JPEG library).
			*/
	struct jpeg_decompress_struct cinfo;
	/* We use our private extension JPEG error handler.
			* Note that this struct must live as long as the main JPEG parameter
			* struct, to avoid dangling-pointer problems.
			*/
	struct jpeg_error_mgr jerr;
	/* More stuff */
	FILE * infile;		/* source file */
	//JSAMPARRAY buffer;		/* Output row buffer */
	int row_stride;		/* physical row width in output buffer */

	/* In this example we want to open the input file before doing anything else,
			* so that the setjmp() error recovery below can assume the file is open.
			* VERY IMPORTANT: use "b" option to fopen() if you are on a machine that
			* requires it in order to read binary files.
			*/

	if ((infile = fopen(filename, "rb")) == NULL) {
		fprintf(stderr, "can't open %s\n", filename);
		return false;
	}

	/* Step 1: allocate and initialize JPEG decompression object */

	/* We set up the normal JPEG error routines, then override error_exit. */
	cinfo.err = jpeg_std_error(&jerr);
	jerr.error_exit = my_error_exit;
	/* Establish the setjmp return context for my_error_exit to use. */
	//if (setjmp(jerr.setjmp_buffer)) {
	//	/* If we get here, the JPEG code has signaled an error.
	//	* We need to clean up the JPEG object, close the input file, and return.
	//	*/
	//	jpeg_destroy_decompress(&cinfo);
	//	fclose(infile);
	//	return 0;
	//}
	/* Now we can initialize the JPEG decompression object. */
	jpeg_create_decompress(&cinfo);

	/* Step 2: specify data source (eg, a file) */

	jpeg_stdio_src(&cinfo, infile);

	/* Step 3: read file parameters with jpeg_read_header() */

	(void) jpeg_read_header(&cinfo, TRUE);
	/* We can ignore the return value from jpeg_read_header since
			*   (a) suspension is not possible with the stdio data source, and
			*   (b) we passed TRUE to reject a tables-only JPEG file as an error.
			* See libjpeg.doc for more info.
			*/

	/* Step 4: set parameters for decompression */

	/* In this example, we don't need to change any of the defaults set by
			* jpeg_read_header(), so we do nothing here.
			*/

	/* Step 5: Start decompressor */

	(void) jpeg_start_decompress(&cinfo);
	/* We can ignore the return value since suspension is not possible
			* with the stdio data source.
			*/

	int width = cinfo.output_width;
	int height = cinfo.output_height;

	/* We may need to do some setup of our own at this point before reading
			* the data.  After jpeg_start_decompress() we have the correct scaled
			* output image dimensions available, as well as the output colormap
			* if we asked for color quantization.
			* In this example, we need to make an output work buffer of the right size.
			*/
	/* JSAMPLEs per row in output buffer */
	row_stride = width * cinfo.output_components;
	/* Make a one-row-high sample array that will go away when done with image */
	//buffer = (*cinfo.mem->alloc_sarray)
	//	((j_common_ptr) &cinfo, JPOOL_IMAGE, row_stride, 1);

	/* Step 6: while (scan lines remain to be read) */
	/*           jpeg_read_scanlines(...); */

	std::vector<uint8_t> tmp_rgb(row_stride);

	*dst = euclase::Image(width, height, euclase::Image::Format_8_RGB);

	/* Here we use the library's state variable cinfo.output_scanline as the
			* loop counter, so that we don't have to keep track ourselves.
			*/
	for (int y = 0; y < height; y++) {
		/* jpeg_read_scanlines expects an array of pointers to scanlines.
				* Here the array is only one element long, but you could ask for
				* more than one scanline at a time if that's more convenient.
				*/
		uint8_t *dstptr = dst->scanLine(y);
		uint8_t *srcptr = &tmp_rgb[0];
		(void) jpeg_read_scanlines(&cinfo, &srcptr, 1);
		if (cinfo.output_components == 1) {
			for (int i = 0; i < width; i++) {
				int y = srcptr[0];
				dstptr[0] = y;
				dstptr[1] = y;
				dstptr[2] = y;
				srcptr++;
				dstptr += 3;
			}
		} else if (cinfo.output_components == 3) {
			for (int i = 0; i < width; i++) {
				dstptr[0] = srcptr[0];
				dstptr[1] = srcptr[1];
				dstptr[2] = srcptr[2];
				srcptr += 3;
				dstptr += 3;
			}
		} else if (cinfo.output_components == 4) {
			for (int i = 0; i < width; i++) {
				int c = srcptr[0];
				int m = srcptr[1];
				int y = srcptr[2];
				int k = srcptr[3];
				dstptr[0] = k * (c) * (228 + m * 27 / 255) / 255 / 255;
				dstptr[1] = k * (160 + c * 95 / 255) * (m) * (241 + y * 14 / 255) / 255 / 255 / 255;
				dstptr[2] = k * (233 + c * 22 / 255) * (127 + m * 128 / 255) * (y) / 255 / 255 / 255;
				srcptr += 4;
				dstptr += 3;
			}
		}

		/* Assume put_scanline_someplace wants a pointer and sample count. */
		//put_scanline_someplace(buffer[0], row_stride);
		//				dst->put(0, y, &tmp_dst[0], width, 0, 0, width, 1);
	}

	/* Step 7: Finish decompression */

	(void) jpeg_finish_decompress(&cinfo);
	/* We can ignore the return value since suspension is not possible
			* with the stdio data source.
			*/

	/* Step 8: Release JPEG decompression object */

	/* This is an important step since it will release a good deal of memory. */
	jpeg_destroy_decompress(&cinfo);

	/* After finish_decompress, we can close the input file.
			* Here we postpone it until after no more JPEG errors are possible,
			* so as to simplify the setjmp error logic above.  (Actually, I don't
			* think that jpeg_destroy can do an error exit, but why assume anything...)
			*/
	fclose(infile);

	/* And we're done! */
	return true;
}
}

