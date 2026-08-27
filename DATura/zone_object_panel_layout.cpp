#include "stdafx.h"
#include "zone_object_panel_layout.h"

#include <algorithm>
#include <cstdlib>

namespace
{
constexpr int kMargin = 8;
constexpr int kGap = 10;
constexpr int kMinimumTotalWidth = 300;
constexpr int kDefaultMinimumPaneWidth = 220;
constexpr int kHeaderHeight = 56;
constexpr int kBottomHeight = 230;

int GetTotalPaneWidth(const int clientWidth)
{
    return std::max(kMinimumTotalWidth, clientWidth - kMargin * 2 - kGap * 2);
}

int GetMinimumPaneWidth(const int totalWidth)
{
    return kDefaultMinimumPaneWidth * 3 > totalWidth ? totalWidth / 3 : kDefaultMinimumPaneWidth;
}
}

namespace ZoneObjectPanelLayout
{
void ComputeZonePaneLayout(const int clientWidth, int paneWidths[3], int* placedX,
                           int* placedWidth, int* rawX, int* rawWidth,
                           int* collisionX, int* collisionWidth)
{
    const int totalWidth = GetTotalPaneWidth(clientWidth);
    const int minimumPaneWidth = GetMinimumPaneWidth(totalWidth);

    if (paneWidths[0] <= 0 || paneWidths[1] <= 0 || paneWidths[2] <= 0)
    {
        paneWidths[0] = totalWidth / 3;
        paneWidths[1] = totalWidth / 3;
        paneWidths[2] = totalWidth - paneWidths[0] - paneWidths[1];
    }

    paneWidths[0] = std::max(minimumPaneWidth, paneWidths[0]);
    paneWidths[1] = std::max(minimumPaneWidth, paneWidths[1]);
    paneWidths[2] = std::max(minimumPaneWidth, paneWidths[2]);

    const int currentTotal = paneWidths[0] + paneWidths[1] + paneWidths[2];
    if (currentTotal != totalWidth)
    {
        const int delta = totalWidth - currentTotal;
        paneWidths[2] += delta;
        if (paneWidths[2] < minimumPaneWidth)
        {
            const int needed = minimumPaneWidth - paneWidths[2];
            paneWidths[2] = minimumPaneWidth;
            paneWidths[1] = std::max(minimumPaneWidth, paneWidths[1] - needed);
        }
    }

    if (placedX) *placedX = kMargin;
    if (placedWidth) *placedWidth = paneWidths[0];
    if (rawX) *rawX = kMargin + paneWidths[0] + kGap;
    if (rawWidth) *rawWidth = paneWidths[1];
    if (collisionX) *collisionX = kMargin + paneWidths[0] + kGap + paneWidths[1] + kGap;
    if (collisionWidth)
        *collisionWidth = std::max(minimumPaneWidth, totalWidth - paneWidths[0] - paneWidths[1]);
}

int HitZonePaneSplitter(const int clientWidth, const int clientHeight, int paneWidths[3],
                        const int x, const int y)
{
    const int listBottom = clientHeight > 0 ? clientHeight - kBottomHeight : 0;
    if (y < kHeaderHeight || y > listBottom)
        return 0;

    int placedX = 0;
    int placedWidth = 0;
    int rawX = 0;
    int rawWidth = 0;
    ComputeZonePaneLayout(clientWidth, paneWidths, &placedX, &placedWidth,
                          &rawX, &rawWidth, nullptr, nullptr);
    const int firstSplitter = placedX + placedWidth + 5;
    const int secondSplitter = rawX + rawWidth + 5;
    if (std::abs(x - firstSplitter) <= 6)
        return 1;
    if (std::abs(x - secondSplitter) <= 6)
        return 2;
    return 0;
}

void ResizeZonePaneSplitter(const int clientWidth, int paneWidths[3], const int splitter,
                            const int mouseX)
{
    const int totalWidth = GetTotalPaneWidth(clientWidth);
    const int minimumPaneWidth = GetMinimumPaneWidth(totalWidth);
    if (splitter == 1)
    {
        const int combinedFirstSecond = paneWidths[0] + paneWidths[1];
        int newFirstWidth = mouseX - kMargin;
        newFirstWidth = std::max(minimumPaneWidth,
                                 std::min(newFirstWidth, combinedFirstSecond - minimumPaneWidth));
        paneWidths[0] = newFirstWidth;
        paneWidths[1] = combinedFirstSecond - newFirstWidth;
    }
    else if (splitter == 2)
    {
        const int secondStart = kMargin + paneWidths[0] + kGap;
        const int combinedSecondThird = paneWidths[1] + paneWidths[2];
        int newSecondWidth = mouseX - secondStart;
        newSecondWidth = std::max(minimumPaneWidth,
                                  std::min(newSecondWidth, combinedSecondThird - minimumPaneWidth));
        paneWidths[1] = newSecondWidth;
        paneWidths[2] = combinedSecondThird - newSecondWidth;
    }
}
}
