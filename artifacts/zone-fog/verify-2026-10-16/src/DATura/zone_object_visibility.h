#pragma once

#include <windows.h>

#include <map>
#include <string>
#include <vector>

#include "noesis_rapi.h"
#include "zone_object_transform.h"
#include "zone_render_frustum.h"

namespace ZoneObjectVisibility
{
using GetMapObjectDisplayNameFn = const char* (*)(int mapObjectIndex);

struct RenderContext
{
    const std::vector<std::string> *hiddenNames = nullptr;
    bool viewerPointValid = false;
    const float* lodViewerPoint = nullptr; // player/orbit target in DAT coordinates
    const std::vector<unsigned char> *visibleMapObjects = nullptr;
    const std::map<std::string, ZoneObjectTransform::DebugTransform> *overrides = nullptr;
    const ZoneRenderFrustum::Data *frustum = nullptr;
};

void SetHidden(std::vector<std::string>& hiddenNames, const char* name, bool hidden);
bool IsHidden(const std::vector<std::string>& hiddenNames, const char* name);
bool PassesRenderVisibility(const noesisModel_t *model, const noesisModel_t::Submesh& submesh,
                            bool filterZoneObjects, const RenderContext& context);
void SetListVisibility(HWND list, bool visible, std::vector<std::string>& hiddenNames,
                       bool& isPopulatingList, GetMapObjectDisplayNameFn getDisplayName);
void ShowOnlySelectedMapObject(HWND placedList, HWND unreferencedList, int selectedMapObjectIndex,
                               int mapObjectCount, std::vector<std::string>& hiddenNames,
                               bool& isPopulatingList, GetMapObjectDisplayNameFn getDisplayName);
}
