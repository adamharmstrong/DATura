#pragma once

#include <windows.h>

namespace FFXIPlayerCustomizationCatalog
{
struct AnimationCategory
{
    const char *label;
    int slot;
    const char *prefix;
};

int AnimationCategoryCount();
const AnimationCategory& AnimationCategoryForIndex(int categoryIndex);
bool AnimationCategoryMatches(const char *label, const char *prefix);

void FillFromLabels(HWND comboBox, const char *const *labels, int count, int selected);
void FillCatalogVariant(HWND comboBox, const char *noneLabel, const char *itemPrefix,
                        int itemCount, int selected, int raceIndex, int slot);
void FillCatalogSparse(HWND comboBox, const char *noneLabel,
                       int selected, int raceIndex, int slot);
void FillCatalogBaseVariant(HWND comboBox, const char *itemPrefix,
                            int itemCount, int selected, int raceIndex, int slot);
void FillAnimationCategory(HWND comboBox, int selected);
void FillAnimation(HWND comboBox, int selected, int raceIndex, int categoryIndex);
void FillFaceVariants(HWND comboBox, int selected, int faceCount);
}
