#pragma once

#include <QGraphicsView>

class GraphicsView : public QGraphicsView {
    Q_OBJECT
  public:
    GraphicsView(QWidget *parent = 0);
    GraphicsView(QGraphicsScene *scene, QWidget *parent = 0);
    virtual ~GraphicsView(){};

  protected:
    virtual void wheelEvent(QWheelEvent *event);
};
