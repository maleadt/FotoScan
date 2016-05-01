#pragma once

#include <QMainWindow>

#include "graphicsview.hpp"
#include "detection.hpp"

class QGraphicsScene;
class QGraphicsView;
class QGraphicsItem;
class QAction;
struct DetectionData;

class Viewer : public QMainWindow {
    Q_OBJECT

  public:
    Viewer();
    bool display(DetectionData *);
    void clear();
    DetectionData *current();

  protected:
    virtual void keyPressEvent(QKeyEvent *event);

  private slots:
    void zoomRestore();
    void showRejects();
    void showUngrouped();

  signals:
    void finished(DetectionData *);

  private:
    void updateActions();

    DetectionData *data = nullptr;

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
