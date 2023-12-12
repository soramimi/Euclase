#ifndef CANVAS_H
#define CANVAS_H

#include "euclase.h"
#include <QColor>
#include <QImage>
#include <QMutex>
#include <QPoint>
#include <functional>
#include <memory>

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

		euclase::Image::Format format() const
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
	};

	class Layer {
	public:
		enum ActivePanel {
			Primary,
			Alternate,
			AlternateSelection,
		};
		ActivePanel active_panel_ = Primary;
		QPoint offset_;
		euclase::Image::MemoryType memtype_ = euclase::Image::Host;
		euclase::Image::Format format_ = euclase::Image::Format_Invalid;
		std::vector<Panel> primary_panels;
		std::vector<Panel> alternate_panels;
		std::vector<Panel> alternate_selection_panels; // grayscale mask

        std::vector<Panel> *panels(Layer::ActivePanel active = Primary)
		{
            switch (active) {
			case Primary:
				return &primary_panels;
			case Alternate:
				return &alternate_panels;
			case AlternateSelection:
				return &alternate_selection_panels;
			}
			Q_ASSERT(0);
		}

        std::vector<Panel> const *panels(Layer::ActivePanel active = Primary) const
		{
            return const_cast<Layer *>(this)->panels(active);
		}

		int panelCount(Layer::ActivePanel alternate = Primary) const
		{
			return (int)panels(alternate)->size();
		}

		Panel const &panel(Layer::ActivePanel alternate, int i) const
		{
			return (*panels(alternate))[i];
		}

		void clear()
		{
			active_panel_ = Primary;
			offset_ = QPoint();
			primary_panels.clear();
			alternate_panels.clear();
			alternate_selection_panels.clear();
		}

		static void remove(std::vector<Panel> *panels, QPoint const &offset)
		{
#if 0
			size_t i = panels->size();
			while (i > 0) {
				if (panels->at(i - 1).offset() == offset) {
					panels->erase(panels->begin() + (i - 1));
				} else {
					i--;
				}
			}
#else
			std::vector<Panel> newpanels;
			newpanels.reserve(panels->size());
			for (Panel &panel : *panels) {
				if (panel.offset() != offset) {
					newpanels.push_back(panel);
				}
			}
			std::swap(*panels, newpanels);
#endif
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

		static Panel *addImagePanel(std::vector<Panel> *panels, int x, int y, int w, int h, euclase::Image::Format format, euclase::Image::MemoryType memtype);

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
			format_ = image.format();
			memtype_ = image.memtype();

			Panel p;
			*p.imagep() = image;
			primary_panels.push_back(p);

			setOffset(offset);
		}

		void finishAlternatePanels(bool apply);

		QRect rect() const;
	};
	using LayerPtr = std::shared_ptr<Layer>;

	struct RenderOption {
		enum Mode {
			Default,
			DirectCopy,
		};
		Mode mode = Default;
		Layer::ActivePanel active_panel = Layer::Alternate;
		QColor brush_color;
		QRect mask_rect;
	};

	struct Private;
	Private *m;

	Canvas();
	~Canvas();

	int width() const;
	int height() const;
	QSize size() const;
	void setSize(QSize const &s);
	Layer *layer(int index);
	Layer *current_layer();
	Layer *selection_layer();
	Layer *current_layer() const;
	Layer *selection_layer() const;

	void paintToCurrentLayer(const Layer &source, const RenderOption &opt, bool *abort);

	enum InputLayer {
		AllLayers,
		CurrentLayerOnly,
	};
	Panel renderToPanel(InputLayer inputlayer, euclase::Image::Format format, QRect const &r, QRect const &maskrect, Layer::ActivePanel activepanel, bool *abort) const;

	static void renderToSinglePanel(Panel *target_panel, const QPoint &target_offset, const Panel *input_panel, const QPoint &input_offset, const Layer *mask_layer, RenderOption const &opt, const QColor &brush_color, int opacity = 255, bool *abort = nullptr);
	static void renderToLayer(Layer *target_layer, Layer::ActivePanel activepanel, const Layer &input_layer, Layer *mask_layer, const RenderOption &opt, bool *abort);
private:
	static void renderToEachPanels_internal_(Panel *target_panel, const QPoint &target_offset, const Layer &input_layer, Layer *mask_layer, const QColor &brush_color, int opacity, RenderOption const &opt, bool *abort);
	static void renderToEachPanels(Panel *target_panel, const QPoint &target_offset, const std::vector<Layer *> &input_layers, Layer *mask_layer, const QColor &brush_color, int opacity, const RenderOption &opt, bool *abort);
	static void composePanel(Panel *target_panel, const Panel *alt_panel, const Panel *alt_mask);
	static void composePanels(Panel *target_panel, std::vector<Panel> const *alternate_panels, std::vector<Panel> const *alternate_selection_panels);
	static Panel *findPanel(const std::vector<Panel> *panels, const QPoint &offset);
	static void sortPanels(std::vector<Panel> *panels);
public:
	enum class SelectionOperation {
		SetSelection,
		AddSelection,
		SubSelection,
	};
	void clearSelection();
	void addSelection(const Layer &source, const RenderOption &opt, bool *abort);
	void subSelection(const Layer &source, const RenderOption &opt, bool *abort);
	Panel renderSelection(const QRect &r, bool *abort) const;
	void changeSelection(SelectionOperation op, QRect const &rect);
	Panel crop(const QRect &r, bool *abort) const;
	void trim(const QRect &r);
	void clear();
	int addNewLayer();
	static LayerPtr newLayer();
	void setCurrentLayer(int index);
};

euclase::Image cropImage(euclase::Image const &srcimg, int sx, int sy, int sw, int sh);

#endif // CANVAS_H
