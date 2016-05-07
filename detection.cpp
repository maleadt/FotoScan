#include "detection.hpp"

#include <QFileInfo>
#include <QDebug>

#include <opencv2/core/core.hpp>
#include <opencv2/objdetect/objdetect.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include <boost/optional.hpp>

#include <iostream>
#include <cmath>
#include <chrono>

#include "clip.hpp"
#include "scanner.hpp"

using namespace cv;
using namespace std;
using namespace boost;

typedef vector<Point> Shape;
typedef vector<Shape> ShapeList;


//
// Detection functionality
//

// Finds a cosine of angle between vectors from pt0->pt1 and from pt0->pt2
static double angle(Point pt1, Point pt2, Point pt0) {
    double dx1 = pt1.x - pt0.x;
    double dy1 = pt1.y - pt0.y;
    double dx2 = pt2.x - pt0.x;
    double dy2 = pt2.y - pt0.y;
    return (dx1 * dx2 + dy1 * dy2) /
           sqrt((dx1 * dx1 + dy1 * dy1) * (dx2 * dx2 + dy2 * dy2) + 1e-10);
}

// Extract a sequence of contours detected in the image.
int thresh = 50, N = 11;
ShapeList extractContours(const Mat &image) {
    // down-scale and upscale the image to filter out the noise
    Mat pyr, timg;
    pyrDown(image, pyr, Size(image.cols / 2, image.rows / 2));
    pyrUp(pyr, timg, image.size());

    // find squares in every color plane of the image
    ShapeList all_contours;
    #pragma omp parallel for
    for (int c = 0; c < 3; c++) {
        Mat gray0(image.size(), CV_8U);

        int ch[] = {c, 0};
        mixChannels(&timg, 1, &gray0, 1, ch, 1);

        // try several threshold levels
        #pragma omp parallel for
        for (int l = 0; l < N; l++) {
            Mat gray;
            // hack: use Canny instead of zero threshold level.
            // Canny helps to catch squares with gradient shading
            if (l == 0) {
                // apply Canny. Take the upper threshold from slider
                // and set the lower to 0 (which forces edges merging)
                Canny(gray0, gray, 0, thresh, 5);
                // dilate canny output to remove potential
                // holes between edge segments
                dilate(gray, gray, Mat(), Point(-1, -1));
            } else {
                // apply threshold if l!=0:
                //     tgray(x,y) = gray(x,y) < (l+1)*255/N ? 255 : 0
                gray = gray0 >= (l + 1) * 255 / N;
            }

            // find contours and store them all as a list
            ShapeList contours;
            findContours(gray, contours, RETR_LIST, CHAIN_APPROX_SIMPLE);

            #pragma omp critical(all_contours)
            all_contours.insert(all_contours.end(), contours.begin(),
                                contours.end());
        }
    }

    return all_contours;
}

// Filter the squares from a list of contours
ShapeList filterShapes(const ShapeList &contours,
                        optional<ShapeList &> rejects) {
    ShapeList accepts;

    // test each contour
    #pragma omp parallel for
    for (size_t i = 0; i < contours.size(); i++) {
        // approximate contour with accuracy proportional
        // to the contour perimeter
        Shape approx;
        approxPolyDP(Mat(contours[i]), approx,
                     arcLength(Mat(contours[i]), true) * 0.02, true);

        // square contours should have 4 vertices after approximation
        // and be convex.
        if (approx.size() == 4 && isContourConvex(Mat(approx))) {
            double maxCosine = 0;

            // Filter on area
            // Note: absolute value of an area is used because
            // area may be positive or negative - in accordance with the
            // contour orientation
            auto area = fabs(contourArea(Mat(approx)));
            if (area < 1000)
                continue; // truly-reject useless contours
            if (area < 500000 || area > 20000000)
                goto reject;

            for (int j = 2; j < 5; j++) {
                // find the maximum cosine of the angle between joint
                // edges
                double cosine =
                    fabs(angle(approx[j % 4], approx[j - 2], approx[j - 1]));
                maxCosine = max(maxCosine, cosine);
            }

            // if cosines of all angles are small (angles should be ~90
            // degrees)
            if (maxCosine > 0.10)
                goto reject;

            #pragma omp critical(accepts)
            accepts.push_back(approx);
            continue;

        reject:
            if (rejects) {
                #pragma omp critical(rejects)
                (*rejects).push_back(approx);
            }
        }
    }

    return accepts;
}

// Comparison function for share partitioning, true if the intersection of
// two shapes occupies 90% or more of the largest shape
bool cmp_shape(const Shape &a, const Shape &b) {
    auto clip = poly_clip(a, b);
    if (clip.size() == 0)
        return false;

    auto a_a = contourArea(Mat(a));
    auto a_b = contourArea(Mat(b));
    auto a_clip = contourArea(Mat(clip));

    return a_clip / max(a_a, a_b) > 0.90;
}

// Minimize the amount of shapes by partitioning based on the area of overlap
// and selecting the square with the straightest corners
ShapeList minimizeShapes(const ShapeList &shapes) {
    // partition shapes according to the intersection area
    vector<int> labels;
    int groups = partition(shapes, labels, cmp_shape);

    // for each group, select shape with straightest corners
    ShapeList grouped_squares(groups);
    vector<float> minCosines(groups);
    for (size_t i = 0; i < shapes.size(); i++) {
        int group = labels[i];

        // find the minimum cosine of the angle between joint edges
        double minCosine = std::numeric_limits<double>::max();
        for (int j = 2; j < 5; j++) {
            double cosine = fabs(
                angle(shapes[i][j % 4], shapes[i][j - 2], shapes[i][j - 1]));
            minCosine = min(minCosine, cosine);
        }

        if (grouped_squares[group].size() == 0 ||
            minCosine < minCosines[group]) {
            grouped_squares[group] = shapes[i];
            minCosines[group] = minCosine;
        }
    }

    return grouped_squares;
}


//
// Auxiliary conversions (OpenCV to Qt)
//

static QPolygon toPolygon(Shape shape) {
    QPolygon polygon;
    for (auto point : shape)
        polygon << QPoint(point.x, point.y);
    return polygon;
}

static QList<QPolygon> toPolygonList(ShapeList shapes) {
    QList<QPolygon> polygons;
    for (auto shape : shapes)
        polygons << toPolygon(shape);
    return polygons;
}

static QRect toRect(Shape shape) {
    assert(shape.size() == 4);
    return QRect(QPoint(shape[0].x, shape[0].y),
                 QPoint(shape[2].x, shape[2].y));
}

static QList<QRect> toRectList(ShapeList shapes) {
    QList<QRect> rects;
    for (auto shape : shapes)
        rects << toRect(shape);
    return rects;
}


//
// DetectionTask
//

DetectionTask::DetectionTask(ScanData *data) : data(data) {}

void DetectionTask::run() {
    // Lazy-load image data
    try {
        data->load();
    } catch (std::exception *ex) {
        emit failure(data, ex);
        return;
    }

    // Convert to OpenCV format
    Mat mat;
    if (data->image.format() == QImage::Format_RGB32)
        mat = Mat(data->image.height(), data->image.width(), CV_8UC4,
                  data->image.bits(), data->image.bytesPerLine());
    else {
        emit failure(data,
                     new runtime_error("Could not convert Qt image to OpenCV"));
        return;
    }

    auto start = chrono::system_clock::now();

    ShapeList cv_contours = extractContours(mat);

    ShapeList cv_rejects;
    ShapeList cv_ungrouped =
        filterShapes(cv_contours, optional<ShapeList &>(cv_rejects));

    ShapeList cv_shapes = minimizeShapes(cv_ungrouped);

    auto end = chrono::system_clock::now();
    data->elapsed += chrono::duration_cast<chrono::milliseconds>(end - start);

    data->rejects = toPolygonList(cv_rejects);
    data->ungrouped = toPolygonList(cv_ungrouped);
    data->shapes = toPolygonList(cv_shapes);

    emit success(data);
}