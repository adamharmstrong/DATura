#pragma once

namespace ZoneObjectPanelLayout
{
void ComputeZonePaneLayout(int clientWidth, int paneWidths[3], int* placedX, int* placedWidth,
                           int* rawX, int* rawWidth, int* collisionX, int* collisionWidth);
int HitZonePaneSplitter(int clientWidth, int clientHeight, int paneWidths[3], int x, int y);
void ResizeZonePaneSplitter(int clientWidth, int paneWidths[3], int splitter, int mouseX);
}
