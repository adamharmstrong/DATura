#include "zone_coverage.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace
{
using namespace ZoneCoverage;
int failures = 0;
int checks = 0;

void Check(bool condition, const std::string& message)
{
    ++checks;
    if (!condition)
    {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

Triangle Tri(Point a, Point b, Point c, Layer layer = Layer::Render)
{
    return {{a, b, c}, layer};
}

void Rectangle(std::vector<Triangle>& triangles, double x0, double z0,
               double x1, double z1, Layer layer, double y = 0.0)
{
    triangles.push_back(Tri({x0,y,z0}, {x1,y,z0}, {x1,y,z1}, layer));
    triangles.push_back(Tri({x0,y,z0}, {x1,y,z1}, {x0,y,z1}, layer));
}

Options UnitGrid()
{
    Options options;
    options.cellSize = 1.0;
    return options;
}

bool Has(const Result& result, std::size_t x, std::size_t z, Layer layer)
{
    return x < result.width && z < result.height && !result.cells.empty() &&
        (result.cells[z * result.width + x] & static_cast<std::uint8_t>(layer)) != 0;
}

void TestAnalyticTriangles()
{
    const Result result = Build({Tri({0,0,0}, {4,0,0}, {0,0,4})}, UnitGrid());
    Check(result.success && result.width == 4 && result.height == 4, "right triangle has analytic common bounds");
    for (std::size_t z = 0; z < 4; ++z)
        for (std::size_t x = 0; x < 4; ++x)
            Check(Has(result, x, z, Layer::Render) == (x + z <= 4),
                  "right triangle covers precisely the cells intersecting x+z<=4");
    Check(result.render.accepted == 1 && result.render.coveredCells == 13,
          "right triangle includes interiors and edge contacts, not only three vertices");

    const Result reversed = Build({Tri({0,0,4}, {4,0,0}, {0,0,0})}, UnitGrid());
    Check(reversed.cells == result.cells, "opposite winding has identical coverage");

    const Result translated = Build({Tri({-120,80,230}, {-116,80,230}, {-120,80,234})}, UnitGrid());
    Check(translated.cells == result.cells && translated.minX == -120 && translated.minZ == 230,
          "translation preserves occupancy and reports translated origin");
    const Result mirrored = Build({Tri({4,0,0}, {0,0,0}, {4,0,4})}, UnitGrid());
    for (std::size_t z = 0; z < 4; ++z)
        for (std::size_t x = 0; x < 4; ++x)
            Check(Has(mirrored, 3 - x, z, Layer::Render) == Has(result, x, z, Layer::Render),
                  "mirroring a triangle mirrors its occupied cells");
}

void TestTessellationAndLayers()
{
    std::vector<Triangle> triangles;
    Rectangle(triangles, 0, 0, 8, 6, Layer::Render);
    // Independently partition the same rectangle into 96 collision triangles.
    for (int z = 0; z < 6; ++z)
        for (int x = 0; x < 8; ++x)
            Rectangle(triangles, x, z, x + 1.0, z + 1.0, Layer::Collision);
    const Result result = Build(triangles, UnitGrid());
    Check(result.success && result.width == 8 && result.height == 6, "unequal tessellations share one grid");
    Check(result.render.accepted == 2 && result.collision.accepted == 96,
          "source triangle counts retain the different tessellations");
    Check(std::all_of(result.cells.begin(), result.cells.end(), [](std::uint8_t cell) {return cell == 3;}),
          "equal regions overlap completely regardless of tessellation");
    Check(result.render.coveredCells == 48 && result.collision.coveredCells == 48,
          "per-layer occupied counts are geometric, not vertex-based");

    triangles.clear();
    Rectangle(triangles, 0, 0, 2, 2, Layer::Render);
    Rectangle(triangles, 4, 0, 6, 2, Layer::Collision);
    Rectangle(triangles, 2.5, 0.25, 3.5, 1.75, Layer::Water);
    const Result separated = Build(triangles, UnitGrid());
    Check(separated.success && separated.width == 6 && separated.height == 2, "disjoint regions use combined bounds");
    Check(Has(separated, 0, 0, Layer::Render) && !Has(separated, 0, 0, Layer::Collision), "render-only region retained");
    Check(Has(separated, 5, 0, Layer::Collision) && !Has(separated, 5, 0, Layer::Render), "collision-only region retained");
    Check(Has(separated, 2, 0, Layer::Water) && Has(separated, 3, 0, Layer::Water), "water occupies its own layer");
    Check(!Has(separated, 0, 0, Layer::Water) && !Has(separated, 5, 0, Layer::Water), "water does not fill unrelated cells");
    Check(std::none_of(separated.cells.begin(), separated.cells.end(), [](std::uint8_t cell) {return (cell & 3) == 3;}),
          "separated render and collision regions are not reported as overlapping");

    triangles.clear();
    Rectangle(triangles, 0, 0, 4, 4, Layer::Render);
    Rectangle(triangles, 1, 1, 3, 3, Layer::Water);
    Rectangle(triangles, 2, 2, 6, 6, Layer::Collision);
    const Result overlap = Build(triangles, UnitGrid());
    Check(overlap.success && Has(overlap, 2, 2, Layer::Render) && Has(overlap, 2, 2, Layer::Collision) &&
          Has(overlap, 2, 2, Layer::Water), "overlapping layers retain all three bits");
    Check(!Has(overlap, 5, 0, Layer::Render) && !Has(overlap, 5, 0, Layer::Collision) &&
          !Has(overlap, 5, 0, Layer::Water), "empty common-bounds corners stay empty");
}

void TestTinyAndThinTriangles()
{
    std::vector<Triangle> triangles;
    Rectangle(triangles, 0, 0, 2, 2, Layer::Collision);
    triangles.push_back(Tri({0.98,0,0.99}, {1.02,0,0.99}, {1,0,1.02}));
    const Result result = Build(triangles, UnitGrid());
    Check(result.success && result.render.coveredCells == 4,
          "tiny triangle crossing a cell corner occupies all four intersected cells");

    triangles.clear();
    Rectangle(triangles, 0, 0, 10, 10, Layer::Collision);
    triangles.push_back(Tri({0.1,0,0.1}, {9.9,0,9.89}, {9.89,0,9.9}));
    const Result thin = Build(triangles, UnitGrid());
    Check(thin.success && Has(thin, 5, 5, Layer::Render), "thin diagonal triangle retains interior cells far from vertices");
    Check(!Has(thin, 0, 9, Layer::Render) && !Has(thin, 9, 0, Layer::Render), "triangle bounding box is not filled as a rectangle");

    const Result remote = Build({Tri({1.0e10,0,1.0e10}, {1.0e10+0.1,0,1.0e10}, {1.0e10,0,1.0e10+0.1})}, UnitGrid());
    Check(remote.success && remote.render.accepted == 1 && remote.render.coveredCells == 1,
          "small triangle far from origin does not lose its area to cancellation");
}

void TestRejections()
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    std::vector<Triangle> triangles = {
        Tri({0,0,0}, {1,0,0}, {0,0,1}),
        Tri({0,0,0}, {0,1,0}, {0,0,1}, Layer::Collision),
        Tri({0,0,0}, {1,0,0}, {2,0,0}),
        Tri({0,0,0}, {0,0,0}, {0,0,0}),
        Tri({nan,0,0}, {1,0,0}, {0,0,1}),
        Tri({0,inf,0}, {1,0,0}, {0,0,1}, Layer::Water),
        Tri({1.0e300,0,0}, {1,0,0}, {0,0,1}),
        Tri({0,0,0}, {1,0,0}, {0,0,1}, static_cast<Layer>(128)),
    };
    const Result result = Build(triangles, UnitGrid());
    Check(result.success && result.render.accepted == 1, "invalid individual geometry does not discard valid geometry");
    Check(result.invalidTriangles == 4 && result.degenerateTriangles == 2 && result.nearVerticalTriangles == 1,
          "nonfinite, out-of-range, unknown layer, degenerate and vertical rejection counts are explicit");
    Check(result.render.rejected == 4 && result.collision.rejected == 1 && result.water.rejected == 1,
          "valid layer IDs receive per-layer rejection counts");

    Options options = UnitGrid();
    options.minAbsNormalY = 0.75;
    const Result steep = Build({Tri({0,0,0}, {2,2,0}, {0,0,2})}, options);
    Check(steep.success && steep.nearVerticalTriangles == 1 && steep.cells.empty(), "normal threshold filters a known 45-degree slope");
    options.minAbsNormalY = 0.70;
    const Result slope = Build({Tri({0,0,0}, {2,2,0}, {0,0,2})}, options);
    Check(slope.success && slope.render.accepted == 1, "normal threshold accepts the same slope below its analytic cosine");
    options.minAbsNormalY = 0.0;
    const Result vertical = Build({Tri({0,0,0}, {0,1,0}, {0,0,1})}, options);
    Check(vertical.success && vertical.cells.empty() && vertical.nearVerticalTriangles == 1,
          "exact vertical faces never become floor coverage even with zero threshold");
}

void TestHeightClipping()
{
    Options options = UnitGrid();
    options.useHeightRange = true;
    options.minY = 1.0;
    options.maxY = 3.0;
    // The source triangle lies in y=x. Clipping must retain x in [1,3],
    // including crossing triangles with no original vertices in the slice.
    const Result result = Build({Tri({0,0,0}, {4,4,0}, {0,0,4})}, options);
    Check(result.success && result.render.accepted == 1 && result.minX == 1.0 && result.minZ == 0.0 &&
          result.width == 2 && result.height == 3, "crossing triangle is clipped to height slab before deriving map bounds");
    Check(Has(result, 0, 0, Layer::Render) && Has(result, 1, 0, Layer::Render), "clipped trapezoid includes its base");
    Check(Has(result, 0, 2, Layer::Render) && Has(result, 1, 2, Layer::Render), "clipped trapezoid conservatively includes its sloped edge contact");

    std::vector<Triangle> triangles;
    Rectangle(triangles, 0, 0, 10, 10, Layer::Render, -5.0);
    Rectangle(triangles, 2, 3, 4, 5, Layer::Collision, 2.0);
    const Result interior = Build(triangles, options);
    Check(interior.success && interior.render.accepted == 0 && interior.outsideHeightRangeTriangles == 2 &&
          interior.collision.accepted == 2 && interior.minX == 2 && interior.minZ == 3,
          "out-of-slice floors are excluded from occupancy and shared bounds");

    options.minY = options.maxY = 2.0;
    const Result exactFloor = Build(triangles, options);
    Check(exactFloor.success && exactFloor.collision.coveredCells == 4, "zero-width height slice retains a horizontal plane exactly on it");
    const Result exactSlope = Build({Tri({0,0,0}, {4,4,0}, {0,0,4})}, options);
    Check(exactSlope.success && exactSlope.cells.empty() && exactSlope.outsideHeightRangeTriangles == 1,
          "zero-area slope section does not become floor coverage");
}

void TestLimitsAndEmptyInput()
{
    const Result empty = Build({}, UnitGrid());
    Check(empty.success && empty.width == 0 && empty.height == 0 && empty.cells.empty() && empty.minX == 0.0,
          "empty input is a valid empty diagnostic");
    Options options = UnitGrid();
    options.maxCells = 16;
    std::vector<Triangle> triangles;
    Rectangle(triangles, -100, -100, 100, 100, Layer::Render);
    const Result coarse = Build(triangles, options);
    Check(coarse.success && coarse.coarsened && coarse.cellSize == 64.0 && coarse.width == 4 && coarse.height == 4,
          "large bounds coarsen explicitly to respect the requested allocation budget");
    Check(coarse.cells.size() <= options.maxCells && coarse.render.coveredCells == coarse.cells.size(),
          "coarsened complete plane still covers all bounded cells");

    options.maxCells = 1;
    const Result single = Build(triangles, options);
    Check(single.success && single.width == 1 && single.height == 1 && single.cellSize >= 200,
          "one-cell allocation limit terminates with disclosed coarse resolution");

    options = UnitGrid();
    options.maxCellTests = 1;
    const Result workLimited = Build(triangles, options);
    Check(!workLimited.success && !workLimited.error.empty() && workLimited.cells.empty(),
          "cell-test budget fails explicitly without exposing a partial coverage image");
    options = UnitGrid();
    options.maxTriangles = 1;
    const Result inputLimited = Build(triangles, options);
    Check(!inputLimited.success && !inputLimited.error.empty(), "input triangle budget fails before unbounded processing");

    const double invalidCellSizes[] = {0.0, -1.0, 1.0e-10, std::numeric_limits<double>::quiet_NaN(),
                                       std::numeric_limits<double>::infinity()};
    for (double cellSize : invalidCellSizes)
    {
        options = UnitGrid();
        options.cellSize = cellSize;
        Check(!Build({}, options).success, "invalid cell size rejected before grid calculations");
    }
    options = UnitGrid();
    options.maxCells = std::numeric_limits<std::size_t>::max();
    Check(!Build({}, options).success, "unbounded allocation option rejected");
    options = UnitGrid();
    options.minAbsNormalY = 1.1;
    Check(!Build({}, options).success, "invalid normal threshold rejected");
    options = UnitGrid();
    options.useHeightRange = true;
    options.minY = 2;
    options.maxY = 1;
    Check(!Build({}, options).success, "inverted height bounds rejected");
    options.maxY = std::numeric_limits<double>::infinity();
    Check(!Build({}, options).success, "nonfinite height bounds rejected");

    options = UnitGrid();
    options.cellSize = 0.000001;
    options.maxCells = 7;
    const Result extremes = Build({Tri({-1e12,0,-1e12}, {1e12,0,-1e12}, {-1e12,0,1e12})}, options);
    Check(extremes.success && extremes.cells.size() <= 7 && std::isfinite(extremes.cellSize),
          "extreme supported bounds terminate safely without integer or allocation overflow");
}
}

int main()
{
    TestAnalyticTriangles();
    TestTessellationAndLayers();
    TestTinyAndThinTriangles();
    TestRejections();
    TestHeightClipping();
    TestLimitsAndEmptyInput();
    std::cout << "Zone coverage: " << checks - failures << '/' << checks << " checks passed.\n";
    return failures == 0 ? 0 : 1;
}
