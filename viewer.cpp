#include "viewer.hpp"

#include <QtWidgets>

#include "scanner.hpp"

Viewer::Viewer() : scene(new QGraphicsScene), view(new GraphicsView(scene)) {
    setCentralWidget(view);
    view->show();

    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));

    QAction *exitAct = fileMenu->addAction(tr("E&xit"), this, &QWidget::close);
    exitAct->setShortcut(tr("Ctrl+Q"));

    QMenu *viewMenu = menuBar()->addMenu(tr("&View"));

    zoomInAct = viewMenu->addAction(tr("Zoom &In"), view, &GraphicsView::zoomIn);
    zoomInAct->setShortcut(QKeySequence::ZoomIn);

    zoomOutAct = viewMenu->addAction(tr("Zoom &Out"), view, &GraphicsView::zoomOut);
    zoomOutAct->setShortcut(QKeySequence::ZoomOut);

    zoomRestoreAct =
        viewMenu->addAction(tr("&Restore"), this, &Viewer::zoomRestore);
    zoomRestoreAct->setShortcut(tr("Ctrl+S"));

    viewMenu->addSeparator();

    showRejectsAct =
        viewMenu->addAction(tr("Show &Rejects"), this, &Viewer::showRejects);
    showRejectsAct->setCheckable(true);
    showRejectsAct->setShortcut(tr("Ctrl+R"));

    showUngroupedAct = viewMenu->addAction(tr("Show Un&grouped"), this,
                                           &Viewer::showUngrouped);
    showUngroupedAct->setCheckable(true);
    showUngroupedAct->setShortcut(tr("Ctrl+G"));

    resize(QGuiApplication::primaryScreen()->availableSize() * 3 / 5);
}

bool Viewer::display(DetectionData *data) {
    this->data = data;

    setWindowFilePath(data->file);
    const QString message =
        tr("Showing \"%1\", %2x%3, detected %4 pictures in %5 ms")
            .arg(QDir::toNativeSeparators(data->file))
            .arg(data->image.width())
            .arg(data->image.height())
            .arg(data->pictures.size())
            .arg(data->elapsed.count());
    statusBar()->showMessage(message);

    imageItem = scene->addPixmap(QPixmap::fromImage(data->image));

    QPen pen;

    pen = QPen(Qt::red, 10);
    for (auto polygon : data->rejects)
        rejectItems << scene->addPolygon(polygon, pen);
    showRejects();

    pen = QPen(Qt::yellow, 10);
    for (auto polygon : data->ungrouped)
        ungroupedItems << scene->addPolygon(polygon, pen);
    showUngrouped();

    pen = QPen(Qt::green, 10);
    for (auto polygon : data->pictures)
        pictureItems << scene->addPolygon(polygon, pen);

    view->fitInView(scene->sceneRect(), Qt::KeepAspectRatio);
    updateActions();

    return true;
}

void Viewer::clear() {
    data = nullptr;
    rejectItems.clear();
    ungroupedItems.clear();
    pictureItems.clear();

    scene->clear(); // This deletes the actual items
    view->clear();
}

DetectionData *Viewer::current() { return data; }

void Viewer::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if (data) {
            emit finished(data);
        }
    }
}

void Viewer::zoomRestore() {
    view->fitInView(scene->sceneRect(), Qt::KeepAspectRatio);
}

void Viewer::showRejects() {
    bool showRejects = showRejectsAct->isChecked();

    for (auto item : rejectItems)
        item->setVisible(showRejects);

    updateActions();
}

void Viewer::showUngrouped() {
    bool showUngrouped = showUngroupedAct->isChecked();

    for (auto item : ungroupedItems)
        item->setVisible(showUngrouped);

    updateActions();
}

void Viewer::updateActions() {}