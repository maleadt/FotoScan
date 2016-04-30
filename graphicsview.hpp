#pragma once

#include <QGraphicsView>
#include <QPoint>
#include <QMap>

class QGraphicsPolygonItem;

class GraphicsView : public QGraphicsView {
    Q_OBJECT
  public:
    GraphicsView(QWidget *parent = 0);
    GraphicsView(QGraphicsScene *scene, QWidget *parent = 0);
    virtual ~GraphicsView(){};

    void zoomIn();
    void zoomOut();

  protected:
    virtual void wheelEvent(QWheelEvent *event);
    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);

  private:
    QMap<Qt::MouseButton, QPoint> mousePosition;
    QGraphicsPolygonItem *selected = nullptr;
};
