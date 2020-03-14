#ifndef DOCUMENT_H
#define DOCUMENT_H

#include <QImage>
#include <QPoint>
#include <memory>
#include <functional>
#include <QMutex>
#include <QColor>
#include "euclase.h"

class Document {
public:
	enum class Type {
		Image,
		Block,
	};
	struct Header {
		unsigned int ref_ = 0;
		Type type_ = Type::Image;
		QPoint offset_;

		QPoint offset() const
		{
			return offset_;
		}
	};

	struct Image {
		Header header_;
		QImage image_;
		enum Format {
			RGB8,
			RGBA8,
			Grayscale8,
			GrayscaleA8,
			RGBF,
			RGBAF,
			GrayscaleF8,
			GrayscaleAF8,
		};
		Format format_ = Format::RGBA8;
		bool linear_ = false;
		std::shared_ptr<std::vector<uint8_t>> data_;

		bool isNull() const
		{
			return image_.isNull();
		}

		void make2(int width, int height, QImage::Format format)
		{
			switch (format) {
			case QImage::Format_RGB32:
				format_ = Format::RGB8;
				data_ = std::make_shared<std::vector<uint8_t>>(width * height * 3);
				break;
			case QImage::Format_RGB888:
				format_ = Format::RGB8;
				data_ = std::make_shared<std::vector<uint8_t>>(width * height * 3);
				break;
			case QImage::Format_RGBA8888:
				format_ = Format::RGBA8;
				data_ = std::make_shared<std::vector<uint8_t>>(width * height * 4);
				break;
			case QImage::Format_Grayscale8:
				format_ = Format::Grayscale8;
				data_ = std::make_shared<std::vector<uint8_t>>(width * height);
				break;
			default:
				data_.reset();
			}
		}

		void make(int width, int height, QImage::Format format)
		{
			image_ = QImage(width, height, format);
			make2(width, height, format);
		}

		void make(QSize const &sz, QImage::Format format)
		{
			make(sz.width(), sz.height(), format);
		}

		uint8_t *scanLine2(int y)
		{
			uint8_t *p = data_->data();
			int w = width();
			switch (format_) {
			case Format::RGB8:
				return p + 3 * w * y;
			case Format::RGBA8:
				return p + 4 * w * y;
			case Format::Grayscale8:
				return p + w * y;
			}
			return nullptr;
		}

		void fill(const QColor &color)
		{
			image_.fill(color);
			int w = width();
			int h = height();
			switch (format_) {
			case Format::RGB8:
				for (int y = 0; y < h; y++) {
					uint8_t *p =scanLine2(y);
					for (int x = 0; x < w; x++) {
						p[x * 3 + 0] = color.red();
						p[x * 3 + 1] = color.green();
						p[x * 3 + 2] = color.blue();
					}
				}
				break;
			case Format::RGBA8:
				for (int y = 0; y < h; y++) {
					uint8_t *p =scanLine2(y);
					for (int x = 0; x < w; x++) {
						p[x * 4 + 0] = color.red();
						p[x * 4 + 1] = color.green();
						p[x * 4 + 2] = color.blue();
						p[x * 4 + 4] = color.alpha();
					}
				}
				break;
			case Format::Grayscale8:
				for (int y = 0; y < h; y++) {
					uint8_t *p =scanLine2(y);
					for (int x = 0; x < w; x++) {
						p[x] = euclase::gray(color.red(), color.green(), color.blue());
					}
				}
				break;
			}
		}

		void setImage(QImage const &image)
		{
			image_ = image;
			int w = width();
			int h = height();
			make2(w, h, image.format());
			switch (format_) {
			case Format::RGB8:
				for (int y = 0; y < h; y++) {
					uint8_t const *s = image.scanLine(y);
					uint8_t *d = scanLine2(y);
					memcpy(d, s, w * 3);
				}
				return;
			case Format::RGBA8:
				for (int y = 0; y < h; y++) {
					uint8_t const *s = image.scanLine(y);
					uint8_t *d = scanLine2(y);
					memcpy(d, s, w * 4);
				}
				return;
			case Format::Grayscale8:
				for (int y = 0; y < h; y++) {
					uint8_t const *s = image.scanLine(y);
					uint8_t *d = scanLine2(y);
					memcpy(d, s, w);
				}
				return;
			}
		}

		QImage &getImage()
		{
			return image_;
		}

		QImage const &getImage() const
		{
			return image_;
		}

		QImage copyImage() const
		{
			return image_.copy();
		}

		Image scaled(int w, int h) const
		{
			Image newimage;
			QImage img = image_.scaled(w, h);
			newimage.setImage(img);
			return newimage;
		}

		QImage::Format format() const
		{
			return image_.format();
		}

		uint8_t *scanLine(int y)
		{
			return image_.scanLine(y);
		}

		uint8_t const *scanLine(int y) const
		{
			return image_.scanLine(y);
		}

		QPoint offset() const
		{
			return header_.offset();
		}

		void setOffset(QPoint const &pt)
		{
			header_.offset_ = pt;
		}

		void setOffset(int x, int y)
		{
			setOffset(QPoint(x, y));
		}

		int width() const
		{
			return image_.width();
		}

		int height() const
		{
			return image_.height();
		}

		QSize size() const
		{
			return image_.size();
		}
		bool isRGBA8888() const
		{
			return image_.format() == QImage::Format_RGBA8888;
		}

		bool isGrayscale8() const
		{
			return image_.format() == QImage::Format_Grayscale8;
		}
	};
//	struct Block {
//		Header header_;

//		QPoint offset() const
//		{
//			return header_.offset();
//		}
//	};

	class PanelPtr {
	private:
		Header *object_ = nullptr;
	public:
		PanelPtr()
		{
		}
		PanelPtr(PanelPtr const &r)
		{
			assign(r.object_);
		}
		void operator = (PanelPtr const &r)
		{
			assign(r.object_);
		}
		~PanelPtr()
		{
			reset();
		}
		void reset()
		{
			assign(nullptr);
		}
		void assign(Header *p)
		{
			if (p == object_) {
				return;
			}
			if (p) {
				p->ref_++;
			}
			if (object_) {
				if (object_->ref_ > 1) {
					object_->ref_--;
				} else {
					switch (object_->type_) {
					case Type::Image:
						reinterpret_cast<Image *>(object_)->~Image();
						break;
//					case Type::Block:
//						reinterpret_cast<Block *>(object_)->~Block();
//						break;
					}
					free(reinterpret_cast<void *>(object_));
				}
			}
			object_ = p;
		}

		Image *image()
		{
			if (object_ && reinterpret_cast<Header *>(object_)->type_ == Type::Image) {
				return reinterpret_cast<Image *>(object_);
			}
			return nullptr;
		}
		Image const *image() const
		{
			if (object_ && reinterpret_cast<Header *>(object_)->type_ == Type::Image) {
				return reinterpret_cast<Image *>(object_);
			}
			return nullptr;
		}
		Image *operator -> ()
		{
			return image();
		}
		Image const *operator -> () const
		{
			return image();
		}
		operator Image *()
		{
			return image();
		}
		operator const Image *() const
		{
			return image();
		}

//		Block *block()
//		{
//			if (object_ && reinterpret_cast<Header *>(object_)->type_ == Type::Block) {
//				return reinterpret_cast<Block *>(object_);
//			}
//			return nullptr;
//		}
//		Block const *block() const
//		{
//			if (object_ && reinterpret_cast<Header *>(object_)->type_ == Type::Block) {
//				return reinterpret_cast<Block *>(object_);
//			}
//			return nullptr;
//		}
//		Block *operator -> ()
//		{
//			return block();
//		}
//		Block const *operator -> () const
//		{
//			return block();
//		}
//		operator Block *()
//		{
//			return block();
//		}
//		operator const Block *() const
//		{
//			return block();
//		}

		bool isImage() const
		{
			return image();
		}
//		bool isBlock() const
//		{
//			return block();
//		}
		static PanelPtr makeImage()
		{
			void *o = malloc(sizeof(Image));
			if (!o) throw std::bad_alloc();
			new(o) Image();
			PanelPtr ptr;
			ptr.assign(reinterpret_cast<Header *>(o));
			return ptr;
		}
		operator bool ()
		{
			return object_;
		}
		PanelPtr copy() const
		{
			if (!object_) return {};
			void *o = malloc(sizeof(Image));
			if (!o) throw std::bad_alloc();
			new(o) Image(*reinterpret_cast<Image const *>(object_));
			Image *p = reinterpret_cast<Image *>(o);
			p->header_.ref_ = 0;
			PanelPtr ptr;
			ptr.assign(&p->header_);
			return ptr;
		}
	};

	class Layer {
	public:
		QPoint offset_;
		bool tile_mode_ = false;
		std::vector<PanelPtr> panels_;

		void clear(QMutex *sync)
		{
			if (sync) sync->lock();

			offset_ = QPoint();
			panels_.clear();

			if (sync) sync->unlock();
		}

		PanelPtr addImagePanel(int x = 0, int y = 0, int w = 64, int h = 64)
		{
			auto panel = PanelPtr::makeImage();
			panel->setOffset(x, y);
			if (w > 0 && h > 0) {
				panel->make(w, h, QImage::Format_RGBA8888);
				panel->fill(Qt::transparent);
			}
			panels_.push_back(panel);
			std::sort(panels_.begin(), panels_.end(), [](PanelPtr const &l, PanelPtr const &r){
				auto COMP = [](PanelPtr const &l, PanelPtr const &r){
					if (l->offset().y() < r->offset().y()) return -1;
					if (l->offset().y() > r->offset().y()) return 1;
					if (l->offset().x() < r->offset().x()) return -1;
					if (l->offset().x() > r->offset().x()) return 1;
					return 0;
				};
				return COMP(l, r) < 0;
			});
			return panel;
		}

		Layer() = default;

		QPoint const &offset() const
		{
			return offset_;
		}

		void setOffset(QPoint const &o)
		{
			offset_ = o;
		}

		void eachPanel(std::function<void(Image *)> const &fn)
		{
			for (PanelPtr &ptr : panels_) {
				fn(ptr.image());
			}
		}

		void setImage(QPoint const &offset, QImage const &image)
		{
			clear(nullptr);
			offset_ = offset;
			addImagePanel();
			panels_[0]->setImage(image);
		}

		QRect rect() const;
	};

	struct RenderOption {
		enum Mode {
			Default,
			DirectCopy,
		};
		Mode mode = Default;
		QColor brush_color;
	};

	struct Private;
	Private *m;

	Document();
	~Document();

	int width() const;
	int height() const;
	QSize size() const;
	void setSize(QSize const &s);
	Layer *current_layer();
	Layer *selection_layer();
	Layer *current_layer() const;
	Layer *selection_layer() const;

	void paintToCurrentLayer(const Layer &source, const RenderOption &opt, QMutex *sync, bool *abort);

	Image renderToLayer(QRect const &r, bool quickmask, QMutex *sync, bool *abort) const;
private:
	static void renderToEachPanels_(Image *target_panel, const QPoint &target_offset, const Layer &input_layer, Layer *mask_layer, const QColor &brush_color, int opacity, bool *abort);
	static void renderToEachPanels(Image *target_panel, const QPoint &target_offset, const Layer &input_layer, Layer *mask_layer, const QColor &brush_color, int opacity, QMutex *sync, bool *abort);
public:
	enum class SelectionOperation {
		SetSelection,
		AddSelection,
		SubSelection,
	};
	static void renderToSinglePanel(Image *target_panel, const QPoint &target_offset, const Image *input_panel, const QPoint &input_offset, const Layer *mask_layer, RenderOption const &opt, const QColor &brush_color, int opacity = 255, bool *abort = nullptr);
	static void renderToLayer(Layer *target_layer, const Layer &input_layer, Layer *mask_layer, const RenderOption &opt, QMutex *sync, bool *abort);
	void clearSelection(QMutex *sync);
	void addSelection(const Layer &source, const RenderOption &opt, QMutex *sync, bool *abort);
	void subSelection(const Layer &source, const RenderOption &opt, QMutex *sync, bool *abort);
	Image renderSelection(const QRect &r, QMutex *sync, bool *abort) const;
	void changeSelection(SelectionOperation op, QRect const &rect, QMutex *sync);
	Image crop(const QRect &r, QMutex *sync, bool *abort) const;
	void crop2(const QRect &r);
	void clear(QMutex *sync);
};

#endif // DOCUMENT_H
