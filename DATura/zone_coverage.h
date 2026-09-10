#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Top-down projected occupancy, not a navigation or walkability calculation.
// Coordinates are in DATura's existing scene space; Y is vertical.
namespace ZoneCoverage
{
struct Point
{
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

enum class Layer : std::uint8_t
{
    Render = 1,
    Collision = 2,
    Water = 4,
};

struct Triangle
{
    Point points[3];
    Layer layer = Layer::Render;
};

struct Options
{
    double cellSize = 4.0;
    std::size_t maxCells = 1048576;
    double minAbsNormalY = 0.5;
    bool useHeightRange = false;
    double minY = 0.0;
    double maxY = 0.0;
    std::size_t maxTriangles = 4000000;
    std::size_t maxCellTests = 64000000;
};

struct LayerCounts
{
    std::size_t accepted = 0;
    std::size_t rejected = 0;
    std::size_t coveredCells = 0;
};

struct Result
{
    bool success = false;
    std::string error;
    double minX = 0.0;
    double minZ = 0.0;
    double cellSize = 0.0;
    std::size_t width = 0;
    std::size_t height = 0;
    // Row-major: cells[z * width + x]. Bits are Layer values. A cell is
    // covered when its closed rectangle intersects a retained polygon.
    std::vector<std::uint8_t> cells;
    LayerCounts render;
    LayerCounts collision;
    LayerCounts water;
    std::size_t invalidTriangles = 0;
    std::size_t degenerateTriangles = 0;
    std::size_t nearVerticalTriangles = 0;
    std::size_t outsideHeightRangeTriangles = 0;
    std::size_t cellTests = 0;
    bool coarsened = false;
};

// All layers share the bounds and resolution. Cell size is doubled as needed
// to respect maxCells. Invalid individual triangles are counted and skipped;
// invalid options, allocation failure, and exhausted work budgets fail the
// whole result. Empty valid input succeeds with an empty grid.
Result Build(const std::vector<Triangle>& triangles, const Options& options = {});
}
