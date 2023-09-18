#ifndef IMAGE_H
#define IMAGE_H

#include <functional>

class QImage;

namespace euclase {
class Image;
}

enum class EnlargeMethod {
	NearestNeighbor,
	Bilinear,
	Bicubic,
};

euclase::Image resizeImage(euclase::Image const &image, int dst_w, int dst_h, EnlargeMethod method/* = EnlargeMethod::Bilinear*/);
euclase::Image filter_blur(euclase::Image image, int radius, bool *cancel, std::function<void (float)> progress);

#endif // IMAGE_H
