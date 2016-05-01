#include "postprocessing.hpp"

#include <QDebug>

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include <chrono>

#include "scanner.hpp"

using namespace cv;
using namespace std;


//
// PostprocessTask
//

PostprocessTask::PostprocessTask(ImageData *data) : data(data) {}

static bool isClockwise(const QPolygon &polygon) {
    double sum = 0.0;
    for (int i = 0; i < polygon.size(); i++) {
        QPoint v1 = polygon[i];
        QPoint v2 = polygon[(i + 1) % polygon.size()];
        sum += (v2.x() - v1.x()) * (v2.y() + v1.y());
    }
    return sum < 0.0;
}

void PostprocessTask::run() {
    try {
        data->load();
    } catch (exception *ex) {
        emit failure(data, ex);
        return;
    }

    Mat mat;
    if (data->image.format() == QImage::Format_RGB32)
        mat = Mat(data->image.height(), data->image.width(), CV_8UC4,
                  data->image.bits(), data->image.bytesPerLine());
    else
        mat = imread(data->file.toStdString(), 1);

    auto start = chrono::system_clock::now();

    #pragma omp parallel for
    for (int index = 0; index < data->pictures.size(); ++index) {
        auto picture = data->pictures[index];
        assert(picture.size() == 4);

        auto bbox = picture.boundingRect();
        auto cv_bbox = Rect(bbox.x(), bbox.y(), bbox.width(), bbox.height());

        // make sure the polygon is oriented clockwise
        if (!isClockwise(picture))
            reverse(picture.begin(), picture.end());

        // move the top-left point to the start of the polygon
        int topleft = 0;
        int topleft_distance = numeric_limits<int>::max();
        for (int i = 0; i < picture.size(); i++) {
            float distance = (bbox.topLeft() - picture[i]).manhattanLength();
            if (distance < topleft_distance) {
                topleft = i;
                topleft_distance = distance;
            }
        }
        rotate(picture.begin(), picture.begin() + topleft, picture.end());

        // extract the picture's submatrix
        const Mat sub_mat = mat(cv_bbox);
        QPolygon sub_polygon = QPolygon(bbox);
        QPoint sub_offset = bbox.topLeft();

        // create the transformation matrix
        Point2f srcTri[4];
        for (int i = 0; i < picture.size(); i++)
            srcTri[i] = Point(picture[i].x() - sub_offset.x(),
                              picture[i].y() - sub_offset.y());
        Point2f dstTri[4];
        for (int i = 0; i < sub_polygon.size(); i++)
            dstTri[i] = Point(sub_polygon[i].x() - sub_offset.x(),
                              sub_polygon[i].y() - sub_offset.y());
        auto warp_mat = getPerspectiveTransform(srcTri, dstTri);

        // perform the actual transform
        Mat sub_warped = Mat::zeros(sub_mat.rows, sub_mat.cols, sub_mat.type());
        warpPerspective(sub_mat, sub_warped, warp_mat, sub_warped.size());

        Mat output(sub_warped.cols, sub_warped.rows, sub_warped.type());
        cvtColor(sub_warped, output, CV_BGR2RGB);
        #pragma omp critical
        data->images << QImage((uchar *)output.data, output.cols, output.rows,
                               output.step, QImage::Format_RGB888);
    }

    auto end = chrono::system_clock::now();
    data->elapsed += chrono::duration_cast<chrono::milliseconds>(end - start);

    emit success(data);
}
