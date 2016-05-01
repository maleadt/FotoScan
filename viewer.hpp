#pragma once

#include <QMainWindow>

#include "graphicsview.hpp"
#include "detection.hpp"

class QGraphicsScene;
class QGraphicsView;
class QGraphicsItem;
class QAction;
struct ImageData;

class Viewer : public QMainWindow {
    Q_OBJECT

  public:
    Viewer();
    void display(ImageData *);
    void clear();
    ImageData *current();

  protected:
    virtual void keyPressEvent(QKeyEvent *event);

  private slots:
    void zoomRestore();
    void showRejects();
    void showUngrouped();

  signals:
    void success(ImageData *);
    void failure(ImageData *, std::exception *);

  private:
    void updateActions();

    ImageData *data = nullptr;

    QGraphicsScene *scene;
    GraphicsView *view;

    QGraphicsPixmapItem *imageItem = nullptr;
    QList<QGraphicsItem *> rejectItems, ungroupedItems, pictureItems;

    QAction *zoomInAct;
    QAction *zoomOutAct;
    QAction *zoomRestoreAct;

    QAction *showRejectsAct;
    QAction *showUngroupedAct;
};
