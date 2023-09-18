#ifndef BRUSHSLIDER_H
#define BRUSHSLIDER_H

#include "RingSlider.h"

class BrushSlider : public RingSlider {
	Q_OBJECT
public:
	enum VisualType {
		SIZE,
		SOFTNESS,
	};
private:
	QImage image_;
	VisualType visual_type_ = SIZE;
protected:
	QImage generateSliderImage() override;
public:
	explicit BrushSlider(QWidget *parent = nullptr);
	VisualType visualType() const;
	void setVisualType(VisualType type);
};

#endif // BRUSHSLIDER_H
