#include "stdafx.h"
#include "noesis_rapi.h"
#include "model_ff11.h"
#include "texture_viewer.h"
#include "resource.h"

#include <commctrl.h>
#include <shellapi.h>
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "msimg32.lib")
#pragma comment(lib, "shell32.lib")

namespace
{
    const char kTextureViewerClassName[] = "DATuraTextureViewerClass";
    const char kTextureCanvasClassName[] = "DATuraTextureCanvasClass";

    enum TextureViewerControlId
    {
        IDC_TEXTURE_OPEN = 9001,
        IDC_TEXTURE_PATH,
        IDC_TEXTURE_LIST,
        IDC_TEXTURE_CANVAS,
        IDC_TEXTURE_CHANNELS,
        IDC_TEXTURE_ZOOM,
        IDC_TEXTURE_STATUS
    };

    enum TextureChannelMode
    {
        TextureChannels_RGBA = 0,
        TextureChannels_RGB,
        TextureChannels_Alpha
    };

    HWND g_window = nullptr;
    HWND g_owner = nullptr;
    HWND g_path = nullptr;
    HWND g_list = nullptr;
    HWND g_canvas = nullptr;
    HWND g_channels = nullptr;
    HWND g_zoom = nullptr;
    HWND g_status = nullptr;
    char g_rootPath[MAX_PATH] = {};
    char g_currentPath[MAX_PATH] = {};
    noeRAPI_t* g_rapi = nullptr;
    noesisModel_t* g_model = nullptr;
    HBITMAP g_bitmap = nullptr;
    int g_bitmapWidth = 0;
    int g_bitmapHeight = 0;
    int g_selectedTexture = -1;
    TextureChannelMode g_channelMode = TextureChannels_RGBA;

    void SetRootPath(const char* rootPath)
    {
        strcpy_s(g_rootPath, rootPath ? rootPath : "");
        const size_t length = strlen(g_rootPath);
        if (length > 0 && length + 1 < sizeof(g_rootPath) &&
            g_rootPath[length - 1] != '\\' && g_rootPath[length - 1] != '/')
        {
            g_rootPath[length] = '\\';
            g_rootPath[length + 1] = '\0';
        }
    }

    const char* TextureFormatName(noesisTexType_e type)
    {
        switch (type)
        {
        case NOESISTEX_RGBA32: return "RGBA32";
        case NOESISTEX_DXT1: return "DXT1";
        case NOESISTEX_DXT3: return "DXT3";
        case NOESISTEX_DXT5: return "DXT5";
        default: return "Unknown";
        }
    }

    void SetStatus(const char* text)
    {
        if (g_status)
            SetWindowTextA(g_status, text ? text : "");
    }

    void DeletePreviewBitmap()
    {
        if (g_bitmap)
        {
            DeleteObject(g_bitmap);
            g_bitmap = nullptr;
        }
        g_bitmapWidth = 0;
        g_bitmapHeight = 0;
    }

    void UnloadTextureDat()
    {
        DeletePreviewBitmap();
        g_selectedTexture = -1;
        g_model = nullptr;
        delete g_rapi;
        g_rapi = nullptr;
    }

    noesisTex_t* SelectedTexture()
    {
        if (!g_model || !g_model->pMatData || g_selectedTexture < 0 ||
            g_selectedTexture >= g_model->pMatData->texCount)
        {
            return nullptr;
        }
        return g_model->pMatData->textures[g_selectedTexture];
    }

    bool ReadWholeFile(const char* path, std::vector<BYTE>& bytes)
    {
        bytes.clear();
        HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr,
                                  OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE)
            return false;

        LARGE_INTEGER size = {};
        const bool validSize = GetFileSizeEx(file, &size) != FALSE &&
                               size.QuadPart > 0 && size.QuadPart <= INT_MAX;
        if (!validSize)
        {
            CloseHandle(file);
            return false;
        }

        bytes.resize((size_t)size.QuadPart);
        DWORD bytesRead = 0;
        const BOOL readOk = ReadFile(file, bytes.data(), (DWORD)bytes.size(), &bytesRead, nullptr);
        CloseHandle(file);
        if (!readOk || bytesRead != bytes.size())
        {
            bytes.clear();
            return false;
        }
        return true;
    }

    bool TextureDataIsComplete(const noesisTex_t* texture)
    {
        if (!texture || !texture->data || texture->dataLen <= 0 ||
            texture->w <= 0 || texture->h <= 0 ||
            texture->w > 16384 || texture->h > 16384)
        {
            return false;
        }

        const size_t width = (size_t)texture->w;
        const size_t height = (size_t)texture->h;
        if (width * height > (size_t)INT_MAX / 4)
            return false;
        size_t expected = 0;
        if (texture->texType == NOESISTEX_RGBA32)
        {
            expected = width * height * 4;
        }
        else if (texture->texType == NOESISTEX_DXT1 ||
                 texture->texType == NOESISTEX_DXT3 ||
                 texture->texType == NOESISTEX_DXT5)
        {
            const size_t blockBytes = texture->texType == NOESISTEX_DXT1 ? 8 : 16;
            expected = ((width + 3) / 4) * ((height + 3) / 4) * blockBytes;
        }
        return expected > 0 && expected <= (size_t)texture->dataLen;
    }

    bool BuildPreviewBitmap()
    {
        DeletePreviewBitmap();
        noesisTex_t* texture = SelectedTexture();
        if (!TextureDataIsComplete(texture) || !g_rapi)
        {
            SetStatus("The selected texture has incomplete or unsupported pixel data.");
            if (g_canvas)
                InvalidateRect(g_canvas, nullptr, FALSE);
            return false;
        }

        unsigned char* decoded = texture->data;
        bool freeDecoded = false;
        if (texture->texType != NOESISTEX_RGBA32)
        {
            decoded = g_rapi->Noesis_ConvertDXT(texture->w, texture->h,
                                                texture->data, texture->texType);
            freeDecoded = decoded != nullptr;
        }
        if (!decoded)
            return false;

        BITMAPINFO bitmapInfo = {};
        bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bitmapInfo.bmiHeader.biWidth = texture->w;
        bitmapInfo.bmiHeader.biHeight = -texture->h; // top-down DIB
        bitmapInfo.bmiHeader.biPlanes = 1;
        bitmapInfo.bmiHeader.biBitCount = 32;
        bitmapInfo.bmiHeader.biCompression = BI_RGB;

        void* bitmapBits = nullptr;
        HDC screen = GetDC(nullptr);
        g_bitmap = CreateDIBSection(screen, &bitmapInfo, DIB_RGB_COLORS,
                                    &bitmapBits, nullptr, 0);
        ReleaseDC(nullptr, screen);
        if (!g_bitmap || !bitmapBits)
        {
            if (freeDecoded)
                g_rapi->Noesis_UnpooledFree(decoded);
            DeletePreviewBitmap();
            return false;
        }

        const uint32_t* source = reinterpret_cast<const uint32_t*>(decoded);
        uint32_t* destination = reinterpret_cast<uint32_t*>(bitmapBits);
        const size_t pixelCount = (size_t)texture->w * (size_t)texture->h;
        for (size_t i = 0; i < pixelCount; ++i)
        {
            const uint32_t pixel = source[i];
            uint32_t alpha = (pixel >> 24) & 0xFF;
            uint32_t red = (pixel >> 16) & 0xFF;
            uint32_t green = (pixel >> 8) & 0xFF;
            uint32_t blue = pixel & 0xFF;

			// Match the main renderer's FFXI-specific DXT3 alpha interpretation.
			// The shader multiplies sampled alpha by 1.875 (15/8) and saturates.
			if (texture->texType == NOESISTEX_DXT3)
				alpha = std::min<uint32_t>(255, (alpha * 15 + 4) / 8);

            if (g_channelMode == TextureChannels_RGBA)
            {
                // AlphaBlend expects premultiplied BGRA pixels.
                red = (red * alpha + 127) / 255;
                green = (green * alpha + 127) / 255;
                blue = (blue * alpha + 127) / 255;
                destination[i] = (alpha << 24) | (red << 16) | (green << 8) | blue;
            }
            else if (g_channelMode == TextureChannels_Alpha)
            {
                destination[i] = 0xFF000000u | (alpha << 16) | (alpha << 8) | alpha;
            }
            else
            {
                destination[i] = 0xFF000000u | (red << 16) | (green << 8) | blue;
            }
        }

        if (freeDecoded)
            g_rapi->Noesis_UnpooledFree(decoded);

        g_bitmapWidth = texture->w;
        g_bitmapHeight = texture->h;
        if (g_canvas)
            InvalidateRect(g_canvas, nullptr, FALSE);
        return true;
    }

    float SelectedZoomScale(int canvasWidth, int canvasHeight)
    {
        const int selection = g_zoom ? (int)SendMessageA(g_zoom, CB_GETCURSEL, 0, 0) : 0;
        if (selection <= 0)
        {
            if (g_bitmapWidth <= 0 || g_bitmapHeight <= 0)
                return 1.0f;
            const float xScale = (float)std::max(1, canvasWidth - 24) / (float)g_bitmapWidth;
            const float yScale = (float)std::max(1, canvasHeight - 24) / (float)g_bitmapHeight;
            return std::min(1.0f, std::min(xScale, yScale));
        }
        static const float scales[] = { 0.25f, 0.5f, 1.0f, 2.0f, 4.0f, 8.0f };
        const int scaleIndex = std::clamp(selection - 1, 0, (int)(sizeof(scales) / sizeof(scales[0])) - 1);
        return scales[scaleIndex];
    }

    void PaintCheckerboard(HDC dc, const RECT& bounds, const RECT& clipBounds)
    {
        const COLORREF colors[] = { RGB(188, 188, 188), RGB(228, 228, 228) };
        const int square = 12;
        RECT visible = {};
        if (!IntersectRect(&visible, &bounds, &clipBounds))
            return;

        HBRUSH brushes[] = { CreateSolidBrush(colors[0]), CreateSolidBrush(colors[1]) };
        const int firstColumn = (int)floor((double)(visible.left - bounds.left) / square);
        const int firstRow = (int)floor((double)(visible.top - bounds.top) / square);
        const int startX = bounds.left + firstColumn * square;
        const int startY = bounds.top + firstRow * square;
        for (int y = startY; y < visible.bottom; y += square)
        {
            for (int x = startX; x < visible.right; x += square)
            {
                RECT cell = { std::max(x, (int)visible.left), std::max(y, (int)visible.top),
                              std::min(x + square, (int)visible.right),
                              std::min(y + square, (int)visible.bottom) };
                const int column = (x - bounds.left) / square;
                const int row = (y - bounds.top) / square;
                FillRect(dc, &cell, brushes[(column + row) & 1]);
            }
        }
        DeleteObject(brushes[0]);
        DeleteObject(brushes[1]);
    }

    LRESULT CALLBACK TextureCanvasWndProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch (message)
        {
        case WM_ERASEBKGND:
            return 1;

        case WM_MOUSEWHEEL:
            if (g_zoom)
            {
                int selection = (int)SendMessageA(g_zoom, CB_GETCURSEL, 0, 0);
                selection += GET_WHEEL_DELTA_WPARAM(wParam) > 0 ? 1 : -1;
                selection = std::clamp(selection, 0, 6);
                SendMessageA(g_zoom, CB_SETCURSEL, selection, 0);
                InvalidateRect(window, nullptr, FALSE);
            }
            return 0;

        case WM_PAINT:
        {
            PAINTSTRUCT paint = {};
            HDC dc = BeginPaint(window, &paint);
            RECT client = {};
            GetClientRect(window, &client);
            HBRUSH background = CreateSolidBrush(RGB(45, 48, 52));
            FillRect(dc, &client, background);
            DeleteObject(background);

            if (g_bitmap && g_bitmapWidth > 0 && g_bitmapHeight > 0)
            {
                const float scale = SelectedZoomScale(client.right, client.bottom);
                const int drawWidth = std::max(1, (int)(g_bitmapWidth * scale + 0.5f));
                const int drawHeight = std::max(1, (int)(g_bitmapHeight * scale + 0.5f));
                const int drawX = (client.right - drawWidth) / 2;
                const int drawY = (client.bottom - drawHeight) / 2;
                RECT imageBounds = { drawX, drawY, drawX + drawWidth, drawY + drawHeight };
                PaintCheckerboard(dc, imageBounds, client);

                HDC memoryDc = CreateCompatibleDC(dc);
                HGDIOBJ previousBitmap = SelectObject(memoryDc, g_bitmap);
                SetStretchBltMode(dc, HALFTONE);
                if (g_channelMode == TextureChannels_RGBA)
                {
                    BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
                    AlphaBlend(dc, drawX, drawY, drawWidth, drawHeight,
                               memoryDc, 0, 0, g_bitmapWidth, g_bitmapHeight, blend);
                }
                else
                {
                    StretchBlt(dc, drawX, drawY, drawWidth, drawHeight,
                               memoryDc, 0, 0, g_bitmapWidth, g_bitmapHeight, SRCCOPY);
                }
                SelectObject(memoryDc, previousBitmap);
                DeleteDC(memoryDc);

                HBRUSH frame = CreateSolidBrush(RGB(110, 114, 120));
                FrameRect(dc, &imageBounds, frame);
                DeleteObject(frame);
            }
            else
            {
                SetBkMode(dc, TRANSPARENT);
                SetTextColor(dc, RGB(220, 220, 220));
                DrawTextA(dc, "Open an FFXI DAT and select a texture to preview it.", -1,
                          &client, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            }
            EndPaint(window, &paint);
            return 0;
        }
        }
        return DefWindowProcA(window, message, wParam, lParam);
    }

    void PopulateTextureList()
    {
        ListView_DeleteAllItems(g_list);
        if (!g_model || !g_model->pMatData)
            return;

        for (int i = 0; i < g_model->pMatData->texCount; ++i)
        {
            noesisTex_t* texture = g_model->pMatData->textures[i];
            if (!texture)
                continue;

            LVITEMA item = {};
            item.mask = LVIF_TEXT | LVIF_PARAM;
            item.iItem = ListView_GetItemCount(g_list);
            item.pszText = texture->name && texture->name[0] ? texture->name : const_cast<LPSTR>("(unnamed)");
            item.lParam = i;
            const int row = (int)SendMessageA(g_list, LVM_INSERTITEMA, 0, (LPARAM)&item);

            char dimensions[48] = {};
            sprintf_s(dimensions, "%d x %d", texture->w, texture->h);
            LVITEMA subItem = {};
            subItem.iSubItem = 1;
            subItem.pszText = dimensions;
            SendMessageA(g_list, LVM_SETITEMTEXTA, row, (LPARAM)&subItem);
            subItem.iSubItem = 2;
            subItem.pszText = const_cast<LPSTR>(TextureFormatName(texture->texType));
            SendMessageA(g_list, LVM_SETITEMTEXTA, row, (LPARAM)&subItem);

            char bytes[48] = {};
            if (texture->dataLen >= 1024 * 1024)
                sprintf_s(bytes, "%.2f MB", (double)texture->dataLen / (1024.0 * 1024.0));
            else
                sprintf_s(bytes, "%.1f KB", (double)texture->dataLen / 1024.0);
            subItem.iSubItem = 3;
            subItem.pszText = bytes;
            SendMessageA(g_list, LVM_SETITEMTEXTA, row, (LPARAM)&subItem);
        }

        if (ListView_GetItemCount(g_list) > 0)
        {
            ListView_SetItemState(g_list, 0, LVIS_SELECTED | LVIS_FOCUSED,
                                  LVIS_SELECTED | LVIS_FOCUSED);
            ListView_EnsureVisible(g_list, 0, FALSE);
        }
    }

    void SelectTextureFromList()
    {
        const int row = ListView_GetNextItem(g_list, -1, LVNI_SELECTED);
        if (row < 0)
            return;

        LVITEMA item = {};
        item.mask = LVIF_PARAM;
        item.iItem = row;
        if (!SendMessageA(g_list, LVM_GETITEMA, 0, (LPARAM)&item))
            return;

        g_selectedTexture = (int)item.lParam;
        noesisTex_t* texture = SelectedTexture();
        if (!texture)
            return;

        BuildPreviewBitmap();
        char status[512] = {};
        sprintf_s(status, "%d of %d  |  %s  |  %d x %d  |  %s  |  %d bytes",
                  row + 1, ListView_GetItemCount(g_list),
                  texture->name && texture->name[0] ? texture->name : "(unnamed)",
                  texture->w, texture->h, TextureFormatName(texture->texType), texture->dataLen);
        SetStatus(status);
    }

    bool LoadTextureDat(const char* path)
    {
        std::vector<BYTE> fileBytes;
        if (!ReadWholeFile(path, fileBytes))
        {
            MessageBoxA(g_window, "The selected DAT could not be read.",
                        "Texture Viewer", MB_OK | MB_ICONERROR);
            return false;
        }

        noeRAPI_t* newRapi = new noeRAPI_t(nullptr);
        newRapi->SetCurrentFilePath(path);
        ff11Opts_t textureOptions = {};
        textureOptions.noShinyMaterials = true;
        ff11Opts_t* previousOptions = gpFF11Opts;
        gpFF11Opts = &textureOptions;
        int modelCount = 0;
        noesisModel_t* newModel = Model_FF11_LoadTextureDAT(
            fileBytes.data(), (int)fileBytes.size(), modelCount, newRapi);
        gpFF11Opts = previousOptions;

        if (!newModel || modelCount == 0 || !newModel->pMatData ||
            newModel->pMatData->texCount <= 0)
        {
            delete newRapi;
            MessageBoxA(g_window,
                        "No supported FFXI textures were found in the selected DAT.",
                        "Texture Viewer", MB_OK | MB_ICONINFORMATION);
            return false;
        }

        UnloadTextureDat();
        g_rapi = newRapi;
        g_model = newModel;
        strcpy_s(g_currentPath, path);
        SetWindowTextA(g_path, path);

        const char* fileName = strrchr(path, '\\');
        fileName = fileName ? fileName + 1 : path;
        char title[MAX_PATH + 64] = {};
        sprintf_s(title, "DATura Image / Texture Viewer - %s", fileName);
        SetWindowTextA(g_window, title);
        PopulateTextureList();
        return true;
    }

    void BrowseForTextureDat()
    {
        char path[MAX_PATH] = {};
        OPENFILENAMEA dialog = {};
        dialog.lStructSize = sizeof(dialog);
        dialog.hwndOwner = g_window;
        dialog.lpstrFilter = "FFXI DAT Files (*.DAT)\0*.DAT\0All Files (*.*)\0*.*\0";
        dialog.lpstrFile = path;
        dialog.nMaxFile = MAX_PATH;
        dialog.lpstrTitle = "Open FFXI Texture DAT";
        dialog.lpstrInitialDir = g_rootPath[0] ? g_rootPath : nullptr;
        dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
        if (GetOpenFileNameA(&dialog))
            LoadTextureDat(path);
    }

    void LayoutTextureViewer(HWND window)
    {
        RECT client = {};
        GetClientRect(window, &client);
        const int width = client.right;
        const int height = client.bottom;
        const int margin = 10;
        const int rowHeight = 25;
        const int statusHeight = 22;
        const int listWidth = std::clamp(width / 3, 280, 430);
        const int contentTop = margin + rowHeight + 9;
        const int contentHeight = std::max(100, height - contentTop - statusHeight - margin);

        HWND openButton = GetDlgItem(window, IDC_TEXTURE_OPEN);
        HWND channelLabel = GetDlgItem(window, -10);
        HWND zoomLabel = GetDlgItem(window, -11);
        if (openButton) MoveWindow(openButton, margin, margin, 88, rowHeight, TRUE);

        const int toolsWidth = 326;
        if (g_path)
            MoveWindow(g_path, margin + 96, margin,
                       std::max(80, width - margin * 2 - 96 - toolsWidth), rowHeight, TRUE);
        const int toolsX = std::max(margin + 190, width - margin - toolsWidth);
        if (channelLabel) MoveWindow(channelLabel, toolsX, margin + 4, 58, 18, TRUE);
        if (g_channels) MoveWindow(g_channels, toolsX + 60, margin, 104, 180, TRUE);
        if (zoomLabel) MoveWindow(zoomLabel, toolsX + 174, margin + 4, 42, 18, TRUE);
        if (g_zoom) MoveWindow(g_zoom, toolsX + 218, margin, 108, 180, TRUE);

        if (g_list)
            MoveWindow(g_list, margin, contentTop, listWidth, contentHeight, TRUE);
        if (g_canvas)
            MoveWindow(g_canvas, margin + listWidth + 8, contentTop,
                       std::max(80, width - (margin * 2 + listWidth + 8)), contentHeight, TRUE);
        if (g_status)
            MoveWindow(g_status, margin, height - statusHeight - 2,
                       std::max(80, width - margin * 2), statusHeight, TRUE);
    }

    LRESULT CALLBACK TextureViewerWndProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch (message)
        {
        case WM_CREATE:
        {
            g_window = window;
            const HINSTANCE instance = GetModuleHandleA(nullptr);
            CreateWindowExA(0, "BUTTON", "Open DAT...",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                0, 0, 0, 0, window, (HMENU)(INT_PTR)IDC_TEXTURE_OPEN, instance, nullptr);
            g_path = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "No DAT open",
                WS_CHILD | WS_VISIBLE | ES_READONLY | ES_AUTOHSCROLL,
                0, 0, 0, 0, window, (HMENU)(INT_PTR)IDC_TEXTURE_PATH, instance, nullptr);
            CreateWindowExA(0, "STATIC", "Channels:", WS_CHILD | WS_VISIBLE,
                0, 0, 0, 0, window, (HMENU)(INT_PTR)-10, instance, nullptr);
            g_channels = CreateWindowExA(0, "COMBOBOX", "",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
                0, 0, 0, 0, window, (HMENU)(INT_PTR)IDC_TEXTURE_CHANNELS, instance, nullptr);
            SendMessageA(g_channels, CB_ADDSTRING, 0, (LPARAM)"RGBA");
            SendMessageA(g_channels, CB_ADDSTRING, 0, (LPARAM)"RGB");
            SendMessageA(g_channels, CB_ADDSTRING, 0, (LPARAM)"Alpha");
            SendMessageA(g_channels, CB_SETCURSEL, TextureChannels_RGBA, 0);
            CreateWindowExA(0, "STATIC", "Zoom:", WS_CHILD | WS_VISIBLE,
                0, 0, 0, 0, window, (HMENU)(INT_PTR)-11, instance, nullptr);
            g_zoom = CreateWindowExA(0, "COMBOBOX", "",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
                0, 0, 0, 0, window, (HMENU)(INT_PTR)IDC_TEXTURE_ZOOM, instance, nullptr);
            const char* zoomLabels[] = { "Fit", "25%", "50%", "100%", "200%", "400%", "800%" };
            for (const char* label : zoomLabels)
                SendMessageA(g_zoom, CB_ADDSTRING, 0, (LPARAM)label);
            SendMessageA(g_zoom, CB_SETCURSEL, 0, 0);

            g_list = CreateWindowExA(WS_EX_CLIENTEDGE, WC_LISTVIEWA, "",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SINGLESEL,
                0, 0, 0, 0, window, (HMENU)(INT_PTR)IDC_TEXTURE_LIST, instance, nullptr);
            ListView_SetExtendedListViewStyle(g_list,
                LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER | LVS_EX_LABELTIP);
            const char* headings[] = { "Name", "Size", "Format", "Data" };
            const int widths[] = { 150, 76, 66, 70 };
            for (int columnIndex = 0; columnIndex < 4; ++columnIndex)
            {
                LVCOLUMNA column = {};
                column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
                column.iSubItem = columnIndex;
                column.cx = widths[columnIndex];
                column.pszText = const_cast<LPSTR>(headings[columnIndex]);
                SendMessageA(g_list, LVM_INSERTCOLUMNA, columnIndex, (LPARAM)&column);
            }

            g_canvas = CreateWindowExA(WS_EX_CLIENTEDGE, kTextureCanvasClassName, "",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                0, 0, 0, 0, window, (HMENU)(INT_PTR)IDC_TEXTURE_CANVAS, instance, nullptr);
            g_status = CreateWindowExA(0, "STATIC",
                "Open a DAT containing FFXI texture chunks.",
                WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP,
                0, 0, 0, 0, window, (HMENU)(INT_PTR)IDC_TEXTURE_STATUS, instance, nullptr);

            const HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            EnumChildWindows(window, [](HWND child, LPARAM fontParam) -> BOOL
            {
                SendMessage(child, WM_SETFONT, (WPARAM)fontParam, TRUE);
                return TRUE;
            }, (LPARAM)font);
            DragAcceptFiles(window, TRUE);
            LayoutTextureViewer(window);
            return 0;
        }

        case WM_SIZE:
            LayoutTextureViewer(window);
            return 0;

        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
            case IDC_TEXTURE_OPEN:
                if (HIWORD(wParam) == BN_CLICKED)
                    BrowseForTextureDat();
                return 0;
            case IDC_TEXTURE_CHANNELS:
                if (HIWORD(wParam) == CBN_SELCHANGE)
                {
                    g_channelMode = (TextureChannelMode)SendMessageA(g_channels, CB_GETCURSEL, 0, 0);
                    BuildPreviewBitmap();
                }
                return 0;
            case IDC_TEXTURE_ZOOM:
                if (HIWORD(wParam) == CBN_SELCHANGE && g_canvas)
                    InvalidateRect(g_canvas, nullptr, FALSE);
                return 0;
            }
            break;

        case WM_NOTIFY:
            if (((LPNMHDR)lParam)->idFrom == IDC_TEXTURE_LIST &&
                ((LPNMHDR)lParam)->code == LVN_ITEMCHANGED)
            {
                const NMLISTVIEW* change = (const NMLISTVIEW*)lParam;
                if ((change->uNewState & LVIS_SELECTED) &&
                    !(change->uOldState & LVIS_SELECTED))
                {
                    SelectTextureFromList();
                }
            }
            return 0;

        case WM_DROPFILES:
        {
            char path[MAX_PATH] = {};
            HDROP drop = (HDROP)wParam;
            if (DragQueryFileA(drop, 0, path, MAX_PATH) > 0)
                LoadTextureDat(path);
            DragFinish(drop);
            return 0;
        }

        case WM_CLOSE:
            DestroyWindow(window);
            return 0;

        case WM_DESTROY:
            DragAcceptFiles(window, FALSE);
            UnloadTextureDat();
            g_window = nullptr;
            g_path = nullptr;
            g_list = nullptr;
            g_canvas = nullptr;
            g_channels = nullptr;
            g_zoom = nullptr;
            g_status = nullptr;
            g_currentPath[0] = '\0';
            return 0;
        }
        return DefWindowProcA(window, message, wParam, lParam);
    }

    bool RegisterTextureViewerClasses(HINSTANCE instance)
    {
        WNDCLASSEXA existing = {};
        existing.cbSize = sizeof(existing);
        if (!GetClassInfoExA(instance, kTextureCanvasClassName, &existing))
        {
            WNDCLASSEXA canvasClass = {};
            canvasClass.cbSize = sizeof(canvasClass);
            canvasClass.style = CS_HREDRAW | CS_VREDRAW;
            canvasClass.lpfnWndProc = TextureCanvasWndProc;
            canvasClass.hInstance = instance;
            canvasClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
            canvasClass.hbrBackground = (HBRUSH)(COLOR_APPWORKSPACE + 1);
            canvasClass.lpszClassName = kTextureCanvasClassName;
            if (!RegisterClassExA(&canvasClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
                return false;
        }

        existing = {};
        existing.cbSize = sizeof(existing);
        if (!GetClassInfoExA(instance, kTextureViewerClassName, &existing))
        {
            WNDCLASSEXA viewerClass = {};
            viewerClass.cbSize = sizeof(viewerClass);
            viewerClass.style = CS_HREDRAW | CS_VREDRAW;
            viewerClass.lpfnWndProc = TextureViewerWndProc;
            viewerClass.hInstance = instance;
            viewerClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
            viewerClass.hIcon = LoadIcon(instance, MAKEINTRESOURCE(IDI_DATURA));
            viewerClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
            viewerClass.lpszClassName = kTextureViewerClassName;
            if (!RegisterClassExA(&viewerClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
                return false;
        }
        return true;
    }
}

void TextureViewer_Show(HWND owner, const char* ffxiRootPath)
{
    g_owner = owner;
    SetRootPath(ffxiRootPath);
    if (TextureViewer_IsOpen())
    {
        ShowWindow(g_window, SW_RESTORE);
        SetForegroundWindow(g_window);
        return;
    }

    const HINSTANCE instance = GetModuleHandleA(nullptr);
    if (!RegisterTextureViewerClasses(instance))
        return;

    g_window = CreateWindowExA(WS_EX_APPWINDOW, kTextureViewerClassName,
        "DATura Image / Texture Viewer", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 1120, 760, owner, nullptr, instance, nullptr);
    if (g_window)
    {
        ShowWindow(g_window, SW_SHOW);
        UpdateWindow(g_window);
        SetForegroundWindow(g_window);
    }
}

void TextureViewer_SetRootPath(const char* ffxiRootPath)
{
    SetRootPath(ffxiRootPath);
}

bool TextureViewer_IsOpen()
{
    return g_window && IsWindow(g_window);
}
