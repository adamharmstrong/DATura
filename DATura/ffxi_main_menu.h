#pragma once

#include <windows.h>

struct IDirect3DDevice9;
struct noesisModel_t;

namespace FFXIMainMenu
{
struct State
{
    bool open = false;
    int page = 0;
    int selected = 0;
};

enum class Action { None, Opened, Closed, Activated };

Action HandleKey(State& state, unsigned int key);
void Draw(HDC dc, const State& state, int width, int height, bool showMogHouse);
void DrawTextures(IDirect3DDevice9* device, bool enableMipMapping,
                  noesisModel_t* uiModel, const State& state,
                  int width, int height, bool showMogHouse);
bool ClickTab(State& state, HDC dc, int width, int height, POINT point);
}
