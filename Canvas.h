#ifndef CANVAS_H
#define CANVAS_H

#include "Bounds.h"
#include "euclase.h"
#include <QColor>
#include <QImage>
#include <QMutex>
#include <QPoint>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>

static const int PANEL_SIZE = 256; // must be power of two

class Canvas {
	friend class LayerComposer;
	friend class MainWindow;//@todo
	friend class ImageViewWidget;//@todo
public:
	class Panel {
	private:
		struct Data {
			euclase::Image image;
			struct {
				QPoint offset;
			} extra;
		};
		Data data_;
	public:
		Panel() = default;
		Panel(euclase::Image const &image, QPoint const &offset = {})
		{
			data_.image = image;
			data_.extra.offset = offset;
		}
		void reset()
		{
			data_ = {};
		}

		euclase::Image *imagep()
		{
			return &data_.image;
		}
		euclase::Image const *imagep() const
		{
			return &data_.image;
		}

		euclase::Image const &image() const
		{
			return data_.image;
		}

		bool isNull() const
		{
			return !data_.image;
		}

		operator bool () const
		{
			return !isNull();
		}

		euclase::Image *operator -> ()
		{
			return imagep();
		}
		euclase::Image const *operator -> () const
		{
			return imagep();
		}

		bool isImage() const
		{
			return !isNull();
		}

		euclase::ImageFormat format() const
		{
			return data_.image.format();
		}

		int width() const
		{
			return data_.image.width();
		}

		int height() const
		{
			return data_.image.height();
		}

		QSize size() const
		{
			return {width(), height()};
		}

		uint8_t *scanLine(int y)
		{
			return data_.image.scanLine(y);
		}

		uint8_t const *scanLine(int y) const
		{
			return data_.image.scanLine(y);
		}

		QPoint offset() const
		{
			return data_.extra.offset;
		}

		void setOffset(QPoint const &pt)
		{
			data_.extra.offset = pt;
		}

		void setOffset(int x, int y)
		{
			data_.extra.offset = {x, y};
		}

		Panel copy() const
		{
			Panel t;
			t.data_.image = data_.image.copy();
			t.data_.extra = data_.extra;
			return t;
		}

		void convertToFormat(euclase::ImageFormat format)
		{
			*imagep() = image().convertToFormat(format);
		}
	};

	enum ActivePanel {
		PrimaryLayer,
		AlternateLayer,
		AlternateSelection,
	};

	enum class BlendMode {
		Disable,
		Normal,
		Replace,
		Eraser,
	};

	class Layer;

	struct RenderOption {
		enum class MaskBlend { Replace, Add, Subtract };

		BlendMode blend_mode = BlendMode::Normal;
		ActivePanel active_panel = PrimaryLayer;
		QColor brush_color;
		QRect mask_rect;
		bool use_mask = false;
		std::function<void (QRect const &rect)> notify_changed_rect;
		MaskBlend mask_blend = MaskBlend::Replace;
	};

	struct RenderOption2 {
		RenderOption opt1;
		const Canvas::Layer *selection_layer = nullptr;
	};

	class Layer {
	public:
		ActivePanel active_panel_ = PrimaryLayer;
		QPoint offset_;
		euclase::Image::MemoryType memtype_ = euclase::Image::Host;
		euclase::ImageFormat format_ = euclase::ImageFormat::Format_Invalid;
		std::vector<Panel> primary_panels;
		std::vector<Panel> alternate_panels;
		std::vector<Panel> alternate_selection_panels; // grayscale mask

		BlendMode alternate_blend_mode = BlendMode::Normal;

		std::vector<Panel> *panels(ActivePanel active = PrimaryLayer)
		{
			switch (active) {
			case PrimaryLayer:
				return &primary_panels;
			case AlternateLayer:
				return &alternate_panels;
			case AlternateSelection:
				return &alternate_selection_panels;
			}
			return nullptr;
		}

		std::vector<Panel> const *panels(ActivePanel active = PrimaryLayer) const
		{
            return const_cast<Layer *>(this)->panels(active);
		}

		int panelCount(ActivePanel alternate = PrimaryLayer) const
		{
			const auto *list = panels(alternate);
			return list ? int(list->size()) : 0;
		}

		Panel const &panel(ActivePanel alternate, int i) const
		{
			const auto *list = panels(alternate);
			if (!list) throw std::out_of_range("Canvas layer panel type");
			return list->at(i);
		}

		void clear()
		{
			active_panel_ = PrimaryLayer;
			offset_ = QPoint();
			primary_panels.clear();
			alternate_panels.clear();
			alternate_selection_panels.clear();
		}

		static void remove(std::vector<Panel> *panels, QPoint const &offset)
		{
			std::vector<Panel> newpanels;
			newpanels.reserve(panels->size());
			for (Panel &panel : *panels) {
				if (panel.offset() != offset) {
					newpanels.push_back(panel);
				}
			}
			std::swap(*panels, newpanels);
		}

		static void sort(std::vector<Panel> *panels)
		{
			std::sort(panels->begin(), panels->end(), [](Panel const &l, Panel const &r){
				auto COMP = [](Panel const &l, Panel const &r){
					if (l.offset().y() < r.offset().y()) return -1;
					if (l.offset().y() > r.offset().y()) return 1;
					if (l.offset().x() < r.offset().x()) return -1;
					if (l.offset().x() > r.offset().x()) return 1;
					return 0;
				};
				return COMP(l, r) < 0;
			});
		}

		static void add(std::vector<Panel> *panels, Panel const &panel)
		{
			remove(panels, panel.offset());
			panels->push_back(panel);
			sort(panels);
		}

		static Panel *addImagePanel(std::vector<Panel> *panels, int x, int y, int w, int h, euclase::ImageFormat format, euclase::Image::MemoryType memtype);

		Layer() = default;

		QPoint const &offset() const
		{
			return offset_;
		}

		void setOffset(QPoint const &o)
		{
			offset_ = o;
		}

		void eachPanel(std::function<void(Panel *)> const &fn)
		{
			for (Panel &ptr : *panels()) {
				fn(&ptr);
			}
		}

		void setImage(QPoint const &offset, euclase::Image const &image)
		{
			clear();
			if (image.isNull()) return;

			format_ = image.format();
			memtype_ = image.memtype();

			Panel p;
			*p.imagep() = image;
			primary_panels.push_back(p);

			setOffset(offset);
		}

		bool finishAlternatePanels(bool apply, const Layer *mask_layer, const RenderOption &opt);
		void setAlternateOption(BlendMode blendmode);

		QRect rect() const;
		static Canvas::Panel *addPanel(std::vector<Panel> *panels, Panel &&panel);
	};
	using LayerPtr = std::shared_ptr<Layer>;

	struct Private;
	Private *m;

	Canvas();
	~Canvas();
	Canvas(const Canvas &) = delete;
	Canvas &operator=(const Canvas &) = delete;

	int width() const;
	int height() const;
	QSize size() const;
	bool setSize(QSize const &s);
	Layer *layer(int index);
	Layer const *layer(int index) const;
	Layer *current_layer();
	Layer const *current_layer() const;
	Layer *selection_layer();
	const Layer *selection_layer() const;

	bool paintToCurrentLayer(const Layer &source, const RenderOption &opt, const std::atomic_bool *abort);
	bool paintToCurrentAlternate(const Layer &source, const RenderOption &opt, const std::atomic_bool *abort);

	enum InputLayerMode {
		AllLayers,
		CurrentLayerOnly,
	};
	Panel renderToPanel(InputLayerMode input_layer_mode, euclase::ImageFormat format, QRect const &r, QRect const &maskrect, ActivePanel activepanel, RenderOption const &opt, const std::atomic_bool *abort) const;

	static bool renderToSinglePanel(Panel *target_panel, const QPoint &target_offset, const Panel *input_panel, const QPoint &input_offset, const Layer *mask_layer, RenderOption const &opt, const QColor &brush_color, int opacity = 255, const std::atomic_bool *abort = nullptr);
	static bool renderToLayer(Layer *target_layer, ActivePanel activepanel, const Layer &input_layer, const Layer *mask_layer, const RenderOption &opt, const std::atomic_bool *abort);
private:
	static bool renderToEachPanel(Panel *target_panel, const Layer &input_layer, const Layer *mask_layer, const RenderOption &opt, const std::atomic_bool *abort);
	static bool selectionMaskForPanel(Panel *mask, const Panel &tile, const QPoint &layer_offset,
									 const Layer &selection, const std::atomic_bool *abort);
	static bool composeAlternatePanel(Panel &result, const Layer &layer, const Panel *primary, const Panel &alternate,
									  const Layer *selection, BlendMode mode, const std::atomic_bool *abort);
	static bool composePanel(Panel *target_panel, const Panel *alt_panel, const Panel *alt_mask, BlendMode mode);
public:
	enum class SelectionOperation {
		SetSelection,
		AddSelection,
		SubSelection,
	};
	void clearSelection();
	bool addSelection(const Layer &source, const RenderOption &opt, const std::atomic_bool *abort);
	bool subSelection(const Layer &source, const RenderOption &opt, const std::atomic_bool *abort);
	Panel renderSelection(const QRect &r, const std::atomic_bool *abort) const;
	Panel crop(const QRect &r, const std::atomic_bool *abort) const;
	bool trim(const QRect &r);
	void clear();
	int addNewLayer();
	static LayerPtr newLayer();
	bool setCurrentLayer(int index);

	class RectangleSelection {
	public:
		virtual ~RectangleSelection() = default;
		virtual QImage make(QRect const &rect) const;
	};
	class EllipseSelection : public RectangleSelection {
	public:
		~EllipseSelection() override = default;
		QImage make(QRect const &rect) const override;
	};
	bool changeSelection(SelectionOperation op, QRect const &rect, Bounds::Type bounds_type);
};

euclase::Image cropImage(euclase::Image const &srcimg, int sx, int sy, int sw, int sh);

#endif // CANVAS_H
