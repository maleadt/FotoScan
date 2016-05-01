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
    void setInteractable(bool);

    void clear();

  protected:
    virtual void wheelEvent(QWheelEvent *event);
    virtual void mousePressEvent(QMouseEvent *event);
    virtual void mouseMoveEvent(QMouseEvent *event);
    virtual void mouseReleaseEvent(QMouseEvent *event);
    virtual void keyPressEvent(QKeyEvent *event);

  private:
    QGraphicsPolygonItem *findPolygon(QPoint location);

    QPoint mousePressPosition, mouseMovePosition;
    QGraphicsPolygonItem *selected = nullptr, *selectedAtPress;
    QGraphicsItem *pending = nullptr;
    int dragCorner;
    bool interactable = true;
};
