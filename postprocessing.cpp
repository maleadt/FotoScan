#include "postprocessing.hpp"

#include <QDebug>

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include "opencv2/objdetect/objdetect.hpp"

#include <chrono>

#include "scanner.hpp"

using namespace cv;
using namespace std;


//
// Auxiliary
//

static bool isClockwise(const QPolygon &polygon) {
    double sum = 0.0;
    for (int i = 0; i < polygon.size(); i++) {
        QPoint v1 = polygon[i];
        QPoint v2 = polygon[(i + 1) % polygon.size()];
        sum += (v2.x() - v1.x()) * (v2.y() + v1.y());
    }
    return sum < 0.0;
}


//
// Detection
//

void extractPictures(ImageData *data) {
    // convert to OpenCV format
    Mat mat;
    if (data->image.format() == QImage::Format_RGB32)
        mat = Mat(data->image.height(), data->image.width(), CV_8UC4,
                  data->image.bits(), data->image.bytesPerLine());
    else {
        throw new runtime_error("Could not convert Qt image to OpenCV");
    }

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

        const auto qt_output =
            QImage((uchar *)sub_warped.data, sub_warped.cols, sub_warped.rows,
                   sub_warped.step, data->image.format());
        #pragma omp critical
        data->images << qt_output.copy();
    }
}

enum class Orientation {
    Unknown = -1,

    Correct = 0,
    Clockwise = 1, // image is rotated to the right
    Flipped = 2,
    Counterclockwise = 3 // image is rotated to the left
};

QDebug operator<<(QDebug d, const Orientation &orientation) {
    switch (orientation) {
    case Orientation::Unknown:
        d << "Unknown";
        break;
    case Orientation::Correct:
        d << "Correct";
        break;
    case Orientation::Clockwise:
        d << "Clockwise";
        break;
    case Orientation::Flipped:
        d << "Flipped";
        break;
    case Orientation::Counterclockwise:
        d << "Counter-clockwise";
        break;
    }
    return d;
}

Orientation clear_winner(const unsigned int(&votes)[4]) {
    int winner = -1;
    int runner_up;

    for (int i = 0; i < 4; i++) {
        if (winner == -1) {
            if (votes[i] > 0) {
                runner_up = winner;
                winner = i;
            }
        } else {
            if (votes[i] >= votes[winner]) {
                runner_up = winner;
                winner = i;
            }
        }
    }

    if (winner == -1)
        return Orientation::Unknown;
    else if (runner_up == -1)
        return static_cast<Orientation>(winner);
    else
        return votes[winner] > votes[runner_up]
                   ? static_cast<Orientation>(winner)
                   : Orientation::Unknown;
}

Mat correctOrientation(Mat image, Orientation orientation) {
    Mat rotated, temp;
    switch (orientation) {
    case Orientation::Clockwise:
        // rotate counter-clockwise
        transpose(image, temp);
        flip(temp, rotated, 0);
        break;
    case Orientation::Flipped:
        flip(image, temp, 0);
        flip(temp, rotated, 1);
        break;
    case Orientation::Counterclockwise:
        // rotate clockwise
        transpose(image, temp);
        flip(temp, rotated, 1);
        break;
    default:
        return image;
    }

    return rotated;
}

// Detect features across all rotations, adding the amount of features detected
// per orientation into a reference-passed array.
void detectFeatures(const Mat &image, const FileStorage &fs,
                    unsigned int(&votes)[4]) {
    #pragma omp parallel for
    for (int orientation = 0; orientation < 4; orientation++) {
        Mat rotated =
            correctOrientation(image, static_cast<Orientation>(orientation));

        // FIXME: do without re-constructing the classifier (opencv #4287)
        CascadeClassifier classifier;
        if (!classifier.read(fs.getFirstTopLevelNode()))
            throw new runtime_error("could not create cascade classifier");

        vector<Rect> faces;
        classifier.detectMultiScale(rotated, faces,
                                    /*scale_factor=*/1.2,
                                    /*minNeighbors=*/2,
                                    /*flags=*/CASCADE_DO_CANNY_PRUNING);

        #pragma omp atomic
        votes[orientation] += faces.size();
    }
}

// Deduce the orientation from the sky by looking at border segments and
// preferring the brightest one to be at the top.
Orientation detectSky(const Mat &image) {
    const int scale = 4;
    Size newsize(round(image.cols / scale), round(image.rows / scale));
    Mat scaled;
    resize(image, scaled, newsize, INTER_LINEAR);

    // take the top 1/$part, right 1/$part, etc
    // NOTE: the index into this array corresponds with the Orientation enum
    //       were this border the brightest
    const int part = 5;
    Mat borders[4] = {/*top*/ scaled.rowRange(0, scaled.rows / part),
                      /*right*/ scaled.colRange(
                          scaled.cols / part * (part - 1), scaled.cols),
                      /*bottom*/ scaled.rowRange(
                          scaled.rows / part * (part - 1), scaled.rows),
                      /*left*/ scaled.colRange(0, scaled.cols / part)};

    Scalar brightness[4];
    #pragma omp parallel for
    for (int i = 0; i < 4; i++)
        brightness[i] = mean(borders[i]);

    // pick best orientation
    assert(image.channels() == 1);
    int winner = 0;
    for (int orientation = 1; orientation < 4; orientation++) {
        if (brightness[orientation].val[0] > brightness[winner].val[0])
            winner = orientation;
    }
    return static_cast<Orientation>(winner);
}

// Detect and correct the orientation of all images
void correctOrientation(ImageData *data) {
    const auto cascades = {
        // listed in order most likely to appear in a photo
        "/usr/share/opencv/haarcascades/haarcascade_frontalface_alt.xml",
        "/usr/share/opencv/haarcascades/haarcascade_profileface.xml",
        "/usr/share/opencv/haarcascades/haarcascade_fullbody.xml"};

    for (int i = 0; i < data->images.size(); ++i) {
        auto image = data->images[i];

        // convert to OpenCV format
        Mat mat;
        if (image.format() == QImage::Format_RGB32)
            mat = Mat(image.height(), image.width(), CV_8UC4, image.bits(),
                      image.bytesPerLine());
        else
            throw new runtime_error("Could not convert Qt image to OpenCV");
        Mat grayscale;
        cvtColor(mat, grayscale, COLOR_BGR2GRAY);

        // detect orientation from features
        unsigned int votes[4] = {0, 0, 0, 0};
        Orientation winner;
        for (auto cascade : cascades) {
            FileStorage fs(cascade, FileStorage::READ);
            if (!fs.isOpened())
                throw new runtime_error(
                    "Could not load cascade classifier file");

            // Try 4 different sizes of our photo
            for (int image_scale = 4; image_scale > 0; image_scale--) {
                auto newsize = Size(round(grayscale.cols / image_scale),
                                    round(grayscale.rows / image_scale));
                Mat scaled;
                resize(grayscale, scaled, newsize, INTER_LINEAR);
                detectFeatures(scaled, fs, votes);
                if ((winner = clear_winner(votes)) != Orientation::Unknown)
                    goto done;
            }
        }
    done:

        // detect orientation from sky, if necessary
        if (winner == Orientation::Unknown)
            winner = detectSky(grayscale);

        // apply orientation
        if (winner != Orientation::Unknown) {
            Mat rotated = correctOrientation(mat, winner);
            QImage qt_rotated((uchar *)rotated.data, rotated.cols, rotated.rows,
                              rotated.step, QImage::Format_RGB32);
            data->images[i] = qt_rotated.copy();
        }
    }
}


//
// PostprocessTask
//

PostprocessTask::PostprocessTask(ImageData *data) : data(data) {}

void PostprocessTask::run() {
    // Lazy-load image data
    try {
        data->load();
    } catch (exception *ex) {
        emit failure(data, ex);
        return;
    }

    auto start = chrono::system_clock::now();

    try {
        extractPictures(data);
        correctOrientation(data);
    } catch (runtime_error *ex) {
        emit failure(data, ex);
        return;
    }

    auto end = chrono::system_clock::now();
    data->elapsed += chrono::duration_cast<chrono::milliseconds>(end - start);

    emit success(data);
}