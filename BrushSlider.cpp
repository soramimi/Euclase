#include "BrushSlider.h"

BrushSlider::BrushSlider(QWidget *parent)
	: RingSlider(parent)
{
	setVisualType(SIZE);
}

BrushSlider::VisualType BrushSlider::visualType() const
{
	return visual_type_;
}

void BrushSlider::setVisualType(VisualType type)
{
	visual_type_ = type;

	int max = (visual_type_ == SIZE) ? 1000 : 100;
	setMaximum(max);

	update();
}

QImage BrushSlider::generateSliderImage()
{
	if (image_.isNull()) {
		switch (visual_type_) {
		case VisualType::SIZE:
			image_.load(":/image/size.png");
			break;
		case VisualType::SOFTNESS:
			image_.load(":/image/softness.png");
			break;
		}
	}
	return image_.scaled(sliderImageSize(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

