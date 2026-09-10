#include "zone_coverage.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <new>

namespace ZoneCoverage
{
namespace
{
// Scene coordinates far beyond any FFXI zone cannot be interpreted reliably
// at a useful map resolution. These limits also bound intermediate products.
constexpr double kMaxCoordinate = 1.0e12;
constexpr double kMinCellSize = 1.0e-6;
constexpr std::size_t kHardMaxCells = 16777216;
constexpr std::size_t kHardMaxTriangles = 10000000;
constexpr std::size_t kHardMaxCellTests = 256000000;

struct Polygon
{
    std::array<Point, 8> points;
    std::size_t count = 0;
};

enum class Rejection { None, Invalid, Degenerate, NearVertical, OutsideHeightRange };

LayerCounts* Counts(Result& result, Layer layer)
{
    switch (layer)
    {
    case Layer::Render: return &result.render;
    case Layer::Collision: return &result.collision;
    case Layer::Water: return &result.water;
    default: return nullptr;
    }
}

bool ValidCoordinate(double value)
{
    return std::isfinite(value) && std::abs(value) <= kMaxCoordinate;
}

Polygon ClipHeight(const Polygon& input, double height, bool retainAbove)
{
    Polygon output;
    if (input.count == 0)
        return output;
    Point previous = input.points[input.count - 1];
    bool previousInside = retainAbove ? previous.y >= height : previous.y <= height;
    for (std::size_t index = 0; index < input.count; ++index)
    {
        const Point current = input.points[index];
        const bool currentInside = retainAbove ? current.y >= height : current.y <= height;
        if (currentInside != previousInside)
        {
            // A crossing is possible only between bounded input coordinates;
            // an out-of-scene height never reaches this interpolation.
            const double t = (height - previous.y) / (current.y - previous.y);
            output.points[output.count++] = {
                previous.x + (current.x - previous.x) * t,
                height,
                previous.z + (current.z - previous.z) * t,
            };
        }
        if (currentInside)
            output.points[output.count++] = current;
        previous = current;
        previousInside = currentInside;
    }
    return output;
}

double ProjectedAreaTwice(const Polygon& polygon)
{
    if (polygon.count < 3)
        return 0.0;
    // Translate before forming products to preserve small polygons situated
    // far from the origin.
    const Point& origin = polygon.points[0];
    double area = 0.0;
    for (std::size_t index = 1; index + 1 < polygon.count; ++index)
    {
        const Point& a = polygon.points[index];
        const Point& b = polygon.points[index + 1];
        area += (a.x - origin.x) * (b.z - origin.z) -
                (a.z - origin.z) * (b.x - origin.x);
    }
    return area;
}

Rejection Prepare(const Triangle& triangle, const Options& options, Polygon& polygon)
{
    if (triangle.layer != Layer::Render && triangle.layer != Layer::Collision &&
        triangle.layer != Layer::Water)
        return Rejection::Invalid;
    for (const Point& point : triangle.points)
        if (!ValidCoordinate(point.x) || !ValidCoordinate(point.y) || !ValidCoordinate(point.z))
            return Rejection::Invalid;

    const Point a = {triangle.points[1].x - triangle.points[0].x,
                     triangle.points[1].y - triangle.points[0].y,
                     triangle.points[1].z - triangle.points[0].z};
    const Point b = {triangle.points[2].x - triangle.points[0].x,
                     triangle.points[2].y - triangle.points[0].y,
                     triangle.points[2].z - triangle.points[0].z};
    const double edgeScale = (std::max)({std::abs(a.x), std::abs(a.y), std::abs(a.z),
                                        std::abs(b.x), std::abs(b.y), std::abs(b.z)});
    if (edgeScale == 0.0)
        return Rejection::Degenerate;
    const Point unitA = {a.x / edgeScale, a.y / edgeScale, a.z / edgeScale};
    const Point unitB = {b.x / edgeScale, b.y / edgeScale, b.z / edgeScale};
    const Point normal = {unitA.y * unitB.z - unitA.z * unitB.y,
                          unitA.z * unitB.x - unitA.x * unitB.z,
                          unitA.x * unitB.y - unitA.y * unitB.x};
    const double normalLength = std::hypot(normal.x, normal.y, normal.z);
    if (!(normalLength > 0.0))
        return Rejection::Degenerate;
    if (normal.y == 0.0 || std::abs(normal.y) / normalLength < options.minAbsNormalY)
        return Rejection::NearVertical;

    polygon.count = 3;
    std::copy(std::begin(triangle.points), std::end(triangle.points), polygon.points.begin());
    if (options.useHeightRange)
    {
        polygon = ClipHeight(polygon, options.minY, true);
        polygon = ClipHeight(polygon, options.maxY, false);
        if (polygon.count < 3 || ProjectedAreaTwice(polygon) == 0.0)
            return Rejection::OutsideHeightRange;
    }
    else if (ProjectedAreaTwice(polygon) == 0.0)
        return Rejection::Degenerate;
    return Rejection::None;
}

void RecordRejection(Result& result, Layer layer, Rejection rejection)
{
    if (LayerCounts* counts = Counts(result, layer))
        ++counts->rejected;
    switch (rejection)
    {
    case Rejection::Invalid: ++result.invalidTriangles; break;
    case Rejection::Degenerate: ++result.degenerateTriangles; break;
    case Rejection::NearVertical: ++result.nearVerticalTriangles; break;
    case Rejection::OutsideHeightRange: ++result.outsideHeightRangeTriangles; break;
    default: break;
    }
}

bool IntersectsCell(const Polygon& polygon, double orientation, double x, double z)
{
    const double centerX = x + 0.5;
    const double centerZ = z + 0.5;
    for (std::size_t index = 0; index < polygon.count; ++index)
    {
        const Point& a = polygon.points[index];
        const Point& b = polygon.points[(index + 1) % polygon.count];
        const double dx = b.x - a.x;
        const double dz = b.z - a.z;
        const double distance = orientation * (dx * (centerZ - a.z) - dz * (centerX - a.x));
        const double radius = 0.5 * (std::abs(dx) + std::abs(dz));
        // Conservatively include edge/corner contacts. Error allowance scales
        // with the operations, not a fixed world-space expansion of geometry.
        const double error = 16.0 * std::numeric_limits<double>::epsilon() *
            (std::abs(dx * (centerZ - a.z)) + std::abs(dz * (centerX - a.x)) + radius);
        if (distance < -radius - error)
            return false;
    }
    return true;
}

std::size_t FirstCell(double value, std::size_t dimension)
{
    const double floorValue = std::floor(std::nextafter(value, -std::numeric_limits<double>::infinity()));
    if (floorValue <= 0.0)
        return 0;
    if (floorValue >= static_cast<double>(dimension - 1))
        return dimension - 1;
    return static_cast<std::size_t>(floorValue);
}

std::size_t LastCell(double value, std::size_t dimension)
{
    const double floorValue = std::floor(std::nextafter(value, std::numeric_limits<double>::infinity()));
    if (floorValue <= 0.0)
        return 0;
    if (floorValue >= static_cast<double>(dimension - 1))
        return dimension - 1;
    return static_cast<std::size_t>(floorValue);
}
}

Result Build(const std::vector<Triangle>& triangles, const Options& options)
{
    Result result;
    result.cellSize = options.cellSize;
    if (!std::isfinite(options.cellSize) || options.cellSize < kMinCellSize ||
        !std::isfinite(options.minAbsNormalY) || options.minAbsNormalY < 0.0 || options.minAbsNormalY > 1.0 ||
        (options.useHeightRange && (!std::isfinite(options.minY) || !std::isfinite(options.maxY) || options.minY > options.maxY)))
    {
        result.error = "Coverage options contain an invalid cell size, normal threshold, or height range.";
        return result;
    }
    if (options.maxCells == 0 || options.maxCells > kHardMaxCells ||
        options.maxTriangles == 0 || options.maxTriangles > kHardMaxTriangles ||
        options.maxCellTests == 0 || options.maxCellTests > kHardMaxCellTests)
    {
        result.error = "Coverage limits must be positive and no greater than 16777216 cells, 10000000 triangles, and 256000000 cell tests.";
        return result;
    }
    if (triangles.size() > options.maxTriangles)
    {
        result.error = "The scene exceeds the coverage triangle budget.";
        return result;
    }

    double maxX = -std::numeric_limits<double>::infinity();
    double maxZ = maxX;
    result.minX = std::numeric_limits<double>::infinity();
    result.minZ = result.minX;
    for (const Triangle& triangle : triangles)
    {
        Polygon polygon;
        const Rejection rejection = Prepare(triangle, options, polygon);
        if (rejection != Rejection::None)
        {
            RecordRejection(result, triangle.layer, rejection);
            continue;
        }
        ++Counts(result, triangle.layer)->accepted;
        for (std::size_t index = 0; index < polygon.count; ++index)
        {
            const Point& point = polygon.points[index];
            result.minX = (std::min)(result.minX, point.x);
            result.minZ = (std::min)(result.minZ, point.z);
            maxX = (std::max)(maxX, point.x);
            maxZ = (std::max)(maxZ, point.z);
        }
    }
    if (!std::isfinite(result.minX))
    {
        result.minX = 0.0;
        result.minZ = 0.0;
        result.success = true;
        return result;
    }

    const double extentX = maxX - result.minX;
    const double extentZ = maxZ - result.minZ;
    // Counts stay floating point until they fit the bounded allocation.
    for (;;)
    {
        const double width = (std::max)(1.0, std::ceil(extentX / result.cellSize));
        const double height = (std::max)(1.0, std::ceil(extentZ / result.cellSize));
        if (width * height <= static_cast<double>(options.maxCells))
        {
            result.width = static_cast<std::size_t>(width);
            result.height = static_cast<std::size_t>(height);
            break;
        }
        result.cellSize *= 2.0;
        result.coarsened = true;
    }
    try
    {
        result.cells.assign(result.width * result.height, 0);
    }
    catch (const std::bad_alloc&)
    {
        result.error = "Could not allocate the bounded coverage grid.";
        return result;
    }

    // Prepare again rather than keeping a potentially large second copy of
    // every triangle's clipped polygon alongside the scene snapshot.
    for (const Triangle& triangle : triangles)
    {
        Polygon polygon;
        if (Prepare(triangle, options, polygon) != Rejection::None)
            continue;
        double minColumn = std::numeric_limits<double>::infinity();
        double minRow = minColumn;
        double maxColumn = -minColumn;
        double maxRow = maxColumn;
        for (std::size_t index = 0; index < polygon.count; ++index)
        {
            Point& point = polygon.points[index];
            point.x = (point.x - result.minX) / result.cellSize;
            point.z = (point.z - result.minZ) / result.cellSize;
            minColumn = (std::min)(minColumn, point.x);
            minRow = (std::min)(minRow, point.z);
            maxColumn = (std::max)(maxColumn, point.x);
            maxRow = (std::max)(maxRow, point.z);
        }
        const double orientation = ProjectedAreaTwice(polygon) >= 0.0 ? 1.0 : -1.0;
        const std::size_t firstX = FirstCell(minColumn, result.width);
        const std::size_t firstZ = FirstCell(minRow, result.height);
        const std::size_t lastX = LastCell(maxColumn, result.width);
        const std::size_t lastZ = LastCell(maxRow, result.height);
        const std::size_t candidates = (lastX - firstX + 1) * (lastZ - firstZ + 1);
        if (candidates > options.maxCellTests - result.cellTests)
        {
            result.error = "Coverage exceeded its cell-test budget. Increase the cell size or narrow the height range.";
            result.cells.clear();
            return result;
        }
        result.cellTests += candidates;
        const std::uint8_t bit = static_cast<std::uint8_t>(triangle.layer);
        for (std::size_t z = firstZ; z <= lastZ; ++z)
            for (std::size_t x = firstX; x <= lastX; ++x)
                if (IntersectsCell(polygon, orientation, static_cast<double>(x), static_cast<double>(z)))
                    result.cells[z * result.width + x] |= bit;
    }
    for (const std::uint8_t cell : result.cells)
    {
        result.render.coveredCells += (cell & static_cast<std::uint8_t>(Layer::Render)) != 0;
        result.collision.coveredCells += (cell & static_cast<std::uint8_t>(Layer::Collision)) != 0;
        result.water.coveredCells += (cell & static_cast<std::uint8_t>(Layer::Water)) != 0;
    }
    result.success = true;
    return result;
}
}
