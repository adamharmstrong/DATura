#pragma once

#include "game_ui_config.h"

struct IDirect3DDevice9;
struct noesisModel_t;

namespace FFXINationSelectionRenderer
{
struct Context
{
    IDirect3DDevice9* device;
    HWND window;
    bool enableMipMapping;
    noesisModel_t* titleUiModel;
    const GameUiNationConfig& config;
    int selectedIndex;
};

void DrawTextures(const Context& context);
void DrawOverlay(const Context& context);
}
