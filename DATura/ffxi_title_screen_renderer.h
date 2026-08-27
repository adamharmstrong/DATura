#pragma once

#include "game_ui_config.h"

struct IDirect3DDevice9;
struct noesisModel_t;

namespace FFXITitleScreenRenderer
{
struct Context
{
    IDirect3DDevice9* device;
    HWND window;
    bool enableMipMapping;
    noesisModel_t* logoModel;
    noesisModel_t* logoMarkModel;
    noesisModel_t* titleUiModel;
    const GameUiTitleConfig& config;
    POINT mouseClient;
};

void DrawTextures(const Context& context);
void DrawOverlay(const Context& context);
}
