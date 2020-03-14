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

	class PanelPtr {
	private:
		euclase::ImageHeader *object_ = nullptr;
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
		void assign(euclase::ImageHeader *p)
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
					reinterpret_cast<euclase::Image *>(object_)->~Image();
					free(reinterpret_cast<void *>(object_));
				}
			}
			object_ = p;
		}

		euclase::Image *image()
		{
			if (object_) {
				return reinterpret_cast<euclase::Image *>(object_);
			}
			return nullptr;
		}
		euclase::Image const *image() const
		{
			if (object_) {
				return reinterpret_cast<euclase::Image *>(object_);
			}
			return nullptr;
		}
		euclase::Image *operator -> ()
		{
			return image();
		}
		euclase::Image const *operator -> () const
		{
			return image();
		}
		operator euclase::Image *()
		{
			return image();
		}
		operator const euclase::Image *() const
		{
			return image();
		}

		bool isImage() const
		{
			return image();
		}
		static PanelPtr makeImage()
		{
			void *o = malloc(sizeof(euclase::Image));
			if (!o) throw std::bad_alloc();
			new(o) euclase::Image();
			PanelPtr ptr;
			ptr.assign(reinterpret_cast<euclase::ImageHeader *>(o));
			return ptr;
		}
		operator bool ()
		{
			return object_;
		}
		PanelPtr copy() const
		{
			if (!object_) return {};
			void *o = malloc(sizeof(euclase::Image));
			if (!o) throw std::bad_alloc();
			new(o) euclase::Image(*reinterpret_cast<euclase::Image const *>(object_));
			euclase::Image *p = reinterpret_cast<euclase::Image *>(o);
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

		void eachPanel(std::function<void(euclase::Image *)> const &fn)
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

	euclase::Image renderToLayer(QRect const &r, bool quickmask, QMutex *sync, bool *abort) const;
private:
	static void renderToEachPanels_(euclase::Image *target_panel, const QPoint &target_offset, const Layer &input_layer, Layer *mask_layer, const QColor &brush_color, int opacity, bool *abort);
	static void renderToEachPanels(euclase::Image *target_panel, const QPoint &target_offset, const Layer &input_layer, Layer *mask_layer, const QColor &brush_color, int opacity, QMutex *sync, bool *abort);
public:
	enum class SelectionOperation {
		SetSelection,
		AddSelection,
		SubSelection,
	};
	static void renderToSinglePanel(euclase::Image *target_panel, const QPoint &target_offset, const euclase::Image *input_panel, const QPoint &input_offset, const Layer *mask_layer, RenderOption const &opt, const QColor &brush_color, int opacity = 255, bool *abort = nullptr);
	static void renderToLayer(Layer *target_layer, const Layer &input_layer, Layer *mask_layer, const RenderOption &opt, QMutex *sync, bool *abort);
	void clearSelection(QMutex *sync);
	void addSelection(const Layer &source, const RenderOption &opt, QMutex *sync, bool *abort);
	void subSelection(const Layer &source, const RenderOption &opt, QMutex *sync, bool *abort);
	euclase::Image renderSelection(const QRect &r, QMutex *sync, bool *abort) const;
	void changeSelection(SelectionOperation op, QRect const &rect, QMutex *sync);
	euclase::Image crop(const QRect &r, QMutex *sync, bool *abort) const;
	void crop2(const QRect &r);
	void clear(QMutex *sync);
};

#endif // DOCUMENT_H
