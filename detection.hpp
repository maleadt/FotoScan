#pragma once

#include <QObject>
#include <QRunnable>
#include <QString>
#include <QImage>

#include <chrono>

struct DetectionData {
    QString file;
    QImage image;

    // NOTE: actually rects, but easier to represent as 4 points
    QList<QPolygon> rejects, ungrouped, pictures;

    std::chrono::milliseconds elapsed;
};

class DetectionTask : public QObject, public QRunnable {
    Q_OBJECT

  public:
    DetectionTask(QString file);
    ~DetectionTask();
    void run();

  signals:
    void finished(DetectionData *);

  private:
    DetectionData *data;
    bool success;
};
