#include <opencv2/core/core.hpp>
#include <opencv2/objdetect/objdetect.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>

#include "clip.hpp"

#include <iostream>
#include <cmath>
#include <chrono>

using namespace cv;
using namespace std;

int thresh = 50, N = 11;
const char *wndname = "Picture Detection";

typedef vector<Point> Shape;
typedef vector<Shape> ShapeList;

// helper function:
// finds a cosine of angle between vectors
// from pt0->pt1 and from pt0->pt2
static double angle(Point pt1, Point pt2, Point pt0) {
    double dx1 = pt1.x - pt0.x;
    double dy1 = pt1.y - pt0.y;
    double dx2 = pt2.x - pt0.x;
    double dy2 = pt2.y - pt0.y;
    return (dx1 * dx2 + dy1 * dy2) /
           sqrt((dx1 * dx1 + dy1 * dy1) * (dx2 * dx2 + dy2 * dy2) + 1e-10);
}

bool cmp_pictures(const Shape &a, const Shape &b) {
    auto clip = poly_clip(a, b);
    if (clip.size() == 0)
        return false;

    auto a_a = contourArea(Mat(a));
    auto a_b = contourArea(Mat(b));
    auto a_clip = contourArea(Mat(clip));

    return a_clip / MAX(a_a, a_b) > 0.95;
}

// returns sequence of contours detected in the image.
static ShapeList findContours(const Mat &image) {
    Mat pyr, timg, gray0(image.size(), CV_8U), gray;

    // down-scale and upscale the image to filter out the noise
    pyrDown(image, pyr, Size(image.cols / 2, image.rows / 2));
    pyrUp(pyr, timg, image.size());

    // find squares in every color plane of the image
    ShapeList all_contours;
    for (int c = 0; c < 3; c++) {
        int ch[] = {c, 0};
        mixChannels(&timg, 1, &gray0, 1, ch, 1);

        // try several threshold levels
        for (int l = 0; l < N; l++) {
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

            all_contours.insert(all_contours.end(), contours.begin(),
                                contours.end());
        }
    }

    return all_contours;
}

// filter the squares from a list of contours
static ShapeList filterSquares(const ShapeList &contours) {
    ShapeList squares;

    // test each contour
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
            if (area < 500000 || area > 10000000)
                continue;

            for (int j = 2; j < 5; j++) {
                // find the maximum cosine of the angle between joint
                // edges
                double cosine =
                    fabs(angle(approx[j % 4], approx[j - 2], approx[j - 1]));
                maxCosine = MAX(maxCosine, cosine);
            }

            // if cosines of all angles are small (angles should be ~90
            // degrees)
            if (maxCosine > 0.05)
                continue;

            squares.push_back(approx);
        }
    }

    return squares;
}

static ShapeList minimizeSquares(const ShapeList &squares) {
    // partition squares based on the area of their intersection
    vector<int> labels;
    int groups = partition(squares, labels, cmp_pictures);
    ShapeList grouped_squares(groups);

    // reduce to groups, selecting the _smallest_ image
    // TODO: save alternatives, allow selection through GUI
    for (int i = 0; i < squares.size(); i++) {
        int group = labels[i];
        if (grouped_squares[group].size() == 0)
            grouped_squares[group] = squares[i];
        else if (contourArea(Mat(squares[i])) <
                 contourArea(Mat(grouped_squares[group])))
            grouped_squares[group] = squares[i];
    }

    return grouped_squares;
}

// the function draws all the squares in the image
static void drawSquares(Mat &image, const ShapeList &squares) {
    for (size_t i = 0; i < squares.size(); i++) {
        const Point *p = &squares[i][0];
        int n = (int)squares[i].size();
        polylines(image, &p, &n, 1, true, Scalar(0, 255, 0), 3);
    }

    imshow(wndname, image);
}

int main(int argc, char **argv) {
    static const char *names[] = {"../fotos/boek1/front/DSC_1986.JPG", 0};
    namedWindow(wndname, WINDOW_NORMAL);

    for (int i = 0; names[i] != 0; i++) {
        cout << "Processing " << names[i] << "...";
        cout.flush();
        auto start = std::chrono::system_clock::now();

        Mat image = imread(names[i], 1);
        if (image.empty()) {
            cout << "Couldn't load " << names[i] << endl;
            continue;
        }

        auto contours = findContours(image);
        auto squares = filterSquares(contours);
        auto minsquares = minimizeSquares(squares);

        auto end = std::chrono::system_clock::now();
        auto elapsed =
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << " done after " << elapsed.count() << " ms!" << endl;

        drawSquares(image, minsquares);

        cout << "Press RETURN to continue" << endl;
        int c = waitKey(0);
        if ((char)c == 27)
            break;
    }

    return 0;
}
