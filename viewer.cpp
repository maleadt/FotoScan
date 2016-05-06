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

    zoomInAct =
        viewMenu->addAction(tr("Zoom &In"), view, &GraphicsView::zoomIn);
    zoomInAct->setShortcut(QKeySequence::ZoomIn);

    zoomOutAct =
        viewMenu->addAction(tr("Zoom &Out"), view, &GraphicsView::zoomOut);
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

Viewer::~Viewer() {
    delete scene;
    delete view;
}

void Viewer::display(ImageData *data) {
    this->data = data;
    try {
        data->load();
    } catch (std::exception *ex) {
        emit failure(data, ex);
        return;
    }

    setWindowFilePath(data->file);

    imageItem = scene->addPixmap(QPixmap::fromImage(data->image));

    QPen pen;

    pen = QPen(Qt::red, 10);
    for (auto polygon : data->rejects)
        rejectItems << scene->addPolygon(polygon, pen);
    showRejects();

    pen = QPen(Qt::blue, 10);
    for (auto polygon : data->ungrouped)
        ungroupedItems << scene->addPolygon(polygon, pen);
    showUngrouped();

    pen = QPen(Qt::green, 10);
    for (auto polygon : data->pictures)
        pictureItems << scene->addPolygon(polygon, pen);

    view->fitInView(scene->sceneRect(), Qt::KeepAspectRatio);
    updateActions();

    return;
}

void Viewer::clear() {
    data = nullptr;
    rejectItems.clear();
    ungroupedItems.clear();
    pictureItems.clear();

    scene->clear(); // This deletes the actual items
    view->clear();
}

ImageData *Viewer::current() { return data; }


//
// Slots
//

void Viewer::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if (data) {
            // only make pictures visible
            showRejectsAct->setChecked(false);
            showRejects();
            showUngroupedAct->setChecked(false);
            showUngrouped();

            data->pictures = QList<QPolygon>();
            for (auto item : scene->items())
                if (item->isVisible())
                    if (auto polygon_item =
                            qgraphicsitem_cast<QGraphicsPolygonItem *>(item))
                        data->pictures << polygon_item->polygon().toPolygon();

            emit success(data);
            return;
        }
    }

    QMainWindow::keyPressEvent(event);
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


//
// Private functionality
//

void Viewer::updateActions() {
    view->setInteractable(
        !(showUngroupedAct->isChecked() | showRejectsAct->isChecked()));
}