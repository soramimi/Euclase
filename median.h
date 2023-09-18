
#ifndef MEDIAN_H_
#define MEDIAN_H_

#include "FilterStatus.h"

#include <QImage>

namespace euclase {
class Image;
}

euclase::Image filter_median(const euclase::Image &image, int radius, FilterStatus *status);
euclase::Image filter_maximize(const euclase::Image &image, int radius, FilterStatus *status);
euclase::Image filter_minimize(const euclase::Image &image, int radius, FilterStatus *status);

#endif

