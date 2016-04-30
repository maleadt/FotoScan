#include "scanner.hpp"

#include <QDirIterator>
#include <QStatusBar>
#include <QDebug>
#include <QThreadPool>

#define BUFFER 8

Scanner::Scanner(int &argc, char **argv) : QApplication(argc, argv) {
    QGuiApplication::setApplicationDisplayName("Foto Scanner");

    connect(&viewer, SIGNAL(finished(DetectionData *)), this,
            SLOT(onReviewFinished(DetectionData *)));

    viewer.show();
}

int Scanner::scan(QString path) {
    QFileInfo finfo(path);
    if (finfo.isFile()) {
        queueLock.lock();
        toDetect << path;
        queueLock.unlock();
        return 1;
    } else if (finfo.isDir()) {
        size_t files = 0;
        QDirIterator it(path, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext())
            files += scan(it.next());
        return files;
    } else
        throw QString("Unable to handle %1").arg(path);
}

void Scanner::enqueueDetection() {
    queueLock.lock();
    if (toReview.size() < BUFFER) {
        auto T = new DetectionTask(toDetect.takeFirst());
        connect(T, SIGNAL(finished(DetectionData *)), this,
                SLOT(onDetectionFinished(DetectionData *)));
        QThreadPool::globalInstance()->start(T);
    }
    queueLock.unlock();
}

void Scanner::enqueueReview() {
    queueLock.lock();
    if (viewer.current() == nullptr && toReview.size() > 0) {
        viewer.display(toReview.takeFirst());
    }
    queueLock.unlock();
}

void Scanner::onEventLoopStarted() {
    viewer.statusBar()->showMessage(
        QString("Loaded %1 image(s)").arg(toDetect.size()));
    enqueueDetection();
}

void Scanner::onDetectionFinished(DetectionData *data) {
    queueLock.lock();
    toReview << data;
    queueLock.unlock();

    enqueueReview();
    enqueueDetection();
}

void Scanner::onReviewFinished(DetectionData *data) {
    viewer.clear();
    enqueueReview();
    enqueueDetection();

    // TODO: save data in another thread

    delete data;
}
