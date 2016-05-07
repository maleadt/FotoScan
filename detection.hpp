#pragma once

#include <QObject>
#include <QRunnable>
#include <QString>

struct ScanData;

class DetectionTask : public QObject, public QRunnable {
    Q_OBJECT

  public:
    DetectionTask(ScanData *);
    void run();

  signals:
    void success(ScanData *);
    void failure(ScanData *, std::exception *);

  private:
    ScanData *data;
};
