#pragma once

#include <windows.h>

namespace Win32Theme
{
struct Resources
{
    HBRUSH windowBrush = NULL;
    HBRUSH controlBrush = NULL;
    HBRUSH editBrush = NULL;
    HFONT font = NULL;
    HFONT sectionFont = NULL;
};

struct State
{
    bool dark = true;
    Resources resources;
};

COLORREF WindowColor(bool dark);
COLORREF ControlColor(bool dark);
COLORREF EditColor(bool dark);
COLORREF TextColor(bool dark);
COLORREF MutedTextColor(bool dark);

bool LoadDarkModePreference(const char* registryKey, const char* valueName,
                            bool defaultDark = true);
void SaveDarkModePreference(const char* registryKey, const char* valueName, bool dark);
void InitializeState(State& state, const char* registryKey, const char* valueName,
                     bool defaultDark = true);
void SetDarkMode(State& state, const char* registryKey, const char* valueName, bool dark);
void ReleaseState(State& state);
void RecreateResources(Resources& resources, bool dark);
void ReleaseResources(Resources& resources);
void ApplyNativeMenuTheme(bool dark);
void ApplyWindowTheme(HWND hWnd, bool dark, Resources& resources);
void ApplyWindowTheme(HWND hWnd, State& state);
void DrawPanel(HDC hdc, const RECT& panel, const char* title,
               bool dark, HFONT sectionFont);
void DrawButton(const DRAWITEMSTRUCT* draw, bool dark, HFONT font, bool accent);
}
