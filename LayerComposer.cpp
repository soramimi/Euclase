
#include "ApplicationGlobal.h"
#include "LayerComposer.h"
#include "MainWindow.h"
#include <QPainter>

struct LayerComposer::Private {
	MainWindow *mainwindow;
	bool interrupted = false;
	bool cancel = false;
	std::mutex mutex;
	std::thread thread;
	std::condition_variable cond;

	LayerComposer::Request req;

	Canvas::Layer composed_panels;

	int div = 2;
};


LayerComposer::LayerComposer(QObject *parent)
	: m(new Private)
{
	start();
}

LayerComposer::~LayerComposer()
{
	stop();
	delete m;
}

void LayerComposer::init(MainWindow *mw)
{
	m->mainwindow = mw;
}

void LayerComposer::run()
{
	while (1) {
		Request req;
		{
			std::unique_lock lock(m->mutex);
			if (isInterrupted()) break;
			if (m->req.rect.isNull()) {
				m->cond.wait(lock);
			}
			if (!m->req.rect.isNull()) {
				req = m->req;
				m->req = {};
			}
		}
		if (!req.rect.isNull()) {
			m->cancel = false;

			SelectionOutlineBitmap selection_outline_data;
			std::vector<Canvas::Panel> panels;
			{
				std::thread th([&](){
					selection_outline_data = m->mainwindow->renderSelectionOutlineBitmap(&m->cancel);
				});

				{
					const int doc_w = m->mainwindow->canvasWidth();
					const int doc_h = m->mainwindow->canvasHeight();
					std::vector<QPoint> pts;

					const int INTERNAL_PANEL_SIZE_2 = INTERNAL_PANEL_SIZE * m->div;
					{
						int nh = (doc_h + INTERNAL_PANEL_SIZE_2 - 1) / INTERNAL_PANEL_SIZE_2;
						int nw = (doc_w + INTERNAL_PANEL_SIZE_2 - 1) / INTERNAL_PANEL_SIZE_2;
						for (int i = 0; i < nh; i++) {
							int y = i * INTERNAL_PANEL_SIZE_2;
							for (int j = 0; j < nw; j++) {
								int x = j * INTERNAL_PANEL_SIZE_2;
								QRect r(x, y, INTERNAL_PANEL_SIZE_2, INTERNAL_PANEL_SIZE_2);
								if (r.intersects(req.rect)) {
									pts.emplace_back(x, y);
								}
							}
						}
					}

					std::sort(pts.begin(), pts.end(), [&](QPoint const &a, QPoint const &b){
						auto Center = [=](QPoint const &pt){
							return QPoint(pt.x() + INTERNAL_PANEL_SIZE_2 / 2, pt.y() + INTERNAL_PANEL_SIZE_2 / 2);
						};
						auto Distance = [](QPoint const &a, QPoint const &b){
							auto dx = a.x() - b.x();
							auto dy = a.y() - b.y();
							return sqrt(dx * dx + dy * dy);
						};
						QPoint ca = Center(a);
						QPoint cb = Center(b);
						return Distance(ca, req.center) < Distance(cb, req.center);
					});

#pragma omp parallel for schedule(static, 8) num_threads(8)
					for (int i = 0; i < pts.size(); i++) {
						if (m->cancel) continue;
						QPoint pt = pts[i];
						QRect rect(pt.x(), pt.y(), std::min(INTERNAL_PANEL_SIZE_2, doc_w - pt.x()), std::min(INTERNAL_PANEL_SIZE_2, doc_h - pt.y()));
						Canvas::Panel panel = m->mainwindow->renderToPanel(Canvas::AllLayers, euclase::Image::Format_F_RGBA, rect, req.rect, &m->cancel);
						if (panel.imagep()->memtype() == euclase::Image::CUDA) {
							int sw = panel.imagep()->width();
							int sh = panel.imagep()->height();
							int dw = rect.width() / m->div;
							int dh = rect.height() / m->div;
							int psize = euclase::bytesPerPixel(panel.imagep()->format());
							euclase::Image newimg(dw, dh, panel.imagep()->format(), panel.imagep()->memtype());
							global->cuda->scale(dw, dh, psize * dw, newimg.data(), sw, sh, psize * sw, panel.imagep()->data(), psize);
							*panel.imagep() = newimg;
						} else {
							*panel.imagep() = panel.imagep()->scaled(rect.width() / m->div, rect.height() / m->div, false);
						}
						panel.setOffset(pt.x() / m->div, pt.y() / m->div);
						{
							std::lock_guard lock(m->mutex);
							panels.emplace_back(panel);
							m->composed_panels.primary_panels = panels;
							Canvas::Layer::sort(&m->composed_panels.primary_panels);
						}
						emit update();
					}
				}

				th.join();
			}

			if (m->cancel) {
				{
					std::lock_guard lock(m->mutex);
					m->composed_panels.primary_panels.clear();
				}
				m->cancel = false;
			} else {
				{
					std::lock_guard lock(m->mutex);
					std::swap(m->composed_panels.primary_panels, panels);
					Canvas::Layer::sort(&m->composed_panels.primary_panels);
				}

				RenderedImage ri;
				ri.selection_outline_data = selection_outline_data;
				emit done(ri);
			}
		}
	}
}

euclase::Image LayerComposer::render(int x, int y, int w, int h)
{
	Canvas::Panel panel;
	panel.imagep()->make(w / m->div, h / m->div, euclase::Image::Format_F_RGBA, m->composed_panels.memtype_);
	panel.imagep()->fill(euclase::k::transparent);
	panel.setOffset(x / m->div, y / m->div);
	{
		std::vector<Canvas::Layer *> layers;
		layers.push_back(&m->composed_panels);
		std::lock_guard lock(m->mutex);
		Canvas::renderToEachPanels(&panel, QPoint(), layers, nullptr, QColor(), 255, {}, &m->cancel);
	}
	return panel.image();
}

void LayerComposer::start()
{
	m->thread = std::thread([&](){
		run();
	});
}

void LayerComposer::wait()
{
	if (m->thread.joinable()) {
		m->thread.join();
	}
}

void LayerComposer::stop()
{
	requestInterruption();
	wait();
}

void LayerComposer::cancel()
{
	m->cancel = true;
}

void LayerComposer::reset()
{
	std::lock_guard lock(m->mutex);
	m->composed_panels = {};
	m->composed_panels.memtype_ = global->cuda ? euclase::Image::CUDA : euclase::Image::Host;
}

void LayerComposer::request(const QRect &rect, QPoint const &center, int div)
{
	std::lock_guard lock(m->mutex);
	m->req.rect = rect;
	m->req.center = center;
	m->div = div;
	m->cond.notify_all();
}

void LayerComposer::requestInterruption()
{
	std::lock_guard lock(m->mutex);
	m->interrupted = true;
	m->cond.notify_all();
}

bool LayerComposer::isInterrupted()
{
	return m->interrupted;
}


