#include "postprocessing.hpp"

#include <QDebug>

#include "scanner.hpp"


//
// PostprocessTask
//

PostprocessTask::PostprocessTask(ImageData *data) : data(data) {}

void PostprocessTask::run() {
    try {
        data->load();
    } catch (std::exception *ex) {
        emit failure(data, ex);
        return;
    }

    qDebug() << "postprocessing" << data->file;

    // TODO

    emit success(data);
}
