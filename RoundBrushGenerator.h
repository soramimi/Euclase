#ifndef ROUNDBRUSHGENERATOR_H
#define ROUNDBRUSHGENERATOR_H

#include "euclase.h"
class Brush {
public:
	float size = 200;
	float softness = 1;
};

class RoundBrushGenerator {
private:
public:
	float radius;
	float blur;
	float mul;
public:
	RoundBrushGenerator(float size, float softness);
	float level(float x, float y);
	euclase::Image image(int w, int h, float cx, float cy, const QColor &color) const;
};


#endif // ROUNDBRUSHGENERATOR_H
