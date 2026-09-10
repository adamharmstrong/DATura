#include "stdafx.h"
#include "zone_object_transform_editor.h"

#include "noesis_rapi.h"
#include "model_ff11.h"

#include <cstdlib>

namespace ZoneObjectTransformEditor
{
void PopulateMapObjectFields(
    HWND* edits, const int editCount, const int mapObjectIndex,
    const std::map<std::string, ZoneObjectTransform::DebugTransform>& overrides)
{
    if (!edits || mapObjectIndex < 0)
        return;

    const char* objectName = Model_FF11_GetLastMapObjectDisplayName(mapObjectIndex);
    float trans[3] = {};
    float rot[3] = {};
    float scale[3] = {};
    Model_FF11_GetLastMapObjectTransform(mapObjectIndex, trans, scale, rot);
    const std::map<std::string, ZoneObjectTransform::DebugTransform>::const_iterator overrideIt =
        overrides.find(objectName ? objectName : "");
    if (overrideIt != overrides.end())
    {
        memcpy(trans, overrideIt->second.trans, sizeof(trans));
        memcpy(rot, overrideIt->second.rot, sizeof(rot));
        memcpy(scale, overrideIt->second.scale, sizeof(scale));
    }

    char buffer[32];
    if (editCount > 0) { sprintf_s(buffer, "%.2f", trans[0]); SetWindowTextA(edits[0], buffer); }
    if (editCount > 1) { sprintf_s(buffer, "%.2f", trans[1]); SetWindowTextA(edits[1], buffer); }
    if (editCount > 2) { sprintf_s(buffer, "%.2f", trans[2]); SetWindowTextA(edits[2], buffer); }
    if (editCount > 3) { sprintf_s(buffer, "%.3f", rot[0]); SetWindowTextA(edits[3], buffer); }
    if (editCount > 4) { sprintf_s(buffer, "%.3f", rot[1]); SetWindowTextA(edits[4], buffer); }
    if (editCount > 5) { sprintf_s(buffer, "%.3f", rot[2]); SetWindowTextA(edits[5], buffer); }
    if (editCount > 6) { sprintf_s(buffer, "%.3f", scale[0]); SetWindowTextA(edits[6], buffer); }
    if (editCount > 7) { sprintf_s(buffer, "%.3f", scale[1]); SetWindowTextA(edits[7], buffer); }
    if (editCount > 8) { sprintf_s(buffer, "%.3f", scale[2]); SetWindowTextA(edits[8], buffer); }
}

ZoneObjectTransform::DebugTransform ReadFields(const HWND* edits, const int editCount)
{
    ZoneObjectTransform::DebugTransform transform = {};
    if (!edits)
        return transform;

    char buffer[64] = {};
    const int fieldCount = editCount < 9 ? editCount : 9;
    for (int index = 0; index < fieldCount; ++index)
    {
        GetWindowTextA(edits[index], buffer, sizeof(buffer));
        const float value = (float)atof(buffer);
        if (index < 3) transform.trans[index] = value;
        else if (index < 6) transform.rot[index - 3] = value;
        else transform.scale[index - 6] = value;
    }
    return transform;
}
}
