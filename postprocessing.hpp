#pragma once

#include <QObject>
#include <QRunnable>
#include <QString>

class ImageData;

class PostprocessTask : public QObject, public QRunnable {
    Q_OBJECT

  public:
    PostprocessTask(ImageData *data);
    void run();

  signals:
    void success(ImageData *);
    void failure(ImageData *, std::exception *);

  private:
    ImageData *data;
};
