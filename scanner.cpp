#include "scanner.hpp"

#include <QDirIterator>
#include <QStatusBar>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

// Although limited by QThreadPool, don't have too many detection tasks alive
// to reduce memory usage
#define DETECTION_BUFFER 8

Scanner::Scanner(int &argc, char **argv) : QApplication(argc, argv) {
    QGuiApplication::setApplicationDisplayName("Foto Scanner");

#if defined(_OPENMP)
    pool.setMaxThreadCount(1);
#endif

    connect(&viewer, SIGNAL(success(DetectionData *)), this,
            SLOT(onReviewSuccess(DetectionData *)));
    connect(&viewer, SIGNAL(failure(DetectionData *, std::exception *)), this,
            SLOT(onReviewFailure(DetectionData *, std::exception *)));

    viewer.show();
}

static void fromJson(DetectionData *data, QJsonDocument doc) {
    QJsonObject root = doc.object();
    QJsonArray json_pictures = root["pictures"].toArray();
    for (auto json_picture_obj : json_pictures) {
        auto json_picture = json_picture_obj.toArray();
        QPolygon picture;
        for (auto json_point_obj : json_picture) {
            auto json_point = json_point_obj.toObject();
            picture << QPoint(json_point["x"].toInt(), json_point["y"].toInt());
        }
        data->pictures << picture;
    }
}

static QJsonDocument toJson(const DetectionData *data) {
    QJsonArray json_pictures;
    for (auto picture : data->pictures) {
        QJsonArray json_picture;
        for (auto point : picture) {
            QJsonObject json_point;
            json_point["x"] = point.x();
            json_point["y"] = point.y();
            json_picture << json_point;
        }
        json_pictures << json_picture;
    }

    QJsonObject root;
    root["pictures"] = json_pictures;

    QJsonDocument doc;
    doc.setObject(root);

    return doc;
}

int Scanner::scan(QString path) {
    QFileInfo finfo(path);
    if (finfo.isFile()) {
        // check file extension
        const QStringList extensions = {"jpg", "png"};
        bool match = false;
        for (auto extension : extensions) {
            if (QString::compare(finfo.suffix(), extension,
                                 Qt::CaseInsensitive) == 0) {
                match = true;
                break;
            }
        }
        if (!match)
            return 0;

        // check if already processed
        // TODO: duplicate code
        QFile results(QString("%1/%2.dat")
                          .arg(finfo.absolutePath())
                          .arg(finfo.completeBaseName()));
        if (results.exists()) {
            if (results.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QString json = results.readAll();
                QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());

                DetectionData *data = new DetectionData(path);
                fromJson(data, doc);

                queueLock.lock();
                toReview << data;
                queueLock.unlock();
            } else {
                qCritical() << QString("Could not read results for %1: %2")
                                   .arg(path)
                                   .arg(results.errorString());
            }
        } else {
            queueLock.lock();
            toDetect << path;
            queueLock.unlock();
        }
        return 1;

    } else if (finfo.isDir()) {
        size_t files = 0;
        QDirIterator it(path, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext())
            files += scan(it.next());
        return files;
    } else
        throw std::runtime_error(
            QString("Unable to handle %1").arg(path).toStdString());
}

void Scanner::enqueueDetection() {
    queueLock.lock();
    if (toDetect.size() > 0 && toReview.size() < DETECTION_BUFFER) {
        auto T = new DetectionTask(toDetect.takeFirst());
        connect(T, SIGNAL(success(DetectionData *)), this,
                SLOT(onDetectionSuccess(DetectionData *)));
        connect(T, SIGNAL(failure(DetectionData *, std::exception *)), this,
                SLOT(onDetectionFailure(DetectionData *, std::exception *)));
        pool.start(T);
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
    enqueueReview();
    enqueueDetection();
}

void Scanner::onDetectionSuccess(DetectionData *data) {
    queueLock.lock();
    toReview << data;
    queueLock.unlock();

    enqueueReview();
    enqueueDetection();
}

void Scanner::onDetectionFailure(DetectionData *data, std::exception *ex) {
    qCritical() << QString("Detection for %1 failed: %2")
                       .arg(data->file)
                       .arg(ex->what());
    delete data;
    delete ex;

    enqueueDetection();
}

void Scanner::onReviewSuccess(DetectionData *data) {
    QJsonDocument doc = toJson(data);

    QFileInfo finfo(data->file);
    QFile results(QString("%1/%2.dat")
                      .arg(finfo.absolutePath())
                      .arg(finfo.completeBaseName()));
    if (results.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream stream(&results);
        stream << doc.toJson();
    } else {
        qCritical() << QString("Could not write results for %1: %2")
                           .arg(data->file)
                           .arg(results.errorString());
    }

    delete data;

    viewer.clear();
    enqueueReview();
    enqueueDetection();
}

void Scanner::onReviewFailure(DetectionData *data, std::exception *ex) {
    qCritical() << QString("Review for %1 failed: %2")
                       .arg(data->file)
                       .arg(ex->what());
    delete data;
    delete ex;

    viewer.clear();
    enqueueReview();
    enqueueDetection();
}
