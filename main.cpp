#include <opencv2/core/core.hpp>
#include <opencv2/objdetect/objdetect.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>

#include <boost/program_options.hpp>
#include <boost/optional.hpp>
#define BOOST_FILESYSTEM_NO_DEPRECATED
#define BOOST_FILESYSTEM_VERSION 3
#include <boost/filesystem.hpp>

#include "clip.hpp"

#include <iostream>
#include <cmath>
#include <chrono>

#include <omp.h>

using namespace cv;
using namespace std;
using namespace boost;
namespace po = boost::program_options;
namespace fs = boost::filesystem;

int thresh = 50, N = 11;
const char *wndname = "Foto Scan";

typedef vector<Point> Shape;
typedef vector<Shape> ShapeList;

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
static ShapeList extractContours(const Mat &image) {
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
static ShapeList
filterSquares(const ShapeList &contours,
              optional<ShapeList &> rejects = optional<ShapeList &>()) {
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
                continue;   // truly-reject useless contours
            if (area < 500000 || area > 10000000)
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

bool cmp_pictures(const Shape &a, const Shape &b) {
    auto clip = poly_clip(a, b);
    if (clip.size() == 0)
        return false;

    auto a_a = contourArea(Mat(a));
    auto a_b = contourArea(Mat(b));
    auto a_clip = contourArea(Mat(clip));

    return a_clip / max(a_a, a_b) > 0.90;
}

// Partition squares based on the area of their intersection
static ShapeList minimizeSquares(const ShapeList &squares) {
    vector<int> labels;
    int groups = partition(squares, labels, cmp_pictures);
    ShapeList grouped_squares(groups);

    // reduce to groups, selecting the _smallest_ image
    // TODO: save alternatives, allow selection through GUI
    for (size_t i = 0; i < squares.size(); i++) {
        int group = labels[i];
        if (grouped_squares[group].size() == 0)
            grouped_squares[group] = squares[i];
        else if (contourArea(Mat(squares[i])) <
                 contourArea(Mat(grouped_squares[group])))
            grouped_squares[group] = squares[i];
    }

    return grouped_squares;
}

class InteractiveDisplay {
  public:
    InteractiveDisplay(const std::string &name, const Mat &image,
                       const ShapeList &squares,
                       const optional<ShapeList &> overlapping,
                       const optional<ShapeList &> rejected)
        : name(name), image(image), squares(squares), overlapping(overlapping),
          rejected(rejected) {
        setMouseCallback(name, onMouse, this);

        scale = 1;
        cv::Size s = image.size();
        x[0] = 0;
        y[0] = 0;
        x[1] = s.width;
        y[1] = s.height;
    }

    ~InteractiveDisplay() { setMouseCallback(name, nullptr, nullptr); }

    static void onMouse(int event, int x, int y, int, void *s) {
        InteractiveDisplay *self = (InteractiveDisplay *)s;

        switch (event) {
        case EVENT_LBUTTONDBLCLK: {
            self->zoom(x, y, 4);
            self->render();
            break;
        }
        case EVENT_RBUTTONDBLCLK: {
            self->zoom(x, y, 0.25);
            self->render();
            break;
        }
        default:
            break;
        }
    }

    void render() {
        cout << "Rendering from (" << x[0] << "," << y[0] << ") to (" << x[1]
             << "," << y[1] << ") from img[" << image.size().width << ","
             << image.size().height << "]" << endl;
        buffer = cv::Mat(image, cv::Rect(x[0], y[0], x[1] - x[0] - 1, y[1] - y[0] - 1));
        if (rejected)
            drawShapes(*rejected, Scalar(0, 0, 255));
        if (overlapping)
            drawShapes(*overlapping, Scalar(255, 0, 0));
        drawShapes(squares);

        imshow(name, buffer);
    }

    void zoom(int x_focus, int y_focus, float factor) {
        scale *= factor;
        if (scale < 1.0)
            scale = 1.0;

        cv::Size s = image.size();
        int x_range = s.width / scale;
        int y_range = s.height / scale;

        x[0] = x_focus - x_focus / scale;
        x[1] = x[0] + x_range;
        if (x[0] < 0) {
            x[1] += abs(x[0]);
            x[0] = 0;
        }
        if (x[1] > s.width) {
            x[0] -= (x[1] - s.width);
            x[1] = s.width;
        }
        assert(x[0] >= 0);
        assert(x[1] <= s.width);

        y[0] = y_focus - y_focus / scale;
        y[1] = y[0] + y_range;
        if (y[0] < 0) {
            y[1] += abs(y[0]);
            y[0] = 0;
        }
        if (y[1] > s.height) {
            y[0] -= (y[1] - s.height);
            y[1] = s.height;
        }
        assert(y[0] >= 0);
        assert(y[1] <= s.height);
    }

  private:
    std::string name;

    Mat buffer;

    float scale;
    int x[2];
    int y[2];

    const Mat &image;
    const ShapeList &squares;
    const optional<ShapeList &> overlapping;
    const optional<ShapeList &> rejected;

    void drawShapes(const ShapeList &squares,
                           const Scalar color = Scalar(0, 255, 0)) {
        for (size_t i = 0; i < squares.size(); i++) {
            const Point *p = &squares[i][0];
            int n = (int)squares[i].size();
            polylines(buffer, &p, &n, 1, true, color, 3);
        }
    }
};

int main(int argc, char **argv) {
    //
    // Options
    //

    // Declare named options
    po::options_description desc("Allowed options");
    bool show_rejected, show_overlapping, recursive;
    std::vector<std::string> inputs;
    desc.add_options()("help", "produce help message")(
        "show-rejected", po::bool_switch(&show_rejected),
        "show rejected contours")(
        "show-overlapping", po::bool_switch(&show_overlapping),
        "show overlapping squares")("recursive,r", po::bool_switch(&recursive),
                                    "process directories recursively")(
        "inputs,i", po::value<std::vector<std::string>>(&inputs)->required(),
        "images to process");

    // Declare positional options
    po::positional_options_description pod;
    pod.add("inputs", -1);

    // Parse
    po::variables_map vm;
    try {
        po::store(po::command_line_parser(argc, argv)
                      .options(desc)
                      .positional(pod)
                      .run(),
                  vm);
    } catch (po::error &e) {
        cerr << "Error: " << e.what() << endl << endl;
        cerr << desc << endl;
        return 1;
    }

    if (vm.count("help")) {
        cout << desc << "\n";
        return 0;
    }

    // Notify the user of errors
    try {
        notify(vm);
    } catch (const std::exception &e) {
        cerr << "Invalid usage: " << e.what() << endl;

        cout << desc << endl;
        return 1;
    }


    //
    // File discovery
    //

    std::vector<fs::path> files;
    std::vector<fs::path> directories;
    for (auto input : inputs) {
        fs::path p(input);
        if (!exists(p)) {
            cerr << "Error: input '" << p << "' does not exist" << endl;
            return 1;
        }
        if (is_regular_file(p))
            files.push_back(p);
        else if (is_directory(p))
            directories.push_back(p);
    }

    while (!directories.empty()) {
        auto dir = directories.back();
        directories.pop_back();

        fs::recursive_directory_iterator end;
        for (fs::recursive_directory_iterator i(dir); i != end; ++i) {
            const fs::path p = (*i);
            if (is_regular_file(p))
                files.push_back(p);
            else if (is_directory(p) && recursive)
                directories.push_back(p);
        }
    }


    //
    // Main processing
    //

    namedWindow(wndname, WINDOW_NORMAL);
    InteractiveDisplay *display = nullptr;

#if defined(_OPENMP)
    cout << "Processing with " << omp_get_max_threads() << " threads" << endl;
    // NOTE: we need nesting or each file would be processed on a single thread
    //       (disregarding the deeper parallelism)
    omp_set_nested(true);
#endif

    size_t i = 0;
    #pragma omp parallel
    {
        while (true) {
            size_t j;
            #pragma omp atomic capture
            { j = i; i++; }
            if (j >= files.size())
                break;
            auto file = files[j];

#if defined(_OPENMP)
            // HACK: give the first thread full control during the first cycle
            if (j < (unsigned)omp_get_num_threads() && j > 0)
                while (!display)
                    sleep(0.1);
#endif

            auto start = std::chrono::system_clock::now();

            Mat image = imread(file.string(), 1);
            if (image.empty()) {
                cout << "Couldn't load " << file << endl;
                continue;
            }

            auto contours = extractContours(image);

            ShapeList rejects;
            auto overlapping = filterSquares(
                contours, show_rejected ? optional<ShapeList &>(rejects)
                                        : optional<ShapeList &>());

            auto squares = minimizeSquares(overlapping);

            auto end = std::chrono::system_clock::now();
            auto elapsed =
                std::chrono::duration_cast<std::chrono::milliseconds>(end -
                                                                      start);

            #pragma omp critical(display)
            {
                display = new InteractiveDisplay(
                    wndname, image, squares,
                    show_overlapping ? optional<ShapeList &>(overlapping)
                                     : optional<ShapeList &>(),
                    show_rejected ? optional<ShapeList &>(rejects)
                                  : optional<ShapeList &>());
                display->render();

                cout << "- " << file << " (took " << elapsed.count()
                     << " ms to process)" << endl;
                cout.flush();

                cout << "Press RETURN to continue" << endl;
                int c = waitKey(0);
                if ((char)c == 27)
                    exit(0);

                delete display;
                display = nullptr;
            }
        }
    }

    return 0;
}
