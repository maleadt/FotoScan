// Sutherland-Hodgman polygon clipping
//
// http://rosettacode.org/wiki/Sutherland-Hodgman_polygon_clipping

#include "clip.hpp"

using namespace cv;
using namespace std;

// tells if Point c lies on the left side of directed edge a.b
// 1 if left, -1 if right, 0 if colinear
int left_of(Point a, Point b, Point c) {
    double x;
    auto tmp1 = b - a;
    auto tmp2 = c - b;
    x = tmp1.cross(tmp2);
    return x < 0 ? -1 : x > 0;
}

int line_sect(Point x0, Point x1, Point y0, Point y1, Point &res) {
    auto dx = x1 - x0;
    auto dy = y1 - y0;
    auto d = x0 - y0;
    // x0 + a dx = y0 + b dy .
    // x0 X dx = y0 X dx + b dy X dx .
    // b = (x0 - y0) X dx / (dy X dx)
    double dyx = dy.cross(dx);
    if (!dyx)
        return 0;
    dyx = d.cross(dx) / dyx;
    if (dyx <= 0 || dyx >= 1)
        return 0;

    res.x = y0.x + dyx * dy.x;
    res.y = y0.y + dyx * dy.y;
    return 1;
}

// this works only if all of the following are true:
//   1. poly has no colinear edges;
//   2. poly has no duplicate vertices;
//   3. poly has at least three vertices;
//   4. poly is convex (implying 3).
int poly_winding(vector<Point> p) { return left_of(p[0], p[1], p[3]); }

vector<Point> poly_edge_clip(vector<Point> sub, Point x0, Point x1, int left) {
    int i, side0, side1;
    Point v0 = sub[sub.size() - 1], v1;

    vector<Point> res;

    side0 = left_of(x0, x1, v0);
    if (side0 != -left)
        res.push_back(v0);

    for (i = 0; i < sub.size(); i++) {
        v1 = sub[i];
        side1 = left_of(x0, x1, v1);
        Point tmp;
        if (side0 + side1 == 0 && side0)
            // last point and current straddle the edge
            if (line_sect(x0, x1, v0, v1, tmp))
                res.push_back(tmp);
        if (i == sub.size() - 1)
            break;
        if (side1 != -left)
            res.push_back(v1);
        v0 = v1;
        side0 = side1;
    }

    return res;
}

vector<Point> poly_clip(vector<Point> sub, vector<Point> clip) {
    vector<Point> p1, p2;

    int dir = poly_winding(clip);
    p2 = poly_edge_clip(sub, clip[clip.size() - 1], clip[0], dir);
    for (int i = 0; i < clip.size() - 1; i++) {
        swap(p1, p2);
        if (p1.size() == 0) {
            p2.clear();
            break;
        }
        p2 = poly_edge_clip(p1, clip[i], clip[i + 1], dir);
    }

    return p2;
}
