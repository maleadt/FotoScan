#include "scanner.hpp"

#include <QDirIterator>
#include <QStatusBar>
#include <QDebug>
#include <QImageReader>
#include <QImageWriter>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "detection.hpp"
#include "postprocessing.hpp"

// Although limited by QThreadPool, don't have too many detection tasks alive
// to reduce memory usage and allow the postprocess task to run
#define DETECTION_BUFFER 8
#define DETECTION_PRIORITY 1
#define POSTPROCESS_PRIORITY 0

using namespace std;


//
// ImageData
//

ImageData::ImageData(const QString &file) : file(file) {}

void ImageData::load() {
    if (image.isNull()) {
        QImageReader reader(file);
        reader.setAutoTransform(true);
        image = reader.read();
        if (image.isNull()) {
            throw new runtime_error(QString("Cannot load %1: %2")
                                        .arg(file, reader.errorString())
                                        .toStdString());
        }
    }
}

Scanner::Scanner(int &argc, char **argv) : QApplication(argc, argv) {
    QGuiApplication::setApplicationDisplayName("Foto Scanner");

#if defined(_OPENMP)
    pool.setMaxThreadCount(1);
#endif

    connect(&viewer, SIGNAL(success(ImageData *)), this,
            SLOT(onReviewSuccess(ImageData *)));
    connect(&viewer, SIGNAL(failure(ImageData *, std::exception *)), this,
            SLOT(onReviewFailure(ImageData *, std::exception *)));

    viewer.show();
}

static void fromJson(ImageData *data, QJsonDocument doc) {
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

static QJsonDocument toJson(const ImageData *data) {
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

int Scanner::scan() {
    if (inputDir == QDir())
        throw runtime_error("No input directory set");

    return scan(inputDir.absolutePath());
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
        QDir dir(finfo.absolutePath());
        QFile results(dir.absoluteFilePath(
            QString("%1.dat").arg(finfo.completeBaseName())));
        if (results.exists()) {
            if (results.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QString json = results.readAll();
                QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());

                ImageData *data = new ImageData(path);
                fromJson(data, doc);

                // TODO: we might want to correct some results,
                //       so conditionally allow that based on the arguments?
                queueLock.lock();
                toPostprocess << data;
                queueLock.unlock();
            } else {
                qCritical() << QString("Could not read results for %1: %2")
                                   .arg(path)
                                   .arg(results.errorString());
            }
        } else {
            queueLock.lock();
            toDetect << new ImageData(path);
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
        throw runtime_error(
            QString("Unable to handle %1").arg(path).toStdString());
}

void Scanner::setOutputDir(QString dir) { outputDir = QDir(dir); }

void Scanner::setInputDir(QString dir) { inputDir = QDir(dir); }

void Scanner::enqueue() {
    queueLock.lock();

    // review
    if (viewer.current() == nullptr && toReview.size() > 0) {
        auto data = toReview.takeFirst();
        const QString message =
            tr("Reviewing \"%1\",detected %2 pictures in %3 ms")
                .arg(inputDir.relativeFilePath(data->file))
                .arg(data->pictures.size())
                .arg(data->elapsed.count());
        viewer.statusBar()->showMessage(message);
        viewer.display(data);
    }

    // detection
    if (toDetect.size() > 0 && toReview.size() < DETECTION_BUFFER) {
        auto T = new DetectionTask(toDetect.takeFirst());
        connect(T, SIGNAL(success(ImageData *)), this,
                SLOT(onDetectionSuccess(ImageData *)));
        connect(T, SIGNAL(failure(ImageData *, std::exception *)), this,
                SLOT(onDetectionFailure(ImageData *, std::exception *)));
        pool.start(T, DETECTION_PRIORITY);
    }

    // postprocess
    if (toPostprocess.size() > 0) {
        auto T = new PostprocessTask(toPostprocess.takeFirst());
        connect(T, SIGNAL(success(ImageData *)), this,
                SLOT(onPostprocessSuccess(ImageData *)));
        connect(T, SIGNAL(failure(ImageData *, std::exception *)), this,
                SLOT(onPostprocessFailure(ImageData *, std::exception *)));
        pool.start(T, POSTPROCESS_PRIORITY);
    }

    queueLock.unlock();
}

void Scanner::onEventLoopStarted() {
    viewer.statusBar()->showMessage(
        QString("Loaded %1 image(s)").arg(toDetect.size()));

    enqueue();
}

void Scanner::onDetectionSuccess(ImageData *data) {
    queueLock.lock();
    toReview << data;
    queueLock.unlock();

    enqueue();
}

void Scanner::onDetectionFailure(ImageData *data, exception *ex) {
    qCritical() << QString("Detection for %1 failed: %2")
                       .arg(data->file)
                       .arg(ex->what());
    delete data;
    delete ex;

    enqueue();
}

void Scanner::onReviewSuccess(ImageData *data) {
    viewer.clear();

    QFileInfo finfo(data->file);
    QDir dir(finfo.absolutePath());
    QFile results(
        dir.absoluteFilePath(QString("%1.dat").arg(finfo.completeBaseName())));
    if (results.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream stream(&results);
        QJsonDocument doc = toJson(data);
        stream << doc.toJson();
    } else {
        qCritical() << QString("Could not write results for %1: %2")
                           .arg(data->file)
                           .arg(results.errorString());
    }

    queueLock.lock();
    toPostprocess << data;
    queueLock.unlock();

    enqueue();
}

void Scanner::onReviewFailure(ImageData *data, exception *ex) {
    viewer.clear();

    qCritical() << QString("Review for %1 failed: %2")
                       .arg(data->file)
                       .arg(ex->what());
    delete data;
    delete ex;

    enqueue();
}

void Scanner::onPostprocessSuccess(ImageData *data) {
    QImageReader reader(data->file);

    QString relative_input = inputDir.relativeFilePath(data->file);
    QString output = outputDir.absoluteFilePath(relative_input);
    QFileInfo finfo(output);
    for (int i = 0; i < data->images.size(); ++i) {
        auto image = data->images[i];
        QDir output_dir(finfo.absolutePath());
        QFile output_split(
            output_dir.absoluteFilePath(QString("%1_%2.%3")
                                            .arg(finfo.completeBaseName())
                                            .arg(i)
                                            .arg(finfo.suffix())));
        QString output_split_path = QFileInfo(output_split).absoluteFilePath();
        QDir().mkpath(QFileInfo(output_split).absolutePath());
        QImageWriter writer(output_split_path, reader.format());
        if (!writer.write(image)) {
            qCritical() << QString("Saving %1 failed: %2")
                               .arg(output_split_path)
                               .arg(writer.errorString());
        }
    }

    delete data;

    enqueue();
}

void Scanner::onPostprocessFailure(ImageData *data, exception *ex) {
    qCritical() << QString("Postprocess for %1 failed: %2")
                       .arg(data->file)
                       .arg(ex->what());
    delete data;
    delete ex;

    enqueue();
}