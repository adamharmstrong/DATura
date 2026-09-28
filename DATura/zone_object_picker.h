#pragma once

#include "zone_object_visibility.h"

#include <d3d9.h>

#include <string>

struct noesisModel_t;

namespace ZoneObjectPicker
{
struct Hit
{
    std::string objectName;
    float depth = 1.0f;
};

Hit Pick(const noesisModel_t* model,
         const ZoneObjectVisibility::RenderContext& visibility,
         const D3DMATRIX& view, const D3DMATRIX& projection,
         int width, int height, float x, float y);
}
