#include "stdafx.h"
#include "zone_object_visibility.h"
#include "model_ff11.h"

#include "zone_object_list_selection.h"

#include <algorithm>
#include <commctrl.h>
#include <cstdlib>

namespace ZoneObjectVisibility
{
void SetHidden(std::vector<std::string>& hiddenNames, const char* name, const bool hidden)
{
    if (!name || !name[0])
        return;
    const std::vector<std::string>::iterator found =
        std::find(hiddenNames.begin(), hiddenNames.end(), name);
    if (hidden)
    {
        if (found == hiddenNames.end())
            hiddenNames.push_back(name);
    }
    else if (found != hiddenNames.end())
    {
        hiddenNames.erase(found);
    }
}

bool IsHidden(const std::vector<std::string>& hiddenNames, const char* name)
{
    return name && name[0] &&
           std::find(hiddenNames.begin(), hiddenNames.end(), name) != hiddenNames.end();
}

bool PassesRenderVisibility(const noesisModel_t *model, const noesisModel_t::Submesh& submesh,
                            const bool filterZoneObjects, const RenderContext& context)
{
    if (!filterZoneObjects)
        return true;
    if (!ZoneLod::Visible(submesh.zoneLod, context.lodViewerPoint))
        return false;
    if (!submesh.objectName.empty() && context.hiddenNames &&
        IsHidden(*context.hiddenNames, submesh.objectName.c_str()))
    {
        return false;
    }
    if (context.viewerPointValid && !submesh.objectName.empty() && context.visibleMapObjects)
    {
        char *end = nullptr;
        const unsigned long mapObjectIndex = std::strtoul(submesh.objectName.c_str(), &end, 10);
        if (end != submesh.objectName.c_str() && end && *end == ':' &&
            !(mapObjectIndex < gFF11LastMapObjects.size() &&
              gFF11LastMapObjects[mapObjectIndex].roomObject) &&
            mapObjectIndex < context.visibleMapObjects->size() &&
            !(*context.visibleMapObjects)[static_cast<size_t>(mapObjectIndex)])
        {
            return false;
        }
    }
    if (!submesh.objectName.empty() && context.overrides &&
        context.overrides->find(submesh.objectName) != context.overrides->end())
    {
        return true;
    }
    const bool staticZone = model && !model->staticBufferGroups.empty();
    return !staticZone || !context.frustum || ZoneRenderFrustum::IntersectsBounds(
        *context.frustum, submesh.boundsMin, submesh.boundsMax, submesh.hasBounds);
}

void SetListVisibility(const HWND list, const bool visible,
                       std::vector<std::string>& hiddenNames, bool& isPopulatingList,
                       const GetMapObjectDisplayNameFn getDisplayName)
{
    if (!list)
        return;

    const int rowCount = ListView_GetItemCount(list);
    isPopulatingList = true;
    for (int row = 0; row < rowCount; ++row)
    {
        const int mapObjectIndex = ZoneObjectListSelection::GetMapObjectIndex(list, row);
        SetHidden(hiddenNames, getDisplayName(mapObjectIndex), !visible);
        ListView_SetCheckState(list, row, visible ? TRUE : FALSE);
    }
    isPopulatingList = false;
}

void ShowOnlySelectedMapObject(
    const HWND placedList, const HWND unreferencedList, const int selectedMapObjectIndex,
    const int mapObjectCount, std::vector<std::string>& hiddenNames, bool& isPopulatingList,
    const GetMapObjectDisplayNameFn getDisplayName)
{
    hiddenNames.clear();
    for (int mapObjectIndex = 0; mapObjectIndex < mapObjectCount; ++mapObjectIndex)
    {
        if (mapObjectIndex != selectedMapObjectIndex)
            SetHidden(hiddenNames, getDisplayName(mapObjectIndex), true);
    }

    const HWND lists[2] = { placedList, unreferencedList };
    isPopulatingList = true;
    for (int listIndex = 0; listIndex < 2; ++listIndex)
    {
        const HWND list = lists[listIndex];
        if (!list)
            continue;
        const int rowCount = ListView_GetItemCount(list);
        for (int row = 0; row < rowCount; ++row)
        {
            ListView_SetCheckState(
                list, row,
                ZoneObjectListSelection::GetMapObjectIndex(list, row) == selectedMapObjectIndex);
        }
    }
    isPopulatingList = false;
}
}
