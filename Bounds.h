#ifndef BOUNDS_H
#define BOUNDS_H

#include <variant>

class Bounds {
public:
	class Rectangle {
	public:
		Rectangle() = default;
	};

	class Ellipse {
	public:
		Ellipse() = default;
	};

	typedef std::variant<
		Rectangle,
		Ellipse
		> variant_t;
};

#endif // BOUNDS_H
