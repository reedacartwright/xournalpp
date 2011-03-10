#include "EraseHandler.h"
#include "../../model/eraser/EraseableStroke.h"
#include "../../model/Layer.h"
#include "../../model/Stroke.h"
#include "../../model/Page.h"
#include "../../model/Document.h"
#include "../../util/Range.h"
#include "../../util/Rectangle.h"
#include "../../undo/UndoRedoHandler.h"
#include "../../undo/EraseUndoAction.h"
#include "../../undo/DeleteUndoAction.h"
#include "../../gui/PageView.h"
#include "../ToolHandler.h"
#include "../../control/jobs/RenderJob.h"

#include "../../gui/XournalWidget.h"

#include <math.h>

EraseHandler::EraseHandler(UndoRedoHandler * undo, Document * doc, XojPage * page, ToolHandler * handler, PageView * view) {
	this->page = page;
	this->handler = handler;
	this->view = view;
	this->doc = doc;

	this->eraseDeleteUndoAction = NULL;
	this->eraseUndoAction = NULL;
	this->undo = undo;

	this->halfEraserSize = 0;
}

EraseHandler::~EraseHandler() {
	if (this->eraseDeleteUndoAction) {
		this->finalize();
	}
}

/**
 * Handle eraser event: Delete Stroke and Standard, Whiteout is not handled here
 */
void EraseHandler::erase(double x, double y) {
	ListIterator<Layer*> it = this->page->layerIterator();

	int selected = page->getSelectedLayerId();

	this->halfEraserSize = this->handler->getThikness();
	GdkRectangle eraserRect = { x - halfEraserSize, y - halfEraserSize, halfEraserSize * 2, halfEraserSize * 2 };

	while (it.hasNext() && selected) {
		Layer * l = it.next();

		ListIterator<Element *> eit = l->elementIterator();
		eit.freeze();
		while (eit.hasNext()) {
			Element * e = eit.next();
			if (e->getType() == ELEMENT_STROKE && e->intersectsArea(&eraserRect)) {
				Stroke * s = (Stroke *) e;

				eraseStroke(l, s, x, y);
			}
		}

		selected--;
	}
}

void EraseHandler::eraseStroke(Layer * l, Stroke * s, double x, double y) {
	if (!s->intersects(x, y, halfEraserSize)) {
		return;
	}

	// delete complete element
	if (this->handler->getEraserType() == ERASER_TYPE_DELETE_STROKE) {
		this->doc->lock();
		int pos = l->removeElement(s, false);
		this->doc->unlock();

		if (pos == -1) {
			return;
		}
		this->view->repaint(s);

		if (!this->eraseDeleteUndoAction) {
			this->eraseDeleteUndoAction = new DeleteUndoAction(this->page, this->view, true);
			this->undo->addUndoAction(this->eraseDeleteUndoAction);
		}

		this->eraseDeleteUndoAction->addElement(l, s, pos);
	} else { // Default eraser
		int pos = l->indexOf(s);
		if (pos == -1) {
			return;
		}

		if (eraseUndoAction == NULL) {
			eraseUndoAction = new EraseUndoAction(this->page, this->view);
			this->undo->addUndoAction(eraseUndoAction);
		}

		double repaintX = s->getX();
		double repaintY = s->getY();
		double repaintWidth = s->getElementWidth();
		double repaintHeight = s->getElementHeight();

		EraseableStroke * eraseable = NULL;
		if (s->getEraseable() == NULL) {
			doc->lock();
			eraseable = new EraseableStroke(s);
			s->setEraseable(eraseable);
			doc->unlock();
			eraseUndoAction->addOriginal(l, s, pos);
		} else {
			eraseable = s->getEraseable();
		}

		Range * rect = eraseable->erase(x, y, halfEraserSize);
		if (rect) {

			// TODO: debug

			//			Rectangle r(*rect);
			//			RenderJob::repaintRectangle(this->view, &r);
			//
			//			GdkRectangle gdkRect = { rect->getX(), rect->getY(), rect->getWidth(), rect->getHeight() };
			//			GdkRegion * region = gdk_region_rectangle(&gdkRect);
			//
			//			GdkEventExpose event = { GDK_EXPOSE, gtk_widget_get_window(this->view->getWidget()), 0, gdkRect, region, 0 };
			//
			//
			//
			//			gdk_window_begin_paint_region(event.window, event.region);
			//			gtk_widget_send_expose(this->view->getWidget(), (GdkEvent *) &event);
			//			gdk_window_end_paint(event.window);

			printf("%lf/%lf %lf %lf\n",rect->getX(), rect->getY() , rect->getWidth(), rect->getHeight());

			//			this->view->paintPage(&event);

			//			g_free(region);

			//			((Redrawable*)this->view)->redraw(*rect);

			this->view->repaint(*rect);

			delete rect;
		}
		// TODO: debug
		//this->view->repaint();
	}
}

void EraseHandler::finalize() {
	if (this->eraseUndoAction) {
		this->eraseUndoAction->finalize();
		this->eraseUndoAction = NULL;
	}
}