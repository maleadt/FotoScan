#include "graphicsview.hpp"

#include <QApplication>
#include <QWheelEvent>
#include <QDebug>
#include <QGraphicsPolygonItem>

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
    mousePosition[event->button()] = event->pos();

    if (event->button() == Qt::LeftButton) {
        setDragMode(QGraphicsView::ScrollHandDrag);
        QGraphicsView::mousePressEvent(event);
    } else {
        QGraphicsView::mousePressEvent(event);
    }
}

void GraphicsView::mouseMoveEvent(QMouseEvent *event) {
    QPoint start = mousePosition[event->button()];

    if (qApp->focusWidget() != this) {
        setCursor(Qt::ArrowCursor);
        setFocus(Qt::MouseFocusReason);
    }

    QGraphicsView::mouseMoveEvent(event);
}

static void select(QGraphicsPolygonItem* item) {
    auto color = item->pen().color();
    QPen pen(color, 2);
    color.setAlpha(64);

    item->setBrush(QBrush(color));
    item->setPen(pen);
}

static void unselect(QGraphicsPolygonItem* item) {
    auto color = item->pen().color();
    QPen pen(color, 10);

    item->setBrush(QBrush());
    item->setPen(pen);
}

void GraphicsView::mouseReleaseEvent(QMouseEvent *event) {
    QPoint start = mousePosition[event->button()];

    if (event->button() == Qt::LeftButton) {
        QGraphicsView::mouseReleaseEvent(event);
        setCursor(Qt::ArrowCursor);
        setDragMode(QGraphicsView::NoDrag);
    } else if (event->button() == Qt::RightButton) {
        if (start == event->pos()) {
            // dealing with a click
            size_t matches = 0;
            for (auto item: items(start)) {
                if (auto polygon = qgraphicsitem_cast<QGraphicsPolygonItem*>(item)) {
                    matches++;
                    if (selected == nullptr) {
                        // nothing selected, click inside a polygon --> select
                        select(polygon);
                        selected = polygon;
                    } else if (selected == polygon) {
                        // click inside selected polygon --> unselect
                        unselect(selected);
                        selected = nullptr;
                    } else {
                        // click inside other polygon --> select & unselect other
                        unselect(selected);
                        select(polygon);
                        selected = polygon;
                    }
                }
            }

            if (matches == 0 && selected) {
                // didn't click inside a polygon, so unselect
                unselect(selected);
                selected = nullptr;
            }
        }
    } else {
        QGraphicsView::mouseReleaseEvent(event);
    }
}
