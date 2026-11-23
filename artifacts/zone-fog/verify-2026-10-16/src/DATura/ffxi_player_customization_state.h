#pragma once

namespace FFXIPlayerPreset
{
struct Data;
}

struct PlayerEquipState
{
    int raceIndex;
    int mainType;
    int mainItem;
    int subType;
    int subItem;
    int rangedType;
    int rangedItem;
    int headItem;
    int bodyItem;
    int handsItem;
    int legsItem;
    int feetItem;
    int animationBank;
    int animationMode;
    bool animationPlaying;
};

namespace FFXIPlayerCustomizationState
{
constexpr int ArmorVariantCount = 128;
constexpr int FaceVariantCount = 16;

int FaceVariantCountForRace(int raceIndex);
void Clamp(PlayerEquipState& equipment, int& faceVariant);
FFXIPlayerPreset::Data CapturePreset(const PlayerEquipState& equipment, int faceVariant);
void ApplyPreset(const FFXIPlayerPreset::Data& preset,
                 PlayerEquipState& equipment, int& faceVariant);
}
