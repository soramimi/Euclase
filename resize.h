#ifndef IMAGE_H
#define IMAGE_H

class QImage;

enum class EnlargeMethod {
	Nearest,
	Bilinear,
	Bicubic,
};

QImage resizeImage(QImage const &image, int dst_w, int dst_h, EnlargeMethod method/* = EnlargeMethod::Bilinear*/, bool alphachannel/* = true*/, bool gamma_correction);

#endif // IMAGE_H
