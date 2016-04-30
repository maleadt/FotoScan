#pragma once

#include <QGraphicsView>
#include <QPoint>

class QGraphicsPolygonItem;

class GraphicsView : public QGraphicsView {
    Q_OBJECT
  public:
    GraphicsView(QWidget *parent = 0);
    GraphicsView(QGraphicsScene *scene, QWidget *parent = 0);
    virtual ~GraphicsView(){};

    void zoomIn();
    void zoomOut();

    void clear();

  protected:
    virtual void wheelEvent(QWheelEvent *event);
    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);

  private:
    QPoint mousePosition, dragPosition;
    QGraphicsPolygonItem *selected = nullptr;
    int dragCorner;
};
