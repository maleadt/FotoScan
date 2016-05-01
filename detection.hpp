#pragma once

#include <QObject>
#include <QRunnable>
#include <QString>
#include <QImage>

#include <chrono>

struct DetectionData {
    QString file;
    QImage image;
    DetectionData(const QString &file);
    void load();

    // NOTE: actually rects, but easier to represent as 4 points
    QList<QPolygon> rejects, ungrouped, pictures;

    std::chrono::milliseconds elapsed = std::chrono::milliseconds::zero();
};

class DetectionTask : public QObject, public QRunnable {
    Q_OBJECT

  public:
    DetectionTask(QString file);
    void run();

  signals:
    void success(DetectionData *);
    void failure(DetectionData *, std::exception *);

  private:
    DetectionData *data;
};
