
#ifndef MEDIAN_H_
#define MEDIAN_H_

#include <QImage>

namespace euclase {
class Image;
}

euclase::Image filter_median(euclase::Image image, int radius);
euclase::Image filter_maximize(euclase::Image image, int radius);
euclase::Image filter_minimize(euclase::Image image, int radius);

#endif

