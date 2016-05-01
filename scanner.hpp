#pragma once

#include <QApplication>
#include <QMutex>
#include <QThreadPool>
#include <QImage>
#include <QDir>

#include "viewer.hpp"

#include <chrono>

struct ImageData {
    QString file;
    QImage image;
    ImageData(const QString &file);
    void load();

    // result of detection
    // NOTE: actually rects, but easier to represent as 4 points
    QList<QPolygon> rejects, ungrouped, pictures;

    // result of post-processing
    QList<QImage> images;

    std::chrono::milliseconds elapsed = std::chrono::milliseconds::zero();
};

class Scanner : public QApplication {
    Q_OBJECT

  public:
    Scanner(int &argc, char *argv[]);
    int scan();
    void setOutputDir(QString dir);
    void setInputDir(QString dir);

  public slots:
    void onEventLoopStarted();

  private slots:
    void onDetectionSuccess(ImageData *);
    void onDetectionFailure(ImageData *, std::exception *);
    void onReviewSuccess(ImageData *);
    void onReviewFailure(ImageData *, std::exception *);
    void onPostprocessSuccess(ImageData *);
    void onPostprocessFailure(ImageData *, std::exception *);

  private:
    int scan(QString);
    void enqueue();

    Viewer viewer;

    QDir inputDir;
    QDir outputDir = QDir::current();

    QThreadPool pool;
    QMutex queueLock;
    QList<ImageData *> toDetect;
    QList<ImageData *> toReview;
    QList<ImageData *> toPostprocess;
};
