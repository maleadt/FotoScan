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

void extractPhotos(ScanData *data) {
    // convert to OpenCV format
    Mat mat;
    if (data->image.format() == QImage::Format_RGB32)
        mat = Mat(data->image.height(), data->image.width(), CV_8UC4,
                  data->image.bits(), data->image.bytesPerLine());
    else {
        throw new runtime_error("Could not convert Qt image to OpenCV");
    }

    #pragma omp parallel for
    for (int index = 0; index < data->shapes.size(); ++index) {
        auto shape = data->shapes[index];
        assert(shape.size() == 4);

        // start working with the bounding box -- this can be significantly
        // larger than the photo itself, eg. when it is rotated
        auto bbox = shape.boundingRect() & data->image.rect();

        // make sure the polygon is oriented clockwise
        if (!isClockwise(shape))
            reverse(shape.begin(), shape.end());

        // move the top-left point to the start of the polygon
        int topleft = 0;
        int topleft_distance = numeric_limits<int>::max();
        for (int i = 0; i < shape.size(); i++) {
            float distance = (bbox.topLeft() - shape[i]).manhattanLength();
            if (distance < topleft_distance) {
                topleft = i;
                topleft_distance = distance;
            }
        }
        rotate(shape.begin(), shape.begin() + topleft, shape.end());

        // extract the image data (coarsely, given its bounding box)
        const Mat submat_coarse =
            mat(Rect(bbox.x(), bbox.y(), bbox.width(), bbox.height()));
        QPoint submat_offset = bbox.topLeft();

        // figure out a destination rectangle to warp the image to
        // NOTE: can't just use the bounding box, because its aspect ratio can
        //       be completely different when dealing with a rotated image
        double width =
            (hypot(shape[0].x() - shape[1].x(), shape[0].y() - shape[1].y()) +
             hypot(shape[2].x() - shape[3].x(), shape[2].y() - shape[3].y())) /
            2.;
        double height =
            (hypot(shape[1].x() - shape[2].x(), shape[1].y() - shape[2].y()) +
             hypot(shape[3].x() - shape[0].x(), shape[3].y() - shape[0].y())) /
            2.;
        QRect dest(0, 0, width, height);

        // create the transformation matrix
        Point2f srcPts[4];
        for (int i = 0; i < shape.size(); i++)
            srcPts[i] = Point(shape[i].x() - submat_offset.x(),
                              shape[i].y() - submat_offset.y());
        Point2f dstPts[4];
        for (int i = 0; i < 4; i++)
            dstPts[i] = Point(QPolygon(dest)[i].x(), QPolygon(dest)[i].y());
        auto warp_mat = getPerspectiveTransform(srcPts, dstPts);

        // perform the actual transform
        Mat sub_warped = Mat::zeros(submat_coarse.rows, submat_coarse.cols,
                                    submat_coarse.type());
        warpPerspective(submat_coarse, sub_warped, warp_mat, sub_warped.size());

        // extract the image data (exactly now, which is only possible after
        // having warped the image)
        const Mat submat_fine =
            sub_warped(Rect(dest.x(), dest.y(), dest.width(), dest.height()));

        const auto qt_output =
            QImage((uchar *)submat_fine.data, submat_fine.cols,
                   submat_fine.rows, submat_fine.step, data->image.format());
        #pragma omp critical
        data->photos << qt_output.copy();
    }
}

enum class Orientation {
    Unknown = -1,

    Correct = 0,
    Clockwise = 1, // photo is rotated to the right
    Flipped = 2,
    Counterclockwise = 3 // photo is rotated to the left
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

// Detect and correct the orientation of all photos
void correctOrientation(ScanData *data) {
    QDir appdir = QDir(QCoreApplication::applicationDirPath());
    const auto cascades = {
        // listed in order most likely to appear in a photo
        // (processing bails out as soon as an orientation has been found)
        //// Arch Linux paths
        QFileInfo("/usr/share/opencv/haarcascades/haarcascade_frontalface_alt.xml"),
        QFileInfo("/usr/share/opencv/haarcascades/haarcascade_profileface.xml"),
        QFileInfo("/usr/share/opencv/haarcascades/haarcascade_fullbody.xml"),
        //// Shipped files
        QFileInfo(appdir, "haarcascades/haarcascade_frontalface_alt.xml"),
        QFileInfo(appdir, "haarcascades/haarcascade_profileface.xml"),
        QFileInfo(appdir, "haarcascades/haarcascade_fullbody.xml"),
        };

    unsigned int page_votes[4] = {0, 0, 0, 0};
    QVector<Orientation> orientations(data->photos.size());

    // detect orientation of individual photos
    for (int i = 0; i < data->photos.size(); ++i) {
        auto photo = data->photos[i];

        // convert to OpenCV format
        Mat mat;
        if (photo.format() == QImage::Format_RGB32)
            mat = Mat(photo.height(), photo.width(), CV_8UC4, photo.bits(),
                      photo.bytesPerLine());
        else
            throw new runtime_error("Could not convert Qt image to OpenCV");
        Mat grayscale;
        cvtColor(mat, grayscale, COLOR_BGR2GRAY);

        // detect orientation from features
        unsigned int votes[4] = {0, 0, 0, 0};
        Orientation winner;
        for (auto cascade : cascades) {
            FileStorage fs(cascade.absoluteFilePath().toLatin1().data(), FileStorage::READ);
            if (!fs.isOpened())
                continue;

            // try at different scales to save on processing power
            for (int scale = 4; scale > 0; scale--) {
                auto newsize = Size(round(grayscale.cols / scale),
                                    round(grayscale.rows / scale));
                Mat scaled;
                resize(grayscale, scaled, newsize, INTER_LINEAR);

                detectFeatures(scaled, fs, votes);

                // bail out if we have a clear winner
                if ((winner = clear_winner(votes)) != Orientation::Unknown)
                    goto done;
            }
        }
        throw new runtime_error("Could not load any cascade classifier file");

    done:

        // detect orientation from sky, if necessary
        if (winner == Orientation::Unknown)
            winner = detectSky(grayscale);

        assert(winner != Orientation::Unknown);
        page_votes[static_cast<int>(winner)]++;
        orientations[i] = winner;
    }

    // check if entire page seems rotated identically
    Orientation page_winner;
    if ((page_winner = clear_winner(page_votes)) != Orientation::Unknown) {
        // if so, force this orientation
        for (int i = 0; i < data->photos.size(); ++i)
            orientations[i] = page_winner;
    }

    // apply orientations
    for (int i = 0; i < data->photos.size(); ++i) {
        // TODO: rotate QImage directly?
        auto photo = data->photos[i];
        Mat mat(photo.height(), photo.width(), CV_8UC4, photo.bits(),
                photo.bytesPerLine());

        Mat rotated = correctOrientation(mat, orientations[i]);
        QImage qt_rotated((uchar *)rotated.data, rotated.cols, rotated.rows,
                          rotated.step, QImage::Format_RGB32);
        data->photos[i] = qt_rotated.copy();
    }
}


//
// PostprocessTask
//

PostprocessTask::PostprocessTask(ScanData *data) : data(data) {}

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
        extractPhotos(data);
        correctOrientation(data);
    } catch (runtime_error *ex) {
        emit failure(data, ex);
        return;
    }

    auto end = chrono::system_clock::now();
    data->elapsed += chrono::duration_cast<chrono::milliseconds>(end - start);

    emit success(data);
}