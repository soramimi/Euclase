#include "SelectionOutlineRenderer.h"
#include "MainWindow.h"

#include <QBitmap>

SelectionOutlineRenderer::SelectionOutlineRenderer(QObject *parent)
	: QThread(parent)
{
}

SelectionOutlineRenderer::~SelectionOutlineRenderer()
{
	abort();
}

void SelectionOutlineRenderer::run()
{
	while (requested_) {
		requested_ = false;
		SelectionOutlineBitmap data;
		if (!abort_) {
			data = mainwindow_->renderSelectionOutlineBitmap(&abort_);
		}
		if (abort_) break;
		emit done(data);
	}
}

void SelectionOutlineRenderer::request(MainWindow *mw, const QRect &rect)
{
	mainwindow_ = mw;
	rect_ = rect;
	requested_ = true;
	abort_ = false;
	if (!isRunning()) {
		start();
	}
}

void SelectionOutlineRenderer::abort()
{
	abort_ = true;
	wait();
}

