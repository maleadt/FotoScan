#pragma once

#include <QApplication>
#include <QMutex>
#include <QThreadPool>
#include <QImage>
#include <QDir>
#include <QDateTime>

#include "viewer.hpp"

#include <chrono>

enum class ProgramMode { DEFAULT, CORRECT_RESULTS };

struct ScanData {
    QString file;
    QImage image;
    ScanData(const QString &file);
    void load();

    // result of detection
    // NOTE: `ungrouped` & `shapes` are actually rectangles,
    //       but are easier to represent as 4 point polygons
    QList<QPolygon> rejects, ungrouped, shapes;

    // result of post-processing
    QList<QImage> photos;

    std::chrono::milliseconds elapsed = std::chrono::milliseconds::zero();
};

class Scanner : public QApplication {
    Q_OBJECT

  public:
    Scanner(int &argc, char *argv[]);
    int scan();
    void setOutputDir(QString dir);
    void setInputDir(QString dir);
    void setMode(ProgramMode);

  public slots:
    void onEventLoopStarted();

  private slots:
    void onDetectionSuccess(ScanData *);
    void onDetectionFailure(ScanData *, std::exception *);
    void onReviewSuccess(ScanData *);
    void onReviewFailure(ScanData *, std::exception *);
    void onPostprocessSuccess(ScanData *);
    void onPostprocessFailure(ScanData *, std::exception *);

  private:
    int scan(QString);
    void enqueue();

    Viewer viewer;

    ProgramMode mode = ProgramMode::DEFAULT;

    QDir inputDir;
    QDir outputDir = QDir::current();

    QThreadPool pool;
    QMutex queueLock;
    QList<ScanData *> toDetect;
    QList<ScanData *> toReview;
    QList<ScanData *> toPostprocess;

    QDateTime start;
    size_t reviews;
};
