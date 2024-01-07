
#include "ApplicationGlobal.h"
#include "Canvas.h"
#include "FilterDialog.h"
#include "FilterFormBlur.h"
#include "FilterFormColorCorrection.h"
#include "FilterFormMedian.h"
#include "FilterStatus.h"
#include "MainWindow.h"
#include "MySettings.h"
#include "NewDialog.h"
#include "ResizeDialog.h"
#include "RoundBrushGenerator.h"
#include "SettingsDialog.h"
#include "antialias.h"
#include "median.h"
#include "ui_MainWindow.h"
#include "xbrz/xbrz.h"
#include <QBitmap>
#include <QClipboard>
#include <QDebug>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QKeyEvent>
#include <QMimeData>
#include <QPainter>
#include <QScreen>
#include <QShortcut>
#include <omp.h>
#include <stdint.h>
#include "euclase.h"


struct MainWindow::Private {
	Canvas doc;
	QColor primary_color;
	QColor secondary_color;
	Brush current_brush;

	double brush_next_distance = 0;
	double brush_span = 4;
	double brush_t = 0;
	QPointF brush_bezier[4];

	MainWindow::Tool current_tool;

	bool mouse_moved = false;
	QPoint start_vpt;
	QPointF anchor_dpt;
	QPointF offset_dpt;
	QPointF topleft_dpt;
	QPointF bottomright_dpt;
	QPointF rect_topleft_dpt;
	QPointF rect_bottomright_dpt;

	MainWindow::RectHandle rect_handle = MainWindow::RectHandle::None;

	bool preview_layer_enabled = true;

	std::mutex canvas_mutex;
};

MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent)
	, ui(new Ui::MainWindow)
	, m(new Private)
{
	ui->setupUi(this);
	ui->horizontalSlider_size->setValue(1);
	ui->horizontalSlider_softness->setValue(0);
	ui->widget_image_view->init(this, ui->verticalScrollBar, ui->horizontalScrollBar);
	ui->widget_image_view->setMouseTracking(true);

	ui->toolButton_scroll->setCheckable(true);
	ui->toolButton_paint_brush->setCheckable(true);
	ui->toolButton_eraser_brush->setCheckable(true);
	ui->toolButton_rect->setCheckable(true);
	ui->toolButton_scroll->click();

	ui->horizontalSlider_size->setVisualType(BrushSlider::SIZE);
	ui->horizontalSlider_softness->setVisualType(BrushSlider::SOFTNESS);

	ui->horizontalSlider_size->setMinimumHeight(ColorEditWidget::MIN_SLIDER_HEIGHT);
	ui->horizontalSlider_softness->setMinimumHeight(ColorEditWidget::MIN_SLIDER_HEIGHT);

	ui->widget_color_edit->bind(ui->widget_color);

	connect(ui->widget_color, &SaturationBrightnessWidget::changeColor, this, &MainWindow::setCurrentColor);

	connect(ui->widget_image_view, &ImageViewWidget::scaleChanged, [&](double scale){
		ui->widget_brush->changeScale(scale);
	});

	setColor(Qt::black, Qt::white);

	{
		Brush b;
		b.size = 85;
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

	qApp->installEventFilter(this);
}

MainWindow::~MainWindow()
{
	clearCanvas();
	delete m;
	delete ui;
}

Canvas *MainWindow::canvas()
{
	return &m->doc;
}

Canvas const *MainWindow::canvas() const
{
	return &m->doc;
}

std::mutex &MainWindow::mutexForCanvas() const
{
	return m->canvas_mutex;
}

int MainWindow::canvasWidth() const
{
	auto const *d = canvas();
	return d ? d->width() : 0;
}

int MainWindow::canvasHeight() const
{
	auto const *d = canvas();
	return d ? d->height() : 0;
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

void MainWindow::hideRect(bool update)
{
	ui->widget_image_view->hideRect(update);
}

bool MainWindow::isRectVisible() const
{
	return ui->widget_image_view->isRectVisible();
}

QRect MainWindow::boundsRect() const
{
	int x = m->rect_topleft_dpt.x();
	int y = m->rect_topleft_dpt.y();
	int w = m->rect_bottomright_dpt.x() - x;
	int h = m->rect_bottomright_dpt.y() - y;
	return QRect(x, y, w, h);
}

void MainWindow::resetView(bool fitview)
{
	hideRect(false);

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

void MainWindow::setupBasicLayer(Canvas::Layer *p)
{
	p->clear();
	p->format_ = euclase::Image::Format_F_RGBA;
	p->memtype_ = preferredMemoryType();
}

void MainWindow::setImage(euclase::Image image, bool fitview)
{
	clearCanvas();
	ui->widget_image_view->clearRenderCache(true, true);


	int w = image.width();
	int h = image.height();
	canvas()->setSize(QSize(w, h));
	setupBasicLayer(canvas()->current_layer());

	Canvas::Layer layer;
	{
		image = image.memconvert(canvas()->current_layer()->memtype_);
		image = image.convertToFormat(canvas()->current_layer()->format_);
		layer.setImage(QPoint(0, 0), image);
	}
	Canvas::RenderOption opt;
	opt.blend_mode = Canvas::BlendMode::Normal;
	canvas()->renderToLayer(canvas()->current_layer(), Canvas::Canvas::PrimaryLayer, layer, nullptr, opt, nullptr);

	resetView(fitview);
	updateImageView({});
}

void MainWindow::setImageFromBytes(QByteArray const &ba, bool fitview)
{
	QImage img;
	img.loadFromData(ba);
	setImage(euclase::Image(img), fitview);
}

void MainWindow::openFile(QString const &path)
{
	QByteArray ba;
	QFile file(path);
	if (file.open(QFile::ReadOnly)) {
		ba = file.readAll();
	}
	setImageFromBytes(ba, true);
}

void MainWindow::setFilteredImage(euclase::Image const &image)
{
	std::lock_guard lock(mutexForCanvas());

	canvas()->current_layer()->alternate_panels.clear();

	Canvas::Layer layer;
	layer.setImage(QPoint(0, 0), image);

	Canvas::RenderOption opt;
	opt.blend_mode = Canvas::BlendMode::Normal;
	canvas()->renderToLayer(canvas()->current_layer(), Canvas::AlternateLayer, layer, nullptr, opt, nullptr);

	canvas()->current_layer()->alternate_blend_mode = Canvas::BlendMode::Replace;

	updateImageViewEntire();
}

/**
 * @brief フィルタ中のプレビューの有効状態を設定
 * @param enabled
 */
void MainWindow::setPreviewLayerEnable(bool enabled)
{
	m->preview_layer_enabled = enabled;
}

/**
 * @brief フィルタ中のプレビューの有効状態を取得
 * @return
 */
bool MainWindow::isPreviewEnabled() const
{
	return m->preview_layer_enabled;
}

Canvas::Panel MainWindow::renderToPanel(Canvas::InputLayerMode input_layer_mode, euclase::Image::Format format, QRect const &r, QRect const &maskrect, const Canvas::RenderOption &opt, bool *abort) const
{
	std::lock_guard lock(mutexForCanvas());
	auto activepanel = isPreviewEnabled() ? Canvas::AlternateLayer : Canvas::PrimaryLayer;
	return canvas()->renderToPanel(input_layer_mode, format, r, maskrect, activepanel, opt, abort).image();
}

euclase::Image MainWindow::renderSelection(const QRect &r, bool *abort) const
{
	std::lock_guard lock(mutexForCanvas());
	return canvas()->renderSelection(r, abort).image();
}

euclase::Image MainWindow::renderToImage(euclase::Image::Format format, QRect const &r, Canvas::RenderOption const &opt, bool *abort) const
{
	return renderToPanel(Canvas::CurrentLayerOnly, format, r, {}, opt, abort).image();
}

SelectionOutline MainWindow::renderSelectionOutline(bool *abort)
{
	return ui->widget_image_view->renderSelectionOutline(abort);
}

QRect MainWindow::selectionRect() const
{
	return const_cast<MainWindow *>(this)->canvas()->selection_layer()->rect();
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
		setImage(newimage, true);
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

void MainWindow::on_action_file_save_as_triggered()
{
	QString path = QFileDialog::getSaveFileName(this);
	if (!path.isEmpty()) {
		QSize sz = canvas()->size();
		auto activepanel = isPreviewEnabled() ? Canvas::AlternateLayer : Canvas::PrimaryLayer;
		euclase::Image img = canvas()->renderToPanel(Canvas::AllLayers, euclase::Image::Format_F_RGBA, QRect(0, 0, sz.width(), sz.height()), {}, activepanel, {}, nullptr).image();
		img.qimage().save(path);
	}
}

euclase::Image MainWindow::renderFilterTargetImage()
{
	QSize sz = canvas()->size();
	return renderToImage(euclase::Image::Format_F_RGBA, QRect(0, 0, sz.width(), sz.height()), {}, nullptr);
}

void MainWindow::filter(FilterContext *context, AbstractFilterForm *form, std::function<euclase::Image (FilterContext *context)> const &fn)
{
	canvas()->current_layer()->alternate_selection_panels.clear();
	if (isRectVisible()) {
		QRect r = boundsRect();
		euclase::Image img;
		if (canvas()->selection_layer()->primary_panels.empty()) {
			img = euclase::Image(r.width(), r.height(), euclase::Image::Format_8_Grayscale, canvas()->selection_layer()->memtype_);
			img.fill(euclase::k::white);
		} else {
			Canvas::Panel panel = canvas()->renderSelection(r, nullptr);
			img = panel.image();
		}
		// フィルタ用選択領域を作成
		Canvas::Layer layer;
		layer.setImage({r.x(), r.y()}, img);
		Canvas::RenderOption o;
		o.brush_color = Qt::white;
		Canvas::renderToLayer(canvas()->current_layer(), Canvas::Canvas::AlternateSelection, layer, nullptr, o, nullptr);
	} else if (!canvas()->selection_layer()->primary_panels.empty()) {
		canvas()->current_layer()->alternate_selection_panels = canvas()->selection_layer()->primary_panels;
	}

	canvas()->current_layer()->alternate_blend_mode = Canvas::BlendMode::Replace;

	euclase::Image image = renderFilterTargetImage();
	context->setSourceImage(image);

	Canvas::RenderOption opt;
	Canvas::Layer *selection = canvas()->selection_layer();
	opt.use_mask = true;
	if (selection->panels()->empty()) {
		selection = nullptr;
		opt.use_mask = false;
	}

	FilterDialog dlg(this, context, form, fn);
	if (dlg.exec() == QDialog::Accepted) {
		canvas()->current_layer()->finishAlternatePanels(true, selection, opt);
	} else {
		canvas()->current_layer()->finishAlternatePanels(false, selection, opt);
	}
	updateImageViewEntire();
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
			euclase::FloatRGBA const *s = (euclase::FloatRGBA const *)image.scanLine(y);
			euclase::FloatRGBA *d = (euclase::FloatRGBA *)newimage.scanLine(y);
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
	filter(&fc, nullptr, [](FilterContext *context){
		FilterStatus s(context->cancel_ptr(), context->progress_ptr());
		return sepia(context->sourceImage(), &s);
	});
}

void MainWindow::on_action_filter_median_triggered()
{
	FilterContext fc;
	fc.setParameter("amount", 10);
	filter(&fc, new FilterFormMedian(this), [](FilterContext *context){
		FilterStatus s(context->cancel_ptr(), context->progress_ptr());
		int value = context->parameter("amount").toInt();
		return filter_median(context->sourceImage(), value, &s);
	});
}

void MainWindow::on_action_filter_maximize_triggered()
{
	FilterContext fc;
	fc.setParameter("amount", 10);
	filter(&fc, nullptr, [](FilterContext *context){
		FilterStatus s(context->cancel_ptr(), context->progress_ptr());
		int value = context->parameter("amount").toInt();
		return filter_maximize(context->sourceImage(), value, &s);
	});
}

void MainWindow::on_action_filter_minimize_triggered()
{
	FilterContext fc;
	fc.setParameter("amount", 10);
	filter(&fc, nullptr, [](FilterContext *context){
		FilterStatus s(context->cancel_ptr(), context->progress_ptr());
		int value = context->parameter("amount").toInt();
		return filter_minimize(context->sourceImage(), value, &s);
	});
}

void MainWindow::on_action_filter_blur_triggered()
{
	auto fn = [](FilterContext *context){
		int radius = context->parameter("amount").toInt();
		euclase::Image newimage = context->sourceImage().toHost();
		FilterStatus s(context->cancel_ptr(), context->progress_ptr());
		for (int pass = 0; pass < 3; pass++) {
			auto progress = [&](float v){
				*s.progress = (pass + v) / 3.0f;
			};
			newimage = filter_blur(newimage, radius, s.cancel, progress);
		}
		return newimage;
	};

	FilterContext fc;
	fc.setParameter("amount", 10);
	filter(&fc, new FilterFormBlur(this), fn);
}

void MainWindow::on_action_filter_antialias_triggered()
{
	FilterContext fc;
	fc.setParameter("amount", 10);
	filter(&fc, nullptr, [](FilterContext *context){
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
	QRect r = selectionRect();
	if (r.isEmpty()) {
		if (isRectVisible()) {
			r = boundsRect();
		}
		if (r.isEmpty()) {
			r = { 0, 0, canvas()->width(), canvas()->height() };
		}
	}
	return canvas()->crop(r, nullptr).image();
}

void MainWindow::on_action_trim_triggered()
{
	if (isRectVisible()) {
		QRect r = boundsRect();
		if (!r.isEmpty()) {
			r = boundsRect();
			canvas()->trim(r);
			resetView(true);
		}
	}
}

void MainWindow::updateSelectionOutline()
{
	ui->widget_image_view->requestUpdateSelectionOutline();
}

void MainWindow::onSelectionChanged()
{
	updateSelectionOutline();
}

void MainWindow::clearCanvas()
{
	canvas()->clear();
}

/**
 * @brief MainWindow::updateImageViewEntire
 *
 * ビューの更新を要求
 */
void MainWindow::updateImageViewEntire()
{
	// ui->widget_image_view->clearRenderCache(false, true);
	// ui->widget_image_view->requestUpdateSelectionOutline();
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

void MainWindow::paintLayer(Operation op, Canvas::Layer const &layer)
{
	Canvas::RenderOption opt;
	opt.notify_changed_rect = [&](QRect const &canvasrect){
		updateImageView(canvasrect);
	};
	opt.brush_color = foregroundColor();
	{
		std::lock_guard lock(mutexForCanvas());
		////ui->widget_image_view->cancelRendering(); // これを呼ぶと、描画要求が溜まりすぎて、描画が追いつかなくなる
		if (op == Operation::PaintToCurrentLayer) {
			canvas()->paintToCurrentLayer(layer, opt, nullptr);
		} else if (op == Operation::PaintToCurrentAlternate) {
			canvas()->paintToCurrentAlternate(layer, opt, nullptr);
		}
	}
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
		paintLayer(Operation::PaintToCurrentAlternate, layer);
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
}

void MainWindow::resetCurrentAlternateOption(Canvas::BlendMode blendmode)
{
	std::lock_guard lock(mutexForCanvas());
	canvas()->current_layer()->finishAlternatePanels(false, nullptr, {});
	canvas()->current_layer()->setAlternateOption(blendmode);
}

void MainWindow::applyCurrentAlternateLayer()
{
	Canvas::BlendMode blendmode = Canvas::BlendMode::Normal;
	switch (currentTool()) {
	case MainWindow::Tool::EraserBrush:
		blendmode = Canvas::BlendMode::Eraser;
		break;
	}

	std::lock_guard lock(mutexForCanvas());

	Canvas::RenderOption opt;
	opt.blend_mode = blendmode;
	Canvas::Layer *selection = canvas()->selection_layer();
	opt.use_mask = true;
	if (selection->panels()->empty()) {
		opt.use_mask = false;
		selection = nullptr;
	}

	canvas()->current_layer()->finishAlternatePanels(true, selection, opt);
}

void MainWindow::onPenDown(double x, double y)
{
	Canvas::BlendMode blendmode = Canvas::BlendMode::Normal;
	switch (currentTool()) {
	case MainWindow::Tool::EraserBrush:
		blendmode = Canvas::BlendMode::Eraser;
		break;
	}

	resetCurrentAlternateOption(blendmode);
	m->brush_bezier[0] = m->brush_bezier[1] = m->brush_bezier[2] = m->brush_bezier[3] = QPointF(x, y);
	m->brush_next_distance = 0;
	m->brush_t = 0;
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

QPointF MainWindow::pointOnCanvas(int x, int y) const
{
	QPointF pos(x + 0.5, y + 0.5);
	return ui->widget_image_view->mapToCanvasFromViewport(pos);
}

QPointF MainWindow::mapToCanvasFromViewport(QPointF const &pt) const
{
	return ui->widget_image_view->mapToCanvasFromViewport(pt);
}

QPointF MainWindow::mapToViewportFromCanvas(QPointF const &pt) const
{
	return ui->widget_image_view->mapToViewportFromCanvas(pt);
}

MainWindow::RectHandle MainWindow::rectHitTest(QPoint const &pt) const
{
	if (isRectVisible()) {

		const int D = 100;

		QPointF topleft = mapToViewportFromCanvas(m->topleft_dpt);
		QPointF bottomright = mapToViewportFromCanvas(m->bottomright_dpt);
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

void MainWindow::setRect()
{
	QPointF topleft = m->topleft_dpt + m->offset_dpt;
	QPointF bottomright = m->bottomright_dpt + m->offset_dpt;
	double x0 = floor(topleft.x());
	double y0 = floor(topleft.y());
	double x1 = floor(bottomright.x());
	double y1 = floor(bottomright.y());
	if (x0 > x1) std::swap(x0, x1);
	if (y0 > y1) std::swap(y0, y1);
	m->rect_topleft_dpt = { x0, y0 };
	m->rect_bottomright_dpt = { x1 + 1, y1 + 1 };
	ui->widget_image_view->showRect(m->rect_topleft_dpt, m->rect_bottomright_dpt);
}

bool MainWindow::onMouseLeftButtonPress(int x, int y)
{
	m->mouse_moved = false;
	m->start_vpt = QPoint(x, y);
	m->offset_dpt = { 0, 0 };

	Tool tool = currentTool();
	if (tool == Tool::Scroll) return false;

	if (tool == Tool::Brush || tool == Tool::EraserBrush) {
		QPointF pos = pointOnCanvas(x, y);
		onPenDown(pos.x(), pos.y());
		return true;
	}

	if (tool == Tool::Rect) {
		m->rect_handle = rectHitTest(m->start_vpt);
		if (m->rect_handle != RectHandle::None) {
			m->topleft_dpt += QPointF(0.01, 0.01);
			m->bottomright_dpt += QPointF(-0.01, -0.01);
			double x0 = m->topleft_dpt.x();
			double y0 = m->topleft_dpt.y();
			double x1 = m->bottomright_dpt.x();
			double y1 = m->bottomright_dpt.y();
			if (m->rect_handle == RectHandle::Center) {
				m->anchor_dpt = QPointF((x0 + x1) / 2, (y0 + y1) / 2);
			} else if (m->rect_handle == RectHandle::TopLeft) {
				m->anchor_dpt = m->topleft_dpt;
			} else if (m->rect_handle == RectHandle::TopRight) {
				m->anchor_dpt = QPointF(x1, y0);
			} else if (m->rect_handle == RectHandle::BottomLeft) {
				m->anchor_dpt = QPointF(x0, y1);
			} else if (m->rect_handle == RectHandle::BottomRight) {
				m->anchor_dpt = m->bottomright_dpt;
			} else if (m->rect_handle == RectHandle::Top) {
				m->anchor_dpt = QPointF((x0 + x1) / 2, y0);
			} else if (m->rect_handle == RectHandle::Left) {
				m->anchor_dpt = QPointF(x0, (y0 + y1) / 2);
			} else if (m->rect_handle == RectHandle::Right) {
				m->anchor_dpt = QPointF(x1, (y0 + y1) / 2);
			} else if (m->rect_handle == RectHandle::Bottom) {
				m->anchor_dpt = QPointF((x0 + x1) / 2, y1);
			}
		}
		if (m->rect_handle == RectHandle::None) {
			m->rect_handle = RectHandle::BottomRight;
			m->anchor_dpt = mapToCanvasFromViewport(m->start_vpt);
			m->topleft_dpt = m->bottomright_dpt = m->anchor_dpt;
			m->offset_dpt = { 0, 0 };
			setRect();
		}
		return true;
	}

	return false;
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

bool MainWindow::mouseMove_internal(int x, int y, bool leftbutton, bool set_cursor_only)
{
	Tool tool = currentTool();
	if (tool == Tool::Scroll) {
		setToolCursor(Qt::OpenHandCursor);
		if (!set_cursor_only) {
			ui->widget_image_view->doHandScroll();
		}
		return true;
	}

	if (tool == Tool::Brush || tool == Tool::EraserBrush) {
		setToolCursor(Qt::ArrowCursor);
		if (!set_cursor_only) {
			if (leftbutton) {
				QPointF pos = pointOnCanvas(x, y);
				onPenStroke(pos.x(), pos.y());
			}
		}
		return true;
	}

	if (tool == Tool::Rect) {
		if (!set_cursor_only && leftbutton) {
			m->mouse_moved = true;
		} else {
			m->rect_handle = rectHitTest(QPoint(x, y));
		}
		if (m->rect_handle == RectHandle::None) {
			setToolCursor(Qt::ArrowCursor);
		} else {
			setToolCursor(Qt::SizeAllCursor);
			if (!set_cursor_only && leftbutton) {
				QPointF pt = mapToCanvasFromViewport(mapToViewportFromCanvas(m->anchor_dpt) + QPointF(x, y) - m->start_vpt);
				if (m->rect_handle == RectHandle::Center) {
					m->offset_dpt = { pt.x() - m->anchor_dpt.x(), pt.y() - m->anchor_dpt.y() };
				} else if (m->rect_handle == RectHandle::TopLeft) {
					m->topleft_dpt = pt;
				} else if (m->rect_handle == RectHandle::BottomRight) {
					m->bottomright_dpt = pt;
				} else if (m->rect_handle == RectHandle::TopRight) {
					m->topleft_dpt.ry() = pt.y();
					m->bottomright_dpt.rx() = pt.x();
				} else if (m->rect_handle == RectHandle::BottomLeft) {
					m->topleft_dpt.rx() = pt.x();
					m->bottomright_dpt.ry() = pt.y();
				} else if (m->rect_handle == RectHandle::Top) {
					m->topleft_dpt.ry() = pt.y();
				} else if (m->rect_handle == RectHandle::Left) {
					m->topleft_dpt.rx() = pt.x();
				} else if (m->rect_handle == RectHandle::Right) {
					m->bottomright_dpt.rx() = pt.x();
				} else if (m->rect_handle == RectHandle::Bottom) {
					m->bottomright_dpt.ry() = pt.y();
				}
				setRect();
			}
		}
		return true;
	}

	return false;
}

void MainWindow::updateToolCursor()
{
	mouseMove_internal(0, 0, false, true);
	ui->widget_image_view->updateToolCursor();
}

bool MainWindow::onMouseMove(int x, int y, bool leftbutton)
{
	return mouseMove_internal(x, y, leftbutton, false);
}

void MainWindow::on_action_clear_bounds_triggered()
{
	hideRect(true);
}

bool MainWindow::onMouseLeftButtonRelase(int x, int y, bool leftbutton)
{
	bool mouse_moved = m->mouse_moved;
	m->mouse_moved = false;

	Tool tool = currentTool();
	if (tool == Tool::Scroll) return false;

	if (tool == Tool::Brush || tool == Tool::EraserBrush) {
		if (leftbutton) {
			QPointF pos = pointOnCanvas(x, y);
			onPenUp(pos.x(), pos.y());
			return true;
		}
	}

	if (tool == Tool::Rect) {

		if (leftbutton) {
			m->topleft_dpt = m->rect_topleft_dpt;
			m->bottomright_dpt = m->rect_bottomright_dpt;
			if (!mouse_moved) {
				hideRect(true);
			}
		}
		return true;
	}

	return false;
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
					changeTool(Tool::Brush);
					return true;
				case Qt::Key_H:
					changeTool(Tool::Scroll);
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
					changeTool(Tool::Rect);
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

void MainWindow::changeTool(Tool tool)
{
	m->current_tool = tool;

	struct Button {
		Tool tool;
		QToolButton *button;
	};

	Button buttons[] = {
		{Tool::Scroll, ui->toolButton_scroll},
		{Tool::Brush, ui->toolButton_paint_brush},
		{Tool::EraserBrush, ui->toolButton_eraser_brush},
		{Tool::Rect, ui->toolButton_rect},
	};

	int n = sizeof(buttons) / sizeof(*buttons);
	for (int i = 0; i < n; i++) {
		bool f = (buttons[i].tool == m->current_tool);
		buttons[i].button->setChecked(f);
	}

	updateToolCursor();
}

MainWindow::Tool MainWindow::currentTool() const
{
	return m->current_tool;
}

void MainWindow::on_toolButton_scroll_clicked()
{
	changeTool(Tool::Scroll);
}

void MainWindow::on_toolButton_paint_brush_clicked()
{
	changeTool(Tool::Brush);
}

void MainWindow::on_toolButton_eraser_brush_clicked()
{
	changeTool(Tool::EraserBrush);
}

void MainWindow::on_toolButton_rect_clicked()
{
	changeTool(Tool::Rect);
}

void MainWindow::on_action_edit_copy_triggered()
{	
	euclase::Image image = selectedImage();
	QApplication::clipboard()->setImage(image.qimage());
}

void MainWindow::on_action_new_triggered()
{
	NewDialog dlg(this);
	if (dlg.exec() == QDialog::Accepted) {
		QSize sz = dlg.imageSize();
		if (dlg.from() == NewDialog::From::New) {
			euclase::Image image;
			image.make(sz.width(), sz.height(), euclase::Image::Format_F_RGBA);
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
			canvas()->changeSelection(op, r);
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
		setImage(euclase::Image(dstimage), true);
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
	int index = m->doc.addNewLayer();
	Canvas::Layer *p = m->doc.layer(index);
	setupBasicLayer(p);
	return index;
}

void MainWindow::setCurrentLayer(int index)
{
	m->doc.setCurrentLayer(index);
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
			euclase::FloatRGBA const *s = (euclase::FloatRGBA const *)srcimage.scanLine(y);
			euclase::FloatRGBA *d = (euclase::FloatRGBA *)newimage.scanLine(y);
			for (int x = 0; x < w; x++) {
				euclase::FloatRGB rgb(s[x].r, s[x].g, s[x].b);
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
				d[x] = euclase::FloatRGBA(rgb.r, rgb.g, rgb.b, A);
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
	filter(&fc, new FilterFormColorCorrection(this), [](FilterContext *context){
		FilterStatus s(context->cancel_ptr(), context->progress_ptr());
		ColorCorrectionParams params;
		params.hue = context->parameter("hue").toInt() / 360.f;
		params.saturation = context->parameter("saturation").toInt() / 100.f;
		params.brightness = context->parameter("brightness").toInt() / 100.f;
		return filter_color_correction(context->sourceImage(), params, &s);
	});
}

void MainWindow::test()
{
#if 1
	updateImageViewEntire();
#else
	openFile("/mnt/lucy/pub/pictures/favolite/white.png");
#endif
}




