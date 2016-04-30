#include "graphicsview.hpp"

#include <QApplication>
#include <QWheelEvent>

static const double scaleFactor = 1.15;
static double currentScale = 1.0; // stores the current scale value.

GraphicsView::GraphicsView(QWidget *parent) : QGraphicsView(parent) {}

GraphicsView::GraphicsView(QGraphicsScene *scene, QWidget *parent)
    : QGraphicsView(scene, parent) {}

void GraphicsView::zoomIn() {
    scale(scaleFactor, scaleFactor);
    currentScale *= scaleFactor;
}

void GraphicsView::zoomOut() {
    scale(1 / scaleFactor, 1 / scaleFactor);
    currentScale /= scaleFactor;
}

void GraphicsView::wheelEvent(QWheelEvent *event) {
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    if (event->delta() > 0)
        zoomIn();
    else
        zoomOut();
}

void GraphicsView::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::RightButton) {
        setDragMode(QGraphicsView::ScrollHandDrag);

        // simulate a left click
        QMouseEvent mouseEvent(event->type(), event->pos(), Qt::LeftButton,
                               Qt::LeftButton, event->modifiers());
        QGraphicsView::mousePressEvent(&mouseEvent);
    } else
        QGraphicsView::mousePressEvent(event);
}

void GraphicsView::mouseMoveEvent(QMouseEvent *event) {
    if (qApp->focusWidget() != this) {
        setCursor(Qt::ArrowCursor);
        setFocus(Qt::MouseFocusReason);
    }

    QGraphicsView::mouseMoveEvent(event);
}

void GraphicsView::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::RightButton) {
        QGraphicsView::mouseReleaseEvent(event);
        setCursor(Qt::ArrowCursor);
        setDragMode(QGraphicsView::NoDrag);
    } else
        QGraphicsView::mouseReleaseEvent(event);
}
