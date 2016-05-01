#pragma once

#include <QApplication>
#include <QMutex>

#include "detection.hpp"
#include "viewer.hpp"

class Scanner : public QApplication {
    Q_OBJECT

  public:
    Scanner(int &argc, char *argv[]);
    int scan(QString);

  public slots:
    void onEventLoopStarted();

  private slots:
    void onDetectionSuccess(DetectionData *);
    void onDetectionFailure(DetectionData *, std::exception *);
    void onReviewFinished(DetectionData *);

  private:
    void enqueueDetection();
    void enqueueReview();

    Viewer viewer;

    QMutex queueLock;
    QList<QString> toDetect;
    QList<DetectionData *> toReview;
};
