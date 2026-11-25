
#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "ApplicationGlobal.h"
#include "FilterDialog.h"
#include "FilterFormBlur.h"
#include "FilterFormColorCorrection.h"
#include "FilterFormMedian.h"
#include "FilterStatus.h"
#include "MySettings.h"
#include "NewDialog.h"
#include "ResizeDialog.h"
#include "RoundBrushGenerator.h"
#include "SettingsDialog.h"
#include "antialias.h"
#include "euclase.h"
#include "median.h"
#include "Canvas.h"
#include "MyToolButton.h"
#include "misc.h"
#include "xbrz/xbrz.h"
#include <QBitmap>
#include <QClipboard>
#include <QDebug>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QKeyEvent>
#include <QMimeData>
#include <QPainter>
#include <QRegion>
#include <QScreen>
#include <QShortcut>
#include <algorithm>
#include <omp.h>
#include <stdint.h>
#include <QMessageBox>
#include <variant>

struct MainWindow::Private {
	QColor primary_color;
	QColor secondary_color;
	Brush current_brush;

	double brush_next_distance = 0;
	double brush_span = 4;
	double brush_t = 0;
	QPointF brush_bezier[4];
	bool brush_failed = false;

	bool mouse_moved = false;
	QPoint start_viewport_pt;
	QPointF anchor_canvas_pt;
	QPointF offset_canvas_pt;
	QPointF topleft_canvas_pt;
	QPointF bottomright_canvas_pt;
	QPointF rect_topleft_canvas_pt;
	QPointF rect_bottomright_canvas_pt;

	MainWindow::RectHandle rect_handle = MainWindow::RectHandle::None;

	bool preview_layer_enabled = true;

	std::mutex canvas_mutex;
	
	std::unique_ptr<FilterDialog> filter_dialog;

	Document document;

	QVBoxLayout *main_menu_bar_vbox_layout;
	QHBoxLayout *property_bar_hbox_layout;

	tool::ToolVariant current_tool;
	tool::AbstractTool *current_tool2 = nullptr;

	tool::ScrollTool tool_scroll;
	tool::BrushTool tool_brush;
	tool::BoundsTool tool_bounds;
};


MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent)
	, ui(new Ui::MainWindow)
	, m(new Private)
{
	ui->setupUi(this);

	{
		createMainToolBar();
		clearMainToolBar();
		addMainToolBarButton("Scroll", m->tool_scroll);
		addMainToolBarButton("Brush", m->tool_brush);
		addMainToolBarButton("Bounds", m->tool_bounds);
	}
	{
		createPropertyBar();
		clearPropertyBar();
	}

	m->current_tool2 = &m->tool_scroll;


	ui->horizontalSlider_size->setValue(1);
	ui->horizontalSlider_softness->setValue(0);
	ui->widget_image_view->init(this, ui->verticalScrollBar, ui->horizontalScrollBar);
	ui->widget_image_view->setMouseTracking(true);

	ui->horizontalSlider_size->setVisualType(BrushSlider::SIZE);
	ui->horizontalSlider_softness->setVisualType(BrushSlider::SOFTNESS);

	ui->horizontalSlider_size->setMinimumHeight(ColorEditWidget::MIN_SLIDER_HEIGHT);
	ui->horizontalSlider_softness->setMinimumHeight(ColorEditWidget::MIN_SLIDER_HEIGHT);

	ui->widget_color_edit->bind(ui->widget_color);

	connect(ui->widget_color, &SaturationBrightnessWidget::changeColor, this, &MainWindow::setCurrentColor);

	connect(ui->widget_image_view, &ImageViewWidget::scaleChanged, [&](double scale){
		ui->widget_brush->changeScale(scale);
	});

	connect(ui->widget_image_view, &ImageViewWidget::updateDocInfo, this, &MainWindow::onUpdateDocumentInformation);

	setColor(Qt::black, Qt::white);

	{
		Brush b;
		b.size = 100;
		b.softness = 1.0;
		setCurrentBrush(b);
	}

	if (1) {
		Qt::WindowStates state = windowState();
		MySettings settings;

		settings.beginGroup("MainWindow");
		bool maximized = settings.value("Maximized").toBool();
		restoreGeometry(settings.value("Geometry").toByteArray());
		settings.endGroup();
		if (maximized) {
			state |= Qt::WindowMaximized;
			setWindowState(state);
		}
	}

	ui->widget_image_view->setFocus();

	m->tool_bounds.bounds_type = Bounds::Ellipse();

	qApp->installEventFilter(this);
}

MainWindow::~MainWindow()
{
	ui->widget_image_view->stopRenderingThread();
	delete m;
	delete ui;
}

Canvas *MainWindow::canvas()
{
	return &m->document.canvas_;
}

Canvas const *MainWindow::canvas() const
{
	return &m->document.canvas_;
}

std::mutex &MainWindow::mutexForCanvas() const
{
	return m->canvas_mutex;
}

QSize MainWindow::canvasSize() const
{
	std::lock_guard lock(mutexForCanvas());
	return canvas()->size();
}

int MainWindow::canvasWidth() const
{
	return canvasSize().width();
}

int MainWindow::canvasHeight() const
{
	return canvasSize().height();
}

void MainWindow::createMainToolBar()
{
	m->main_menu_bar_vbox_layout = new QVBoxLayout;
	ui->main_tool_bar_frame->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Maximum);
	ui->main_tool_bar_frame->setLayout(m->main_menu_bar_vbox_layout);
	m->main_menu_bar_vbox_layout->setContentsMargins(0, 0, 0, 0);
}

void MainWindow::createPropertyBar()
{
	m->property_bar_hbox_layout = new QHBoxLayout;
	ui->property_bar_frame->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
	ui->property_bar_frame->setLayout(m->property_bar_hbox_layout);
	m->property_bar_hbox_layout->setContentsMargins(0, 0, 0, 0);
}

void MainWindow::clearMainToolBar()
{
	int i = m->main_menu_bar_vbox_layout->count();
	while (i > 0) {
		i--;
		delete m->main_menu_bar_vbox_layout->takeAt(i);
	}
}

void MainWindow::clearPropertyBar()
{
	int i = m->property_bar_hbox_layout->count();
	while (i > 0) {
		i--;
		delete m->property_bar_hbox_layout->takeAt(i);
	}
}

MyToolButton *MainWindow::newMyToolButton(QString text, tool::ToolVariant tool)
{
	MyToolButton *b = new MyToolButton;
	b->setText(text);
	b->setFixedSize(40, 40);
	b->setCheckable(true);
	b->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
	b->setProperty("tool", QVariant::fromValue(MainTool(tool)));
	return b;
}

void MainWindow::addMainToolBarButton(QString const &text, tool::ToolVariant tool)
{
	MyToolButton *b = newMyToolButton(text, tool);
	connect(b, &MyToolButton::clicked, this, &MainWindow::onToolButton);
	m->main_menu_bar_vbox_layout->addWidget(b);
}

void MainWindow::addPropertyBarButton(QString const &text, tool::ToolVariant tool)
{
	MyToolButton *b = newMyToolButton(text, tool);
	connect(b, &MyToolButton::clicked, this, &MainWindow::onToolButton);
	m->property_bar_hbox_layout->addWidget(b);
}

void MainWindow::setColor(QColor primary_color, QColor secondary_color)
{
	m->primary_color = primary_color;
	if (secondary_color.isValid()) {
		m->secondary_color = secondary_color;
	}

	ui->widget_color_edit->setColor(m->primary_color);

	ui->widget_color_preview->setColor(m->primary_color, m->secondary_color);
}

void MainWindow::setCurrentColor(const QColor &color)
{
	setColor(color, QColor());
}

QColor MainWindow::foregroundColor() const
{
	return m->primary_color;
}

void MainWindow::setCurrentBrush(const Brush &brush)
{
	m->current_brush = brush;
	m->brush_span = std::max(brush.size / 8.0, 0.5);

	bool f1 = ui->horizontalSlider_size->blockSignals(true);
	bool f2 = ui->horizontalSlider_softness->blockSignals(true);
	bool f3 = ui->spinBox_brush_size->blockSignals(true);
	bool f4 = ui->horizontalSlider_softness->blockSignals(true);

	ui->widget_brush->setBrush_(brush);

	ui->horizontalSlider_size->setValue(brush.size);
	ui->spinBox_brush_size->setValue(brush.size);
	ui->horizontalSlider_softness->setValue(brush.softness * 100);
	ui->spinBox_brush_softness->setValue(brush.softness * 100);

	ui->horizontalSlider_softness->blockSignals(f4);
	ui->spinBox_brush_size->blockSignals(f3);
	ui->horizontalSlider_softness->blockSignals(f2);
	ui->horizontalSlider_size->blockSignals(f1);
}

Brush const &MainWindow::currentBrush() const
{
	return m->current_brush;
}

void MainWindow::hideBounds(bool update)
{
	ui->widget_image_view->hideRect(update);
}

bool MainWindow::isRectVisible() const
{
	return ui->widget_image_view->isRectVisible();
}

QRect MainWindow::boundsRect() const
{
	int x = m->rect_topleft_canvas_pt.x();
	int y = m->rect_topleft_canvas_pt.y();
	int w = m->rect_bottomright_canvas_pt.x() - x;
	int h = m->rect_bottomright_canvas_pt.y() - y;
	return QRect(x, y, w, h);
}

void MainWindow::resetView(bool fitview)
{
	hideBounds(false);

	if (fitview) {
		fitView();
	} else {
		updateImageViewEntire();
	}
	onSelectionChanged();
}

euclase::Image::MemoryType MainWindow::preferredMemoryType() const
{
	return global->cuda ? euclase::Image::CUDA : euclase::Image::Host;;
}

euclase::ImageFormat MainWindow::preferredImageFormat() const
{
	// return euclase::ImageFormat::Format_F32_RGBA;
	return euclase::ImageFormat::Format_F16_RGBA;
}

void MainWindow::setupBasicLayer(Canvas::Layer *layer)
{
	layer->clear();
	layer->format_ = preferredImageFormat();
	layer->memtype_ = preferredMemoryType();
}

Document const &MainWindow::currentDocument() const
{
	return m->document;
}

void MainWindow::setImage(euclase::Image image, bool fitview, QString path)
{
	try {
		Canvas::Layer replacement;
		setupBasicLayer(&replacement);
		image = image.convertToFormat(replacement.format_);
		image.memconvert(replacement.memtype_);
		if (!image) {
			QMessageBox::critical(this, tr("Error"), tr("Failed to create image"));
			return;
		}

		Canvas::Layer source;
		source.setImage(QPoint(0, 0), image);
		Canvas::RenderOption opt;
		opt.blend_mode = Canvas::BlendMode::Normal;
		if (!Canvas::renderToLayer(&replacement, Canvas::PrimaryLayer, source, nullptr, opt, nullptr)) {
			QMessageBox::critical(this, tr("Error"), tr("Failed to create image"));
			return;
		}

		std::lock_guard lock(mutexForCanvas());
		canvas()->clear();
		canvas()->setSize(image.size());
		*canvas()->current_layer() = std::move(replacement);
	} catch (const std::bad_alloc &) {
		QMessageBox::critical(this, tr("Error"), tr("Failed to create image"));
		return;
	}
	m->document.setDocumentPath(path);
	ui->widget_image_view->clearRenderCache();
	resetView(fitview);
	updateImageView({});
	onUpdateDocumentInformation();
}

void MainWindow::setImageFromBytes(QByteArray const &ba, bool fitview)
{
	QImage img;
	img.loadFromData(ba);
	setImage(euclase::Image(img), fitview);
}

void MainWindow::openFile(QString const &path)
{
	const QString suffix = QFileInfo(path).suffix().toUpper();
	std::optional<euclase::Image> result;
	if (suffix == "PNG") {
		result = euclase::load_png(path.toStdString().c_str());
	} else if (suffix == "JPEG" || suffix == "JPG") {
		result = euclase::load_jpeg(path.toStdString().c_str());
	} else {
		QMessageBox::critical(this, tr("Error"), tr("Unsupported file format: %1").arg(suffix));
		return;
	}
	if (!result || !*result) {
		QMessageBox::critical(this, tr("Error"), tr("Failed to open image: %1").arg(path));
		return;
	}
	setImage(*result, true, path);
}

void MainWindow::on_action_file_save_as_triggered()
{
	QString path = QFileDialog::getSaveFileName(this);
	if (!path.isEmpty()) {
		euclase::Image img;
		{
			std::lock_guard lock(mutexForCanvas());
			img = renderCanvasLocked(Canvas::AllLayers);
		}
		if (!img) {
			QMessageBox::critical(this, tr("Error"), tr("Failed to render image for saving"));
			return;
		}
		auto i = path.lastIndexOf('.');
		QString suffix;
		if (i > 0) {
			suffix = path.mid(i).toUpper();
		}
		// img.qimage().save(path);
		if (suffix == ".PNG") {
			if (!euclase::save_png(img, path.toStdString().c_str())) {
				QMessageBox::critical(this, tr("Error"), tr("Failed to save PNG file"));
			}
		} else if (suffix == ".JPEG" || suffix == ".JPG") {
			if (!euclase::save_jpeg(img, path.toStdString().c_str())) {
				QMessageBox::critical(this, tr("Error"), tr("Failed to save JPEG file"));
			}
		} else {
			QMessageBox::critical(this, tr("Error"), tr("Unsupported file format: %1").arg(suffix));
		}
	}
}


/**
 * @brief フィルタ中のプレビューの有効状態を設定
 * @param enabled
 */
void MainWindow::setPreviewLayerEnable(bool enabled)
{
	std::lock_guard lock(mutexForCanvas());
	m->preview_layer_enabled = enabled;
}

/**
 * @brief フィルタ中のプレビューの有効状態を取得
 * @return
 */
bool MainWindow::isPreviewEnabled() const
{
	std::lock_guard lock(mutexForCanvas());
	return m->preview_layer_enabled;
}

Canvas::Panel MainWindow::renderToPanel(Canvas::InputLayerMode input_layer_mode, euclase::ImageFormat format, QRect const &r, QRect const &maskrect, const Canvas::RenderOption &opt, const std::atomic_bool *abort) const
{
	if (abort && abort->load(std::memory_order_relaxed)) return {};
	std::lock_guard lock(mutexForCanvas());
	auto activepanel = m->preview_layer_enabled ? Canvas::AlternateLayer : Canvas::PrimaryLayer;
	return canvas()->renderToPanel(input_layer_mode, format, r, maskrect, activepanel, opt, abort);
}

euclase::Image MainWindow::renderSelection(const QRect &r, const std::atomic_bool *abort) const
{
	if (abort && abort->load(std::memory_order_relaxed)) return {};
	std::lock_guard lock(mutexForCanvas());
	return canvas()->renderSelection(r, abort).image();
}

euclase::Image MainWindow::renderToImage(euclase::ImageFormat format, QRect const &r, Canvas::RenderOption const &opt, const std::atomic_bool *abort) const
{
	return renderToPanel(Canvas::CurrentLayerOnly, format, r, {}, opt, abort).image();
}

QRect MainWindow::selectionRect() const
{
	std::lock_guard lock(mutexForCanvas());
	return canvas()->selection_layer()->rect();
}

void MainWindow::fitView()
{
	ui->widget_image_view->scaleFit(0.98);
}

void MainWindow::on_horizontalSlider_size_valueChanged(int value)
{
	ui->widget_brush->setBrushSize(value);
}

void MainWindow::on_spinBox_brush_size_valueChanged(int value)
{
	ui->widget_brush->setBrushSize(value);
}

void MainWindow::on_horizontalSlider_softness_valueChanged(int value)
{
	ui->widget_brush->setBrushSoftness(value / 100.0);
}

void MainWindow::on_spinBox_brush_softness_valueChanged(int value)
{
	ui->widget_brush->setBrushSoftness(value / 100.0);
}

void MainWindow::on_action_resize_triggered()
{
	euclase::Image srcimage = renderFilterTargetImage();
	auto memtype = srcimage.memtype();
	QSize sz = srcimage.size();

	ResizeDialog dlg(this);
	dlg.setImageSize(sz);
	if (dlg.exec() == QDialog::Accepted) {
		sz = dlg.imageSize();
		unsigned int w = sz.width();
		unsigned int h = sz.height();
		w = std::max(w, 1U);
		h = std::max(h, 1U);
		euclase::EnlargeMethod method = dlg.method();
		euclase::Image newimage = resizeImage(srcimage, w, h, method);
		newimage.memconvert(memtype);
		setImage(newimage, true, currentDocument().file_path_);
	}
}

void MainWindow::on_action_file_open_triggered()
{
	MySettings s;
	static const char *DefaultDirectory = "DefaultDirectory";
	s.beginGroup("Global");
	QString path = s.value(DefaultDirectory).toString();
	path = QFileDialog::getOpenFileName(this, tr("Open"), path);
	if (!path.isEmpty()) {
		QString dir = QFileInfo(path).absoluteDir().absolutePath();
		s.setValue(DefaultDirectory, dir);
		openFile(path);
	}
}

euclase::Image MainWindow::renderFilterTargetImage()
{
	std::lock_guard lock(mutexForCanvas());
	return renderCanvasLocked(Canvas::CurrentLayerOnly);
}

euclase::Image MainWindow::renderCanvasLocked(Canvas::InputLayerMode input_layer_mode) const
{
	const QRect area(QPoint(), canvas()->size());
	const auto active_panel = m->preview_layer_enabled ? Canvas::AlternateLayer : Canvas::PrimaryLayer;
	return canvas()->renderToPanel(input_layer_mode, euclase::Format_F32_RGBA, area, {}, active_panel, {}, nullptr).image();
}

Canvas::BlendMode MainWindow::blendMode() const
{
	Canvas::BlendMode blendmode = Canvas::BlendMode::Normal;

	auto var = currentTool();
	if (tool::BrushTool *t = std::get_if<tool::BrushTool>(&var)) {
		if (t->isEraser()) {
			blendmode = Canvas::BlendMode::Eraser;
		}
	}

	return blendmode;
}

Canvas::RenderOption2 MainWindow::renderOption() const
{
	Canvas::RenderOption2 opt;

	opt.opt1.blend_mode = blendMode();

	opt.opt1.use_mask = true;
	opt.selection_layer = canvas()->selection_layer();
	if (opt.selection_layer->panels()->empty()) {
		opt.selection_layer = nullptr;
		opt.opt1.use_mask = false;
	}

	return opt;
}

void MainWindow::onPenDown(double x, double y)
{
	Canvas::BlendMode blendmode = blendMode();

	resetCurrentAlternateOption(blendmode);
	m->brush_bezier[0] = m->brush_bezier[1] = m->brush_bezier[2] = m->brush_bezier[3] = QPointF(x, y);
	m->brush_next_distance = 0;
	m->brush_t = 0;
	m->brush_failed = false;
	drawBrush(true);
}

void MainWindow::onPenStroke(double x, double y)
{
	m->brush_bezier[0] = m->brush_bezier[3];
	m->brush_bezier[3] = QPointF(x, y);
	x = (m->brush_bezier[0].x() * 2 + m->brush_bezier[3].x()) / 3;
	y = (m->brush_bezier[0].y() * 2 + m->brush_bezier[3].y()) / 3;
	m->brush_bezier[1] = QPointF(x, y);
	x = (m->brush_bezier[0].x() + m->brush_bezier[3].x() * 2) / 3;
	y = (m->brush_bezier[0].y() + m->brush_bezier[3].y() * 2) / 3;
	m->brush_bezier[2] = QPointF(x, y);

	drawBrush(false);
}

void MainWindow::onPenUp(double x, double y)
{
	(void)x;
	(void)y;
	m->brush_next_distance = 0;
	applyCurrentAlternateLayer();
}

bool MainWindow::isFilterDialogActive() const
{
	return (bool)m->filter_dialog;
}

void MainWindow::setFilerDialogActive(bool active)
{
	bool enable = !active;
	ui->menuBar->setEnabled(enable);
	ui->mainToolBar->setEnabled(enable);
	ui->frame_tool_frame->setEnabled(enable);
	ui->frame_property->setEnabled(enable);
	ui->tabWidget_color->setEnabled(enable);
	ui->widget_color_edit->setEnabled(enable);
	ui->tabWidget_brush->setEnabled(enable);
}

void MainWindow::filterStart(FilterContext &&context, AbstractFilterForm *form, const FilterFunction &fn)
{
	std::unique_ptr<AbstractFilterForm> filter_form(form);
	if (isFilterDialogActive()) return;

	const QRect bounds = isRectVisible() ? boundsRect() : QRect();
	const auto image = [&]() -> euclase::Image {
		std::lock_guard lock(mutexForCanvas());
		auto *current = canvas()->current_layer();
		const auto *selection = canvas()->selection_layer();
		Canvas::Layer mask;
		if (bounds.isValid()) {
			euclase::Image mask_image;
			if (selection->primary_panels.empty()) {
				mask_image.make(bounds.width(), bounds.height(), euclase::Format_U8_Grayscale,
								euclase::Image::Host, euclase::k::white);
			} else {
				mask_image = canvas()->renderSelection(bounds, nullptr).image();
			}
			if (!mask_image) return {};
			Canvas::Layer source;
			source.setImage(bounds.topLeft(), mask_image);
			mask.setOffset(current->offset());
			if (!Canvas::renderToLayer(&mask, Canvas::AlternateSelection, source, nullptr, {}, nullptr)) {
				return {};
			}
		} else if (selection->offset() == current->offset()) {
			mask.alternate_selection_panels = selection->primary_panels;
		} else if (!selection->primary_panels.empty()) {
			mask.setOffset(current->offset());
			if (!Canvas::renderToLayer(&mask, Canvas::AlternateSelection, *selection, nullptr, {}, nullptr)) {
				return {};
			}
		}

		auto source = renderCanvasLocked(Canvas::CurrentLayerOnly);
		if (source) {
			current->alternate_selection_panels = std::move(mask.alternate_selection_panels);
			current->alternate_blend_mode = Canvas::BlendMode::Replace;
		}
		return source;
	}();
	if (!image) return;

	context.setSourceImage(image);
	m->filter_dialog = std::make_unique<FilterDialog>(this, std::move(context), filter_form.get(), fn);
	filter_form.release();
	m->filter_dialog->connect(m->filter_dialog.get(), &FilterDialog::end, this, &MainWindow::filterClose);
	m->filter_dialog->show();
	setFilerDialogActive(true);
}

void MainWindow::filterClose(bool apply)
{
	std::unique_ptr<FilterDialog> p;
	p.swap(m->filter_dialog);
	if (p) {
		setFilerDialogActive(false);
		euclase::Image result;
		if (apply) {
			result = p->result();
		}
		p.reset();
		if (!result || !setFilteredImage(result, true)) {
			resetCurrentAlternateOption({});
			if (result) {
				QMessageBox::critical(this, tr("Error"), tr("Failed to apply filter"));
			}
		}
		setPreviewLayerEnable(true);
		updateImageViewEntire();
	}
}

euclase::Image sepia(euclase::Image const &image, FilterStatus *status)
{
	if (image.memtype() != euclase::Image::Host) {
		return sepia(image.toHost(), status);
	}

	auto isInterrupted = [&](){
		return status && status->cancel && *status->cancel;
	};
	auto progress = [&](float v){
		if (status && status->progress) {
			*status->progress = v;
		}
	};
	int w = image.width();
	int h = image.height();
	euclase::Image newimage;
	newimage.make(w, h, image.format());
	if (w > 0 || h > 0) {
		std::atomic_int rows = 0;
#pragma omp parallel for schedule(static, 8)
		for (int y = 0; y < h; y++) {
			if (isInterrupted()) continue;
			euclase::Float32RGBA const *s = (euclase::Float32RGBA const *)image.scanLine(y);
			euclase::Float32RGBA *d = (euclase::Float32RGBA *)newimage.scanLine(y);
			for (int x = 0; x < w; x++) {
				double r = s[x].r;
				double g = s[x].g;
				double b = s[x].b;
				double a = s[x].a;
				r = pow(r, 0.62) * 0.80392156 + 0.07450980;
				g = pow(g, 1.00) * 0.71372549 + 0.06666666;
				b = pow(b, 1.16) * 0.61176470 + 0.08235294;
				d[x].r = r;
				d[x].g = g;
				d[x].b = b;
				d[x].a = a;
			}
			progress((float)++rows / h);
		}
	}
	return newimage;
}

void MainWindow::on_action_filter_sepia_triggered()
{
	FilterContext fc;
	fc.setParameter("amount", 10);
	filterStart(std::move(fc), nullptr, [](const FilterContext *context, FilterStatus *status){
		return sepia(context->sourceImage(), status);
	});
}

void MainWindow::on_action_filter_median_triggered()
{
	FilterContext fc;
	fc.setParameter("amount", 10);
	filterStart(std::move(fc), new FilterFormMedian(this), [](const FilterContext *context, FilterStatus *status){
		int value = context->parameter("amount").toInt();
		return filter_median(context->sourceImage(), value, status);
	});
}

void MainWindow::on_action_filter_maximize_triggered()
{
	FilterContext fc;
	fc.setParameter("amount", 10);
	filterStart(std::move(fc), nullptr, [](const FilterContext *context, FilterStatus *status){
		int value = context->parameter("amount").toInt();
		return filter_maximize(context->sourceImage(), value, status);
	});
}

void MainWindow::on_action_filter_minimize_triggered()
{
	FilterContext fc;
	fc.setParameter("amount", 10);
	filterStart(std::move(fc), nullptr, [](const FilterContext *context, FilterStatus *status){
		int value = context->parameter("amount").toInt();
		return filter_minimize(context->sourceImage(), value, status);
	});
}

void MainWindow::on_action_filter_blur_triggered()
{
	auto fn = [](const FilterContext *context, FilterStatus *status){
		int radius = context->parameter("amount").toInt();
		euclase::Image newimage = context->sourceImage().toHost();
		auto isInterrupted = [&]() { return status->cancel->load(); };
		for (int pass = 0; pass < 3; pass++) {
			auto progress = [&](float v){
				*status->progress = (pass + v) / 3.0f;
			};
			newimage = filter_blur(newimage, radius, isInterrupted, progress);
		}
		return newimage;
	};

	FilterContext fc;
	fc.setParameter("amount", 10);
	filterStart(std::move(fc), new FilterFormBlur(this), fn);
}

void MainWindow::on_action_filter_antialias_triggered()
{
	FilterContext fc;
	fc.setParameter("amount", 10);
	filterStart(std::move(fc), nullptr, [](const FilterContext *context, FilterStatus *){
		euclase::Image newimage = context->sourceImage();
		filter_antialias(&newimage);
		return newimage;
	});
}


void MainWindow::on_horizontalScrollBar_valueChanged(int value)
{
	(void)value;
	ui->widget_image_view->refrectScrollBar();
}

void MainWindow::on_verticalScrollBar_valueChanged(int value)
{
	(void)value;
	ui->widget_image_view->refrectScrollBar();
}

euclase::Image MainWindow::selectedImage() const
{
	const QRect bounds = isRectVisible() ? boundsRect() : QRect();
	std::lock_guard lock(mutexForCanvas());
	QRect area = canvas()->selection_layer()->rect();
	if (area.isEmpty()) area = bounds;
	if (area.isEmpty()) area = QRect(QPoint(), canvas()->size());
	return canvas()->crop(area, nullptr).image();
}

void MainWindow::on_action_trim_triggered()
{
	if (!isRectVisible()) return;
	const QRect area = boundsRect();
	{
		std::lock_guard lock(mutexForCanvas());
		if (!canvas()->trim(area)) return;
	}
	resetView(true);
	updateImageViewEntire();
}

void MainWindow::updateSelectionOutline()
{
	ui->widget_image_view->requestUpdateSelectionOutline();
}

void MainWindow::onSelectionChanged()
{
	updateSelectionOutline();
}

QPointF MainWindow::mapToCanvasFromViewport(QPointF const &pt) const
{
	return ui->widget_image_view->mapToCanvasFromViewport(pt);
}

QPointF MainWindow::mapToViewportFromCanvas(QPointF const &pt) const
{
	return ui->widget_image_view->mapToViewportFromCanvas(pt);
}

/**
 * @brief MainWindow::updateImageViewEntire
 *
 * ビューの更新を要求
 */
void MainWindow::updateImageViewEntire()
{
	ui->widget_image_view->requestRendering({});
}

/**
 * @brief ビューの更新を要求
 * @param canvasrect 更新する領域（キャンバス座標系）
 */
void MainWindow::updateImageView(const QRect &canvasrect)
{
	ui->widget_image_view->requestRendering(canvasrect);
}

bool MainWindow::paintLayer(Operation op, Canvas::Layer const &layer, QRegion &dirty)
{
	if (isFilterDialogActive()) return false;
	
	std::vector<QRect> changed;
	changed.reserve(layer.primary_panels.size());
	Canvas::RenderOption opt;
	opt.notify_changed_rect = [&](const QRect &area){
		changed.push_back(area);
	};
	opt.brush_color = foregroundColor();
	bool drawn;
	{
		std::lock_guard lock(mutexForCanvas());
		drawn = op == Operation::PaintToCurrentLayer ? canvas()->paintToCurrentLayer(layer, opt, nullptr)
					: canvas()->paintToCurrentAlternate(layer, opt, nullptr);
	}
	for (const auto &area : changed) dirty += area;
	return drawn;
}

/**
 * @brief MainWindow::drawBrush
 * @param one
 *
 * ブラシを描画する
 * oneがtrueの場合は、一回だけ描画する
 * falseの場合は、前回の座標から現在の座標までの間を補完して描画する
 */
void MainWindow::drawBrush(bool one)
{
	if (isFilterDialogActive() || m->brush_failed) return;

	QRegion dirty;
	auto Put = [&](QPointF const &pt, Brush const &brush){
		Canvas::Layer layer;
		{
			double x = pt.x();
			double y = pt.y();
			if (brush.softness == 0) {
				x = floor(x) + 0.5;
				y = floor(y) + 0.5;
			}
			int x0 = floor(x - brush.size / 2.0);
			int y0 = floor(y - brush.size / 2.0);
			int x1 = ceil(x + brush.size / 2.0);
			int y1 = ceil(y + brush.size / 2.0);
			int w = x1 - x0;
			int h = y1 - y0;
			RoundBrushGenerator shape(brush.size, brush.softness);
			layer.setImage(QPoint(x0, y0), shape.image(w, h, x - x0, y - y0, m->primary_color));
		}
		m->brush_failed = !paintLayer(Operation::PaintToCurrentAlternate, layer, dirty);
	};

	auto Point = [&](double t){
		return euclase::cubicBezierPoint(m->brush_bezier[0], m->brush_bezier[1], m->brush_bezier[2], m->brush_bezier[3], t);
	};

	QPointF pt0 = Point(m->brush_t);
	if (one) {
		Put(pt0, currentBrush());
		m->brush_next_distance = m->brush_span;
	} else {
		do {
			if (m->brush_next_distance == 0) {
				Put(pt0, currentBrush());
				if (m->brush_failed) break;
				m->brush_next_distance = m->brush_span;
			}
			double t = std::min(m->brush_t + (1.0 / 16), 1.0);
			QPointF pt1 = Point(t);
			double dx = pt0.x() - pt1.x();
			double dy = pt0.y() - pt1.y();
			double d = hypot(dx, dy);
			if (m->brush_next_distance > d) {
				m->brush_next_distance -= d;
				m->brush_t = t;
				pt0 = pt1;
			} else {
				m->brush_t += (t - m->brush_t) * m->brush_next_distance / d;
				m->brush_t = std::min(m->brush_t, 1.0);
				m->brush_next_distance = 0;
				pt0 = Point(m->brush_t);
			}
		} while (m->brush_t < 1.0);
	}

	m->brush_t = 0;
	if (!dirty.isEmpty()) ui->widget_image_view->requestRendering(dirty);
}

void MainWindow::resetCurrentAlternateOption(Canvas::BlendMode blendmode)
{
	std::lock_guard lock(mutexForCanvas());
	canvas()->current_layer()->finishAlternatePanels(false, nullptr, {});
	canvas()->current_layer()->setAlternateOption(blendmode);
}

void MainWindow::applyCurrentAlternateLayer()
{
	{
		std::lock_guard lock(mutexForCanvas());
		const auto opt = renderOption();
		auto *layer = canvas()->current_layer();
		if (!m->brush_failed && layer->finishAlternatePanels(true, opt.selection_layer, opt.opt1)) return;
		layer->finishAlternatePanels(false, nullptr, {});
	}
	updateImageViewEntire();
	QMessageBox::critical(this, tr("Error"), tr("Failed to apply brush stroke"));
}

QPointF MainWindow::pointOnCanvas(int x, int y) const
{
	QPointF pos(x + 0.5, y + 0.5);
	return ui->widget_image_view->mapToCanvasFromViewport(pos);
}

bool MainWindow::setFilteredImage(const euclase::Image &image, bool apply)
{
	if (!image) return false;

	try {
		Canvas::Layer source;
		source.setImage({}, image);
		std::lock_guard lock(mutexForCanvas());
		auto *current = canvas()->current_layer();
		Canvas::Layer replacement;
		replacement.setOffset(current->offset());
		replacement.format_ = image.format();
		replacement.memtype_ = current->memtype_;
		if (!Canvas::renderToLayer(&replacement, Canvas::AlternateLayer, source, nullptr, {}, nullptr)) return false;

		auto mode = current->alternate_blend_mode;
		current->alternate_panels.swap(replacement.alternate_panels);
		current->alternate_blend_mode = Canvas::BlendMode::Replace;
		if (apply) {
			const auto opt = renderOption();
			if (!current->finishAlternatePanels(true, opt.selection_layer, opt.opt1)) {
				current->alternate_panels.swap(replacement.alternate_panels);
				current->alternate_blend_mode = mode;
				return false;
			}
		}
	} catch (const std::bad_alloc &) {
		return false;
	}
	updateImageViewEntire();
	return true;
}

MainWindow::RectHandle MainWindow::rectHitTest(QPoint const &pt) const
{
	if (isRectVisible()) {

		const int D = 100;

		QPointF topleft = mapToViewportFromCanvas(m->topleft_canvas_pt);
		QPointF bottomright = mapToViewportFromCanvas(m->bottomright_canvas_pt);
		if (topleft.x() > bottomright.x()) std::swap(topleft.rx(), bottomright.rx());
		if (topleft.y() > bottomright.y()) std::swap(topleft.ry(), bottomright.ry());
		int x0 = topleft.x();
		int y0 = topleft.y();
		int x1 = bottomright.x() + 1;
		int y1 = bottomright.y() + 1;

		int d, dx, dy;

		dx = pt.x() - (x0 + x1) / 2;
		dy = pt.y() - (y0 + y1) / 2;
		d = dx * dx + dy * dy;
		if (d < D) {
			return RectHandle::Center;
		}

		dx = pt.x() - x1;
		dy = pt.y() - y1;
		d = dx * dx + dy * dy;
		if (d < D) {
			return RectHandle::BottomRight;
		}

		dx = pt.x() - x0;
		dy = pt.y() - y1;
		d = dx * dx + dy * dy;
		if (d < D) {
			return RectHandle::BottomLeft;
		}

		dx = pt.x() - x1;
		dy = pt.y() - y0;
		d = dx * dx + dy * dy;
		if (d < D) {
			return RectHandle::TopRight;
		}

		dx = pt.x() - x0;
		dy = pt.y() - y0;
		d = dx * dx + dy * dy;
		if (d < D) {
			return RectHandle::TopLeft;
		}

		dx = pt.x() - (x0 + x1) / 2;
		dy = pt.y() - y0;
		d = dx * dx + dy * dy;
		if (d < D) {
			return RectHandle::Top;
		}

		dx = pt.x() - x0;
		dy = pt.y() - (y0 + y1) / 2;
		d = dx * dx + dy * dy;
		if (d < D) {
			return RectHandle::Left;
		}

		dx = pt.x() - x1;
		dy = pt.y() - (y0 + y1) / 2;
		d = dx * dx + dy * dy;
		if (d < D) {
			return RectHandle::Right;
		}

		dx = pt.x() - (x0 + x1) / 2;
		dy = pt.y() - y1;
		d = dx * dx + dy * dy;
		if (d < D) {
			return RectHandle::Bottom;
		}
	}

	return RectHandle::None;
}

namespace tool {

class MouseButtonPress {
public:
	int x;
	int y;
	MouseButtonPress() = default;
	MouseButtonPress(int x, int y)
		: x(x)
		, y(y)
	{}
};

class MouseMove {
public:
	int x;
	int y;
	bool left_button;
	bool set_cursor_only;
	MouseMove() = default;
	MouseMove(int x, int y, bool left_button, bool set_cursor_only)
		: x(x)
		, y(y)
		, left_button(left_button)
		, set_cursor_only(set_cursor_only)
	{}
};

class MouseButtonRelease {
public:
	int x;
	int y;
	bool left_button;
	bool mouse_moved;
	MouseButtonRelease() = default;
	MouseButtonRelease(int x, int y, bool left_button, bool mouse_moved)
		: x(x)
		, y(y)
		, left_button(left_button)
		, mouse_moved(mouse_moved)
	{}
};

bool BoundsTool::on(MainWindow *mw, const MouseButtonPress &a)
{
	mw->onBoundsStart();
	return true;
}

bool BoundsTool::on(MainWindow *mw, const MouseMove &a)
{
	mw->onBoundsMove(a);
	return true;
}

bool BoundsTool::on(MainWindow *mw, const MouseButtonRelease &a)
{
	mw->onBoundsEnd(a);
	return true;
}

bool BrushTool::on(MainWindow *mw, const MouseButtonPress &a)
{
	QPointF pos = mw->pointOnCanvas(a.x, a.y);
	mw->onPenDown(pos.x(), pos.y());
	return true;
}

bool BrushTool::on(MainWindow *mw, const MouseMove &a)
{
	mw->setToolCursor(Qt::ArrowCursor);
	if (!a.set_cursor_only) {
		if (a.left_button) {
			QPointF pos = mw->pointOnCanvas(a.x, a.y);
			mw->onPenStroke(pos.x(), pos.y());
		}
	}
	return true;
}

bool BrushTool::on(MainWindow *mw, const MouseButtonRelease &a)
{
	if (a.left_button) {
		QPointF pos = mw->pointOnCanvas(a.x, a.y);
		mw->onPenUp(pos.x(), pos.y());
		return true;
	}
	return false;
}

bool ScrollTool::on(MainWindow *mw, const MouseButtonPress &a)
{
	return false;
}

bool ScrollTool::on(MainWindow *mw, const MouseMove &a)
{
	mw->setToolCursor(Qt::OpenHandCursor);
	if (!a.set_cursor_only) {
		mw->doHandScroll();
	}
	return true;
}

bool ScrollTool::on(MainWindow *mw, const MouseButtonRelease &a)
{
	return false;
}

struct ToolUtil {

	static AbstractTool *toolptr(tool::ScrollTool &t) { return &t; }
	static AbstractTool *toolptr(tool::BrushTool &t) { return &t; }
	static AbstractTool *toolptr(tool::BoundsTool &t) { return &t; }

	static void setupPropertyBar(MainWindow *mw, tool::ScrollTool const &)
	{
	}

	static void setupPropertyBar(MainWindow *mw, tool::BrushTool const &)
	{
		mw->addPropertyBarButton(MainWindow::tr("Brush"), tool::BrushTool());
		mw->addPropertyBarButton(MainWindow::tr("Eraser"), tool::BrushTool::Eraser());
	}

	static void setupPropertyBar(MainWindow *mw, tool::BoundsTool const &t)
	{
		mw->addPropertyBarButton(MainWindow::tr("Rectangle"), tool::BoundsTool::Rectangle());
		mw->addPropertyBarButton(MainWindow::tr("Ellipse"), tool::BoundsTool::Ellipse());
	}

};

void ScrollTool::setupPropertyBar(MainWindow *mw) { ToolUtil::setupPropertyBar(mw, *this); }
void BrushTool::setupPropertyBar(MainWindow *mw)  { ToolUtil::setupPropertyBar(mw, *this); }
void BoundsTool::setupPropertyBar(MainWindow *mw) { ToolUtil::setupPropertyBar(mw, *this); }

} // namespace tool

tool::AbstractTool *MainWindow::abstractTool(tool::ToolVariant tool)
{
	return std::visit([](auto &x){ return tool::ToolUtil::toolptr(x); }, tool);
}

void MainWindow::changeTool_internal(MainTool tool)
{
	m->current_tool = tool.var;
	m->current_tool2 = abstractTool(tool.var);

	clearPropertyBar();

	std::visit([this](auto const &x){
		tool::ToolUtil::setupPropertyBar(this, x);
	}, m->current_tool);
}

void MainWindow::changeTool(tool::ToolVariant tool)
{
	changeTool_internal(MainTool(tool));
}

void MainWindow::onToolButton(MyToolButton *button)
{
	MainTool t = button->property("tool").value<MainTool>();
	changeTool_internal(t);
}

tool::ToolVariant MainWindow::currentTool() const
{
	if (isFilterDialogActive()) {
		return m->tool_scroll;
	}

	return m->current_tool;
}


void MainWindow::setBounds_internal()
{
	QPointF topleft = m->topleft_canvas_pt + m->offset_canvas_pt;
	QPointF bottomright = m->bottomright_canvas_pt + m->offset_canvas_pt;
	double x0 = floor(topleft.x());
	double y0 = floor(topleft.y());
	double x1 = floor(bottomright.x());
	double y1 = floor(bottomright.y());
	if (x0 > x1) std::swap(x0, x1);
	if (y0 > y1) std::swap(y0, y1);
	m->rect_topleft_canvas_pt = { x0, y0 };
	m->rect_bottomright_canvas_pt = { x1 + 1, y1 + 1 };
	ui->widget_image_view->showBounds(m->rect_topleft_canvas_pt, m->rect_bottomright_canvas_pt);
}

void MainWindow::onBoundsStart()
{
	m->rect_handle = rectHitTest(m->start_viewport_pt);
	if (m->rect_handle != RectHandle::None) {
		m->topleft_canvas_pt += QPointF(0.01, 0.01);
		m->bottomright_canvas_pt += QPointF(-0.01, -0.01);
		double x0 = m->topleft_canvas_pt.x();
		double y0 = m->topleft_canvas_pt.y();
		double x1 = m->bottomright_canvas_pt.x();
		double y1 = m->bottomright_canvas_pt.y();
		if (m->rect_handle == RectHandle::Center) {
			m->anchor_canvas_pt = QPointF((x0 + x1) / 2, (y0 + y1) / 2);
		} else if (m->rect_handle == RectHandle::TopLeft) {
			m->anchor_canvas_pt = m->topleft_canvas_pt;
		} else if (m->rect_handle == RectHandle::TopRight) {
			m->anchor_canvas_pt = QPointF(x1, y0);
		} else if (m->rect_handle == RectHandle::BottomLeft) {
			m->anchor_canvas_pt = QPointF(x0, y1);
		} else if (m->rect_handle == RectHandle::BottomRight) {
			m->anchor_canvas_pt = m->bottomright_canvas_pt;
		} else if (m->rect_handle == RectHandle::Top) {
			m->anchor_canvas_pt = QPointF((x0 + x1) / 2, y0);
		} else if (m->rect_handle == RectHandle::Left) {
			m->anchor_canvas_pt = QPointF(x0, (y0 + y1) / 2);
		} else if (m->rect_handle == RectHandle::Right) {
			m->anchor_canvas_pt = QPointF(x1, (y0 + y1) / 2);
		} else if (m->rect_handle == RectHandle::Bottom) {
			m->anchor_canvas_pt = QPointF((x0 + x1) / 2, y1);
		}
	}
	if (m->rect_handle == RectHandle::None) {
		m->rect_handle = RectHandle::BottomRight;
		m->anchor_canvas_pt = mapToCanvasFromViewport(m->start_viewport_pt);
		m->topleft_canvas_pt = m->bottomright_canvas_pt = m->anchor_canvas_pt;
		m->offset_canvas_pt = { 0, 0 };
		setBounds_internal();
	}
}

void MainWindow::onBoundsMove(tool::MouseMove const &a)
{
	if (!a.set_cursor_only && a.left_button) {
		m->mouse_moved = true;
	} else {
		m->rect_handle = rectHitTest(QPoint(a.x, a.y));
	}
	if (m->rect_handle == RectHandle::None) {
		setToolCursor(Qt::ArrowCursor);
	} else {
		setToolCursor(Qt::SizeAllCursor);
		if (!a.set_cursor_only && a.left_button) {
			QPointF pt = mapToCanvasFromViewport(mapToViewportFromCanvas(m->anchor_canvas_pt) + QPointF(a.x, a.y) - m->start_viewport_pt);
			if (m->rect_handle == RectHandle::Center) {
				m->offset_canvas_pt = { pt.x() - m->anchor_canvas_pt.x(), pt.y() - m->anchor_canvas_pt.y() };
			} else if (m->rect_handle == RectHandle::TopLeft) {
				m->topleft_canvas_pt = pt;
			} else if (m->rect_handle == RectHandle::BottomRight) {
				m->bottomright_canvas_pt = pt;
			} else if (m->rect_handle == RectHandle::TopRight) {
				m->topleft_canvas_pt.ry() = pt.y();
				m->bottomright_canvas_pt.rx() = pt.x();
			} else if (m->rect_handle == RectHandle::BottomLeft) {
				m->topleft_canvas_pt.rx() = pt.x();
				m->bottomright_canvas_pt.ry() = pt.y();
			} else if (m->rect_handle == RectHandle::Top) {
				m->topleft_canvas_pt.ry() = pt.y();
			} else if (m->rect_handle == RectHandle::Left) {
				m->topleft_canvas_pt.rx() = pt.x();
			} else if (m->rect_handle == RectHandle::Right) {
				m->bottomright_canvas_pt.rx() = pt.x();
			} else if (m->rect_handle == RectHandle::Bottom) {
				m->bottomright_canvas_pt.ry() = pt.y();
			}
			setBounds_internal();
		}
	}
}

void MainWindow::onBoundsEnd(tool::MouseButtonRelease const &a)
{
	if (a.left_button) {
		m->topleft_canvas_pt = m->rect_topleft_canvas_pt;
		m->bottomright_canvas_pt = m->rect_bottomright_canvas_pt;
		if (!a.mouse_moved) {
			hideBounds(true);
		}
	}
}

void MainWindow::doHandScroll()
{
	ui->widget_image_view->doHandScroll();
}



Bounds::Type MainWindow::boundsType() const
{
	return m->tool_bounds.bounds_type;
}

bool MainWindow::onMouseLeftButtonPress(int x, int y)
{
	tool::MouseButtonPress args(x, y);

	m->mouse_moved = false;
	m->start_viewport_pt = QPoint(x, y);
	m->offset_canvas_pt = { 0, 0 };

	auto v = currentTool();
	return std::visit([&](auto &tool){ return tool.on(this, args); }, v);
}

bool MainWindow::mouseMove_internal(int x, int y, bool left_button, bool set_cursor_only)
{
	tool::MouseMove args(x, y, left_button, set_cursor_only);

	auto v = currentTool();
	return std::visit([&](auto &tool){ return tool.on(this, args); }, v);
}

bool MainWindow::onMouseMove(int x, int y, bool left_button)
{
	return mouseMove_internal(x, y, left_button, false);
}

bool MainWindow::onMouseLeftButtonRelease(int x, int y, bool left_button)
{
	tool::MouseButtonRelease args(x, y, left_button, m->mouse_moved);
	m->mouse_moved = false;

	auto v = currentTool();
	return std::visit([&](auto &tool){ return tool.on(this, args); }, v);
}

void MainWindow::setToolCursor(QCursor const &cursor)
{
	ui->widget_image_view->setToolCursor(cursor);
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
	if (QApplication::modalWindow()) return;

	if (event->mimeData()->hasUrls()) {
		event->setDropAction(Qt::CopyAction);
		event->accept();
		return;
	}
	QMainWindow::dragEnterEvent(event);
}

void MainWindow::dropEvent(QDropEvent *event)
{
	if (QApplication::modalWindow()) return;

	if (0) {
		QMimeData const *mimedata = event->mimeData();
		QByteArray encoded = mimedata->data("application/x-qabstractitemmodeldatalist");
		QDataStream stream(&encoded, QIODevice::ReadOnly);
		while (!stream.atEnd()) {
			int row, col;
			QMap<int,  QVariant> roledatamap;
			stream >> row >> col >> roledatamap;
		}
	}

	if (event->mimeData()->hasUrls()) {
		QStringList paths;
		QByteArray ba = event->mimeData()->data("text/uri-list");
		if (ba.size() > 4 && memcmp(ba.data(), "h\0t\0t\0p\0", 8) == 0) {
			QString path = QString::fromUtf16((ushort const *)ba.data(), ba.size() / 2);
			int i = path.indexOf('\n');
			if (i >= 0) {
				path = path.mid(0, i);
			}
			if (!path.isEmpty()) {
				paths.push_back(path);
			}
		} else {
			QList<QUrl> urls = event->mimeData()->urls();
			for (QUrl const &url : urls) {
				QString path = url.url();
				paths.push_back(path);
			}
		}
		for (QString const &path : paths) {
			if (path.startsWith("file://")) {
				int i = 7;
#ifdef Q_OS_WIN
				if (path.utf16()[i] == '/') {
					i++;
				}
#endif
				openFile(path.mid(i));
			} else if (path.startsWith("http://") || path.startsWith("https://")) {
			}
		}
	}
}

void MainWindow::closeEvent(QCloseEvent *)
{
	MySettings settings;

	if (1) {
		setWindowOpacity(0);
		Qt::WindowStates state = windowState();
		bool maximized = (state & Qt::WindowMaximized) != 0;
		if (maximized) {
			state &= ~Qt::WindowMaximized;
			setWindowState(state);
		}
		{
			settings.beginGroup("MainWindow");
			settings.setValue("Maximized", maximized);
			settings.setValue("Geometry", saveGeometry());
			settings.endGroup();
		}
	}
}

void MainWindow::updateToolCursor()
{
	mouseMove_internal(0, 0, false, true);
	ui->widget_image_view->updateToolCursor();
}

void MainWindow::on_action_clear_bounds_triggered()
{
	hideBounds(true);
}

void MainWindow::setColorRed(int value)
{
	QColor c = foregroundColor();
	c = QColor(value, c.green(), c.blue());
	setCurrentColor(c);
}

void MainWindow::setColorGreen(int value)
{
	QColor c = foregroundColor();
	c = QColor(c.red(), value, c.blue());
	setCurrentColor(c);
}

void MainWindow::setColorBlue(int value)
{
	QColor c = foregroundColor();
	c = QColor(c.red(), c.green(), value);
	setCurrentColor(c);
}

void MainWindow::setColorHue(int value)
{
	QColor c = foregroundColor();
	c = QColor::fromHsv(value, c.saturation(), c.value());
	setCurrentColor(c);
}

void MainWindow::setColorSaturation(int value)
{
	QColor c = foregroundColor();
	c = QColor::fromHsv(c.hue(), value, c.value());
	setCurrentColor(c);
}

void MainWindow::setColorValue(int value)
{
	QColor c = foregroundColor();
	c = QColor::fromHsv(c.hue(), c.saturation(), value);
	setCurrentColor(c);
}

void MainWindow::on_horizontalSlider_rgb_r_valueChanged(int value)
{
	setColorRed(value);
}

void MainWindow::on_horizontalSlider_rgb_g_valueChanged(int value)
{
	setColorGreen(value);
}

void MainWindow::on_horizontalSlider_rgb_b_valueChanged(int value)
{
	setColorBlue(value);
}

void MainWindow::on_spinBox_rgb_r_valueChanged(int value)
{
	setColorRed(value);
}

void MainWindow::on_spinBox_rgb_g_valueChanged(int value)
{
	setColorGreen(value);
}

void MainWindow::on_spinBox_rgb_b_valueChanged(int value)
{
	setColorBlue(value);
}

void MainWindow::on_horizontalSlider_hsv_h_valueChanged(int value)
{
	setColorHue(value);
}

void MainWindow::on_horizontalSlider_hsv_s_valueChanged(int value)
{
	setColorSaturation(value);
}

void MainWindow::on_horizontalSlider_hsv_v_valueChanged(int value)
{
	setColorValue(value);
}

void MainWindow::on_spinBox_hsv_h_valueChanged(int value)
{
	setColorHue(value);
}

void MainWindow::on_spinBox_hsv_s_valueChanged(int value)
{
	setColorSaturation(value);
}

void MainWindow::on_spinBox_hsv_v_valueChanged(int value)
{
	setColorValue(value);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
	if (event->type() == QEvent::KeyPress) {
		if (QWidget *w = qobject_cast<QWidget *>(watched)) {
			if (isAncestorOf(w)) {
				QKeyEvent *e = (QKeyEvent *)event;
				bool ctrl = (e->modifiers() & Qt::ControlModifier);
				int k = e->key();
				switch (k) {
				case Qt::Key_B:
					changeTool(tool::BrushTool());
					return true;
				case Qt::Key_H:
					changeTool(tool::ScrollTool());
					return true;
				case Qt::Key_P:
					if (ctrl) {
						QList<QScreen *> list = QApplication::screens();
						QRect rect;
						std::vector<QRect> bounds;
						for (int i = 0; i < list.size(); i++) {
							QRect r = list[i]->geometry();
							if (i == 0) {
								rect = r;
							} else {
								rect = rect.united(r);
							}
							bounds.push_back(r);
						}
						if (!bounds.empty()) {
							QImage im;
							{
								im = QImage(rect.width(), rect.height(), QImage::Format_RGBA8888);
								im.fill(Qt::transparent);

								QPainter pr(&im);
								for (int i = 0; i < (int)bounds.size(); i++) {
									QPixmap pm = list[i]->grabWindow(0);
									QRect r = bounds[i];
									pr.drawPixmap(r, pm, pm.rect());
								}
							}
							setImage(euclase::Image(im), true);
						}
					}
					return true;
				case Qt::Key_R:
					changeTool(tool::BoundsTool());
					return true;
				case Qt::Key_T:
					if (ctrl) {
						test();
					}
					return true;
				case Qt::Key_U:
					colorCollection();
					return true;
				case Qt::Key_X:
					setColor(m->secondary_color, m->primary_color);
					return true;
				case Qt::Key_Plus:
					ui->widget_image_view->zoomIn();
					return true;
				case Qt::Key_Minus:
					ui->widget_image_view->zoomOut();
					return true;
				case Qt::Key_Escape:
					on_action_clear_bounds_triggered();
					return true;
				}
			}
		}
	}
	return false;
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
	QMainWindow::keyPressEvent(event);
}

void MainWindow::on_toolButton_scroll_clicked()
{
	changeTool(tool::ScrollTool());
}

void MainWindow::on_toolButton_paint_brush_clicked()
{
	changeTool(tool::BrushTool());
}

void MainWindow::on_toolButton_eraser_brush_clicked()
{
	changeTool(tool::BrushTool::Eraser());
}

void MainWindow::on_toolButton_rect_clicked()
{
	changeTool(tool::BoundsTool());
}

void MainWindow::on_action_edit_copy_triggered()
{	
	const QImage image = selectedImage().qimage();
	if (!image.isNull()) QApplication::clipboard()->setImage(image);
}

void MainWindow::on_action_new_triggered()
{
	NewDialog dlg(this);
	if (dlg.exec() == QDialog::Accepted) {
		QSize sz = dlg.imageSize();
		if (dlg.from() == NewDialog::From::New) {
			euclase::Image image;
			image.make(sz.width(), sz.height(), euclase::ImageFormat::Format_F32_RGBA);
			image.fill(euclase::k::white);
			setImage(image, true);
			return;
		}
		if (dlg.from() == NewDialog::From::Clipboard) {
			QImage qimg = qApp->clipboard()->image();
			if (qimg.width() > 0 && qimg.height() > 0) {
				qimg = qimg.convertToFormat(QImage::Format_RGBA8888);
				euclase::Image image(qimg);
				setImage(image, true);
			}
			return;
		}
	}
}

void MainWindow::on_action_select_rectangle_triggered()
{
	if (isRectVisible()) {
		QRect r = boundsRect();
		if (r.width() > 0 && r.height() > 0) {
			Canvas::SelectionOperation op = Canvas::SelectionOperation::AddSelection;
			const auto type = boundsType();
			{
				std::lock_guard lock(mutexForCanvas());
				if (!canvas()->changeSelection(op, r, type)) return;
			}
			onSelectionChanged();
			updateImageViewEntire();
		}
	}
}

void MainWindow::on_action_settings_triggered()
{
	SettingsDialog dlg(this);
	if (dlg.exec() == QDialog::Accepted) {

	}
}

void MainWindow::filter_xBRZ(int factor)
{
	euclase::Image image = renderFilterTargetImage();
	QImage srcimage = image.qimage();
	srcimage = srcimage.convertToFormat(QImage::Format_RGBA8888);
	int w = srcimage.width();
	int h = srcimage.height();
	if (w > 0 && h > 0) {
		QImage dstimage(w * factor, h * factor, QImage::Format_RGBA8888);
		xbrz::ScalerCfg cfg;
		xbrz::scale(factor, (uint32_t*)srcimage.bits(), (uint32_t*)dstimage.bits(), w, h, xbrz::ColorFormat::RGBA, cfg, 0, h);
		setImage(euclase::Image(dstimage), true, currentDocument().file_path_);
	}
}

void MainWindow::on_action_filter_2xBRZ_triggered()
{
	filter_xBRZ(2);
}

void MainWindow::on_action_filter_4xBRZ_triggered()
{
	filter_xBRZ(4);
}

int MainWindow::addNewLayer()
{
	std::lock_guard lock(mutexForCanvas());
	int index = canvas()->addNewLayer();
	Canvas::Layer *p = canvas()->layer(index);
	if (p) setupBasicLayer(p);
	return index;
}

bool MainWindow::setCurrentLayer(int index)
{
	std::lock_guard lock(mutexForCanvas());
	return canvas()->setCurrentLayer(index);
}

struct ColorCorrectionParams {
	float hue = 0;
	float saturation = 0;
	float brightness = 0;
};

euclase::Image filter_color_correction(euclase::Image const &image, ColorCorrectionParams const &params, FilterStatus *status)
{
	euclase::Image srcimage;
	if (image.memtype() == euclase::Image::Host) {
		srcimage = image;
	} else {
		srcimage = image.toHost();
	}

	auto isInterrupted = [&](){
		return status && status->cancel && *status->cancel;
	};
	auto progress = [&](float v){
		if (status && status->progress) {
			*status->progress = v;
		}
	};
	int w = srcimage.width();
	int h = srcimage.height();
	euclase::Image newimage;
	newimage.make(w, h, srcimage.format());
	if (w > 0 || h > 0) {
		float s_lo = -1;
		float s_hi = -1;
		if (params.saturation < 0) {
			s_lo = 1.0f + params.saturation;
		} else if (params.saturation > 0) {
			s_hi = 1.0f - params.saturation;
		}
		float v_lo = -1;
		float v_hi = -1;
		if (params.brightness < 0) {
			v_lo = 1.0f + params.brightness;
			v_lo *= v_lo;
		} else if (params.brightness > 0) {
			v_hi = 1.0f - params.brightness;
			v_hi = sqrtf(v_hi);
		}
		std::atomic_int rows = 0;
#pragma omp parallel for schedule(static, 8)
		for (int y = 0; y < h; y++) {
			if (isInterrupted()) continue;
			euclase::Float32RGBA const *s = (euclase::Float32RGBA const *)srcimage.scanLine(y);
			euclase::Float32RGBA *d = (euclase::Float32RGBA *)newimage.scanLine(y);
			for (int x = 0; x < w; x++) {
				euclase::Float32RGB rgb(s[x].r, s[x].g, s[x].b);
				double A = s[x].a;
				if (params.hue != 0) {
					auto hsv = euclase::rgb_to_hsv(rgb);
					hsv.h = hsv.h + params.hue;
					rgb = euclase::hsv_to_rgb(hsv);
				}
				if (params.saturation != 0) {
					float gray = euclase::grayf(rgb.r, rgb.g, rgb.b);
					float s = params.saturation + 1.0f;
					if (s > 0) {
						s *= s;
					}
					rgb.r = gray + (rgb.r - gray) * s;
					rgb.g = gray + (rgb.g - gray) * s;
					rgb.b = gray + (rgb.b - gray) * s;
				}
				if (v_lo >= 0) {
					rgb.r *= v_lo;
					rgb.g *= v_lo;
					rgb.b *= v_lo;
				} else if (v_hi >= 0) {
					rgb.r = 1.0f - (1.0f - rgb.r) * v_hi;
					rgb.g = 1.0f - (1.0f - rgb.g) * v_hi;
					rgb.b = 1.0f - (1.0f - rgb.b) * v_hi;
				}
				d[x] = euclase::Float32RGBA(rgb.r, rgb.g, rgb.b, A);
			}
			progress((float)++rows / h);
		}
	}
	return newimage;
}

void MainWindow::colorCollection()
{
	FilterContext fc;
	fc.setParameter("hue", 0);
	fc.setParameter("saturation", 0);
	fc.setParameter("brightness", 0);
	filterStart(std::move(fc), new FilterFormColorCorrection(this), [](const FilterContext *context, FilterStatus *status){
		ColorCorrectionParams params;
		params.hue = context->parameter("hue").toInt() / 360.f;
		params.saturation = context->parameter("saturation").toInt() / 100.f;
		params.brightness = context->parameter("brightness").toInt() / 100.f;
		return filter_color_correction(context->sourceImage(), params, status);
	});
}

void MainWindow::onUpdateDocumentInformation()
{
	Document const &doc = currentDocument();
	QString str("%1 x %2");
	QSize sz = canvasSize();
	str = str.arg(sz.width()).arg(sz.height());
	ui->statusBar->showMessage(str);

	const QString name = doc.fileName();
	setWindowTitle(name.isEmpty() ? qApp->applicationName() : name + " - " + qApp->applicationName());
}


void MainWindow::test()
{
}





