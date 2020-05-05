#ifndef IMAGE_H
#define IMAGE_H

class QImage;

namespace euclase {
class Image;
}

enum class EnlargeMethod {
	NearestNeighbor,
	Bilinear,
	Bicubic,
};

euclase::Image resizeImage(euclase::Image const &image, int dst_w, int dst_h, EnlargeMethod method/* = EnlargeMethod::Bilinear*/, bool alphachannel/* = true*/, bool gamma_correction);
euclase::Image filter_blur(euclase::Image image, int radius, bool gamma_correction, bool *cancel_request);

#endif // IMAGE_H
