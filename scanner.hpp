#pragma once

#include <QApplication>
#include <QMutex>
#include <QThreadPool>
#include <QImage>

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
    int scan(QString);

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
    void enqueue();

    Viewer viewer;

    QThreadPool pool;
    QMutex queueLock;
    QList<ImageData *> toDetect;
    QList<ImageData *> toReview;
    QList<ImageData *> toPostprocess;
};
