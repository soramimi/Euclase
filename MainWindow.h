#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "AbstractFilterForm.h"
#include "Canvas.h"
#include "SelectionOutline.h"
#include <QMainWindow>

class Brush;

class FilterContext;

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow {
	Q_OBJECT
	friend class FilterDialog;
public:
	enum class Tool {
		Scroll,
		Brush,
		EraserBrush,
		Rect,
	};
	enum RectHandle {
		None,
		Center,
		Top,
		Left,
		Right,
		Bottom,
		TopLeft,
		TopRight,
		BottomLeft,
		BottomRight,
	};
private:
	Ui::MainWindow *ui;

	struct Private;
	Private *m;

	void setImage(euclase::Image image, bool fitview);
	void setImageFromBytes(QByteArray const &ba, bool fitview);
	void setFilteredImage(const euclase::Image &image, bool apply);

	enum class Operation {
		PaintToCurrentLayer,
		PaintToCurrentAlternate,
	};
	void paintLayer(Operation op, const Canvas::Layer &layer);

	void drawBrush(bool one);
	void updateImageViewEntire();
	void updateSelectionOutline();
	void setColorRed(int value);
	void setColorGreen(int value);
	void setColorBlue(int value);
	void setColorHue(int value);
	void setColorSaturation(int value);
	void setColorValue(int value);
	euclase::Image renderFilterTargetImage();
	void onSelectionChanged();
	euclase::Image selectedImage() const;
	MainWindow::RectHandle rectHitTest(const QPoint &pt) const;
	QPointF pointOnCanvas(int x, int y) const;
	QPointF mapToCanvasFromViewport(const QPointF &pt) const;
	QPointF mapToViewportFromCanvas(const QPointF &pt) const;
	void setRect();
	void clearCanvas();
	void hideRect(bool update);
	bool isRectVisible() const;
	QRect boundsRect() const;
	void resetView(bool fitview);
	void filterStart(FilterContext &&context, AbstractFilterForm *form, const std::function<euclase::Image (FilterContext *)> &fn);
	void filter_xBRZ(int factor);
	void resetCurrentAlternateOption(Canvas::BlendMode blendmode = Canvas::BlendMode::Normal);
	void applyCurrentAlternateLayer(bool lock = true);
	int addNewLayer();
	void setupBasicLayer(Canvas::Layer *p);
	void colorCollection();
	bool mouseMove_internal(int x, int y, bool leftbutton, bool set_cursor_only);
	Canvas::RenderOption2 renderOption() const;
	void setFilerDialogActive(bool active);
protected:
	void keyPressEvent(QKeyEvent *event);
public:
	explicit MainWindow(QWidget *parent = 0);
	~MainWindow();

	Canvas *canvas();
	Canvas const *canvas() const;

	void fitView();
	Canvas::Panel renderToPanel(Canvas::InputLayerMode input_layer_mode, euclase::Image::Format format, QRect const &r, const QRect &maskrect, const Canvas::RenderOption &opt, bool *abort) const;
	euclase::Image renderToImage(euclase::Image::Format format, QRect const &r, Canvas::RenderOption const &opt, bool *abort) const;
	QRect selectionRect() const;
	void openFile(const QString &path);
	int canvasWidth() const;
	int canvasHeight() const;
	QColor foregroundColor() const;
	const Brush &currentBrush() const;
	void changeTool(Tool tool);
	MainWindow::Tool currentTool() const;
	SelectionOutline renderSelectionOutline(bool *abort);
	void setColor(QColor primary_color, QColor secondary_color);
	void updateToolCursor();
public slots:
	void setCurrentColor(const QColor &primary_color);
	void setCurrentBrush(const Brush &brush);
	void onPenDown(double x, double y);
	void onPenStroke(double x, double y);
	void onPenUp(double x, double y);
	bool onMouseLeftButtonPress(int x, int y);
	bool onMouseMove(int x, int y, bool leftbutton);
	bool onMouseLeftButtonRelase(int x, int y, bool leftbutton);
private slots:
	void on_action_file_open_triggered();
	void on_action_file_save_as_triggered();
	void on_action_filter_antialias_triggered();
	void on_action_filter_blur_triggered();
	void on_action_filter_maximize_triggered();
	void on_action_filter_median_triggered();
	void on_action_filter_minimize_triggered();
	void on_action_filter_sepia_triggered();
	void on_action_resize_triggered();
	void on_action_trim_triggered();
	void on_horizontalScrollBar_valueChanged(int value);
	void on_verticalScrollBar_valueChanged(int value);
	void on_horizontalSlider_size_valueChanged(int value);
	void on_horizontalSlider_softness_valueChanged(int value);
	void on_spinBox_brush_size_valueChanged(int value);
	void on_spinBox_brush_softness_valueChanged(int value);
	void on_horizontalSlider_rgb_r_valueChanged(int value);
	void on_horizontalSlider_rgb_g_valueChanged(int value);
	void on_horizontalSlider_rgb_b_valueChanged(int value);
	void on_spinBox_rgb_r_valueChanged(int value);
	void on_spinBox_rgb_g_valueChanged(int value);
	void on_spinBox_rgb_b_valueChanged(int value);
	void on_horizontalSlider_hsv_h_valueChanged(int value);
	void on_horizontalSlider_hsv_s_valueChanged(int value);
	void on_horizontalSlider_hsv_v_valueChanged(int value);
	void on_spinBox_hsv_h_valueChanged(int value);
	void on_spinBox_hsv_s_valueChanged(int value);
	void on_spinBox_hsv_v_valueChanged(int value);
	void on_toolButton_scroll_clicked();
	void on_toolButton_paint_brush_clicked();
	void on_toolButton_eraser_brush_clicked();
	void on_toolButton_rect_clicked();
	void on_action_clear_bounds_triggered();
	void on_action_edit_copy_triggered();
	void on_action_filter_2xBRZ_triggered();
	void on_action_filter_4xBRZ_triggered();
	void on_action_new_triggered();
	void on_action_select_rectangle_triggered();
	void on_action_settings_triggered();
	void test();
	
	
	void filterClose(bool apply);
public:
	bool eventFilter(QObject *watched, QEvent *event);
	void setToolCursor(const QCursor &cursor);
	void setPreviewLayerEnable(bool enabled);
	bool isPreviewEnabled() const;
	void setCurrentLayer(int index);
	euclase::Image::MemoryType preferredMemoryType() const;
	void updateImageView(const QRect &canvasrect); // canvasrect is in canvas coordinate
	std::mutex &mutexForCanvas() const;
	euclase::Image renderSelection(const QRect &r, bool *abort) const;
	bool isFilterDialogActive() const;
protected:
	void dragEnterEvent(QDragEnterEvent *event);
	void dropEvent(QDropEvent *event);

	// QWidget interface
protected:
	void closeEvent(QCloseEvent *);
};

#endif // MAINWINDOW_H
