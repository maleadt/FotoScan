#pragma once

#include <QObject>
#include <QRunnable>
#include <QString>

struct ImageData;

class DetectionTask : public QObject, public QRunnable {
    Q_OBJECT

  public:
    DetectionTask(ImageData *);
    void run();

  signals:
    void success(ImageData *);
    void failure(ImageData *, std::exception *);

  private:
    ImageData *data;
};
