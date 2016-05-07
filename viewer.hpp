#pragma once

#include <QMainWindow>

#include "graphicsview.hpp"
#include "detection.hpp"

class QGraphicsScene;
class QGraphicsView;
class QGraphicsItem;
class QAction;
struct ScanData;

class Viewer : public QMainWindow {
    Q_OBJECT

  public:
    Viewer();
    ~Viewer();
    void display(ScanData *);
    void clear();
    ScanData *current();

  protected:
    virtual void keyPressEvent(QKeyEvent *event);

  private slots:
    void zoomRestore();
    void showRejects();
    void showUngrouped();

  signals:
    void success(ScanData *);
    void failure(ScanData *, std::exception *);

  private:
    void updateActions();

    ScanData *data = nullptr;

    QGraphicsScene *scene;
    GraphicsView *view;

    QGraphicsPixmapItem *imageItem = nullptr;
    QList<QGraphicsItem *> rejectItems, ungroupedItems, shapeItems;

    QAction *zoomInAct;
    QAction *zoomOutAct;
    QAction *zoomRestoreAct;

    QAction *showRejectsAct;
    QAction *showUngroupedAct;
};
