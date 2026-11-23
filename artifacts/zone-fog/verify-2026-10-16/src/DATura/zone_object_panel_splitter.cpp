#include "stdafx.h"
#include "zone_object_panel_splitter.h"

#include "zone_object_panel_layout.h"

namespace ZoneObjectPanelSplitter
{
bool SetResizeCursorIfOverSplitter(const HWND panel, int paneWidths[3])
{
    POINT point = {};
    GetCursorPos(&point);
    ScreenToClient(panel, &point);
    RECT client = {};
    GetClientRect(panel, &client);
    if (!ZoneObjectPanelLayout::HitZonePaneSplitter(client.right - client.left,
                                                    client.bottom - client.top,
                                                    paneWidths, point.x, point.y))
        return false;

    SetCursor(LoadCursor(NULL, IDC_SIZEWE));
    return true;
}

bool BeginResizeDrag(const HWND panel, int paneWidths[3], const int mouseX, const int mouseY,
                     int& activeSplitter)
{
    RECT client = {};
    GetClientRect(panel, &client);
    activeSplitter = ZoneObjectPanelLayout::HitZonePaneSplitter(
        client.right - client.left, client.bottom - client.top, paneWidths, mouseX, mouseY);
    if (!activeSplitter)
        return false;

    SetCapture(panel);
    SetCursor(LoadCursor(NULL, IDC_SIZEWE));
    return true;
}

bool ResizeActiveDrag(const HWND panel, int paneWidths[3], const int activeSplitter,
                      const int mouseX, int* outClientWidth, int* outClientHeight)
{
    if (!activeSplitter)
        return false;

    RECT client = {};
    GetClientRect(panel, &client);
    const int clientWidth = client.right - client.left;
    const int clientHeight = client.bottom - client.top;
    ZoneObjectPanelLayout::ResizeZonePaneSplitter(clientWidth, paneWidths, activeSplitter, mouseX);
    if (outClientWidth)
        *outClientWidth = clientWidth;
    if (outClientHeight)
        *outClientHeight = clientHeight;
    return true;
}

bool EndResizeDrag(int& activeSplitter)
{
    if (!activeSplitter)
        return false;

    activeSplitter = 0;
    ReleaseCapture();
    return true;
}
}
