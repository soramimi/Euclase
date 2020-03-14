#ifndef ANTIALIAS_H
#define ANTIALIAS_H

class QImage;

namespace euclase {
class Image;
}

bool filter_antialias(euclase::Image *image);

#endif // ANTIALIAS_H
