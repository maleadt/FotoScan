#include "graphicsview.hpp"

#include <QApplication>
#include <QWheelEvent>
#include <QDebug>
#include <QGraphicsPolygonItem>
#include <QMessageBox>

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

void GraphicsView::clear() {
    selected = nullptr;
    pending = nullptr;
    dragCorner = -1;
    interactable = true;
}

void GraphicsView::setInteractable(bool v) { interactable = v; }

void GraphicsView::wheelEvent(QWheelEvent *event) {
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    if (event->delta() > 0)
        zoomIn();
    else
        zoomOut();
}

static void select(QGraphicsPolygonItem *item) {
    auto color = item->pen().color();
    QPen pen(color, 2);
    color.setAlpha(64);

    item->setBrush(QBrush(color));
    item->setPen(pen);
}

static void unselect(QGraphicsPolygonItem *item) {
    auto color = item->pen().color();
    QPen pen(color, 10);

    item->setBrush(QBrush());
    item->setPen(pen);
}

// Find the polygon at the current location (if multiple, select smallest)
QGraphicsPolygonItem *GraphicsView::findPolygon(QPoint location) {
    float smallest_area = std::numeric_limits<float>::max();
    QGraphicsPolygonItem *smallest = nullptr;

    for (auto item : items(location)) {
        if (auto polygon = qgraphicsitem_cast<QGraphicsPolygonItem *>(item)) {
            auto rect = polygon->boundingRect();
            float area = rect.height() * rect.width();
            if (!smallest || area < smallest_area) {
                smallest = polygon;
                smallest_area = area;
            }
        }
    }
    return smallest;
}

void GraphicsView::mousePressEvent(QMouseEvent *event) {
    mousePressPosition = event->pos();
    mouseMoveEffect = false;

    if (event->button() == Qt::LeftButton) {
        setDragMode(QGraphicsView::ScrollHandDrag);
        QGraphicsView::mousePressEvent(event);
    } else if (interactable && event->button() == Qt::RightButton) {
        // start of a click: handle selection of polygons
        selectedAtPress = selected;
        if (!pending) { // only applies when not drawing a polygon
            if (auto polygon = findPolygon(mousePressPosition)) {
                if (selected == nullptr) {
                    // nothing selected, click inside a polygon --> select
                    select(polygon);
                    selected = polygon;
                } else if (selected != polygon) {
                    // click inside other polygon --> select & unselect other
                    unselect(selected);
                    select(polygon);
                    selected = polygon;
                }
            } else if (selected) {
                // didn't click inside a polygon, so unselect
                unselect(selected);
                selected = nullptr;
            }
        }

        dragCorner = -1;
    } else {
        QGraphicsView::mousePressEvent(event);
    }
}

void GraphicsView::mouseMoveEvent(QMouseEvent *event) {
    if (qApp->focusWidget() != this) {
        setCursor(Qt::ArrowCursor);
        setFocus(Qt::MouseFocusReason);
    }

    if (interactable && event->buttons() & Qt::RightButton) {
        // moving while polygon selected and right button held --> corner drag
        if (selected) {
            if (dragCorner == -1) {
                qreal min_distance = std::numeric_limits<qreal>::max();
                for (int i = 0; i < selected->polygon().size(); ++i) {
                    auto point = selected->polygon()[i];
                    qreal distance = (point - mapToScene(mousePressPosition))
                                         .manhattanLength();
                    if (distance < min_distance && distance < 500) {
                        dragCorner = i;
                        min_distance = distance;
                    }
                }
                if (dragCorner != -1)
                    mouseMovePosition = event->pos();
            } else {
                QPointF delta =
                    mapToScene(event->pos()) - mapToScene(mouseMovePosition);
                auto polygon = selected->polygon();
                polygon[dragCorner] += delta;
                selected->setPolygon(polygon);
                mouseMovePosition = event->pos();
                mouseMoveEffect = true;
            }
        }
    }

    QGraphicsView::mouseMoveEvent(event);
}

void GraphicsView::mouseReleaseEvent(QMouseEvent *event) {
    QPoint mouseReleasePosition = event->pos();

    if (event->button() == Qt::LeftButton) {
        QGraphicsView::mouseReleaseEvent(event);
        setCursor(Qt::ArrowCursor);
        setDragMode(QGraphicsView::NoDrag);
    } else if (interactable && event->button() == Qt::RightButton) {
        if (!mouseMoveEffect &&
            (mousePressPosition-mouseReleasePosition).manhattanLength() <= 10) {
            // right-button click while nothing is selected --> define new polygon
            if (!pending && !selected && !selectedAtPress) {
                // initial point is an ellipse
                QPen pen(Qt::yellow, 2);
                QBrush brush(Qt::yellow);
                QPointF pos = mapToScene(mousePressPosition);
                pending = scene()->addEllipse(pos.x() - 10, pos.y() - 10, 20,
                                              20, pen, brush);
            } else if (pending) {
                // replace the initial ellipse with a proper polygon
                if (auto ellipse_item =
                        qgraphicsitem_cast<QGraphicsEllipseItem *>(pending)) {
                    QPolygonF polygon;
                    polygon << ellipse_item->rect().center();
                    pending = scene()->addPolygon(polygon, ellipse_item->pen(),
                                                  ellipse_item->brush());
                    scene()->removeItem(ellipse_item);
                    delete ellipse_item;
                }

                QPolygonF polygon;
                auto polygon_item =
                    qgraphicsitem_cast<QGraphicsPolygonItem *>(pending);
                polygon << polygon_item->polygon();

                polygon << mapToScene(mousePressPosition);
                polygon_item->setPolygon(polygon);

                // make sure the pending item is selected so that we can drag
                // corners
                select(polygon_item);
                selected = polygon_item;
            }
        }

    } else {
        QGraphicsView::mouseReleaseEvent(event);
    }
}

void GraphicsView::keyPressEvent(QKeyEvent *event) {
    // return while pending polygon --> accept
    if (pending &&
        (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)) {
        auto polygon_item = qgraphicsitem_cast<QGraphicsPolygonItem *>(pending);
        if (!polygon_item || polygon_item->polygon().size() != 4) {
            // polygon has wrong amount of points
            scene()->removeItem(pending);
            delete pending;
            selected = nullptr;
            pending = nullptr;
            QMessageBox messageBox;
            messageBox.critical(0, "Error", "Picture should have 4 corners");
            return;
        }

        polygon_item->setPen(QPen(Qt::green));
        unselect(polygon_item);
        selected = nullptr;
        pending = nullptr;

        return;
    }

    // escape while pending polygon --> cancel
    if (pending &&
        (event->key() == Qt::Key_Escape || event->key() == Qt::Key_Delete)) {
        scene()->removeItem(pending);
        delete pending;
        selected = nullptr;
        pending = nullptr;
        return;
    }

    // delete while selected polygon --> delete
    if (selected && event->key() == Qt::Key_Delete) {
        scene()->removeItem(selected);
        delete selected;
        selected = nullptr;
        return;
    }

    QGraphicsView::keyPressEvent(event);
}
