#pragma once

#include <QObject>
#include <QRunnable>

struct ScanData;

class PostprocessTask : public QObject, public QRunnable {
    Q_OBJECT

  public:
    PostprocessTask(ScanData *data);
    void run();

  signals:
    void success(ScanData *);
    void failure(ScanData *, std::exception *);

  private:
    ScanData *data;
};
