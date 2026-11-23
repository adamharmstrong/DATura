#pragma once

#include <windows.h>

// Opens (or focuses) the FFXI DAT image/texture browser.
void TextureViewer_Show(HWND owner, const char* ffxiRootPath);
void TextureViewer_SetRootPath(const char* ffxiRootPath);
bool TextureViewer_IsOpen();
