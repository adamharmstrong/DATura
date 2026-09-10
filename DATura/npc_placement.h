#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include <d3d9.h>

namespace FFXINpcPlacement
{
    // Appearance data arrives independently from an NPC's world transform. Keep
    // the original payload until the character/model resolver has enough
    // information to turn it into one or more DATs.
    enum class AppearanceKind : std::uint8_t
    {
        Unknown,
        HumanoidLook,
        ModelId,
        DatPath,
    };

    struct Appearance
    {
        AppearanceKind kind = AppearanceKind::Unknown;
        std::uint32_t modelId = 0;
        std::string datPath;
        std::vector<std::uint8_t> rawLook;
    };

    struct Transform
    {
        // X/Z are horizontal; Y increases downward. Catalog heading zero faces
        // +X, with positive rotation turning toward -Z.
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float headingRadians = 0.0f;
        float scale = 1.0f;
    };

    struct Placement
    {
        std::uint32_t entityId = 0;
        std::uint16_t entityIndex = 0;
        std::string name;
        std::string roleTitle; // Unofficial, reviewed role from npc_roles.csv.
        std::string dialogue; // Optional UTF-8 dialogue supplied by catalog or entity updates.
        std::string starterQuestName; // Static fresh-character quest offer, if reviewed.
        Transform transform;
        Appearance appearance;
        bool visible = true;
        bool starterQuestAvailable = false; // Static fresh-character catalog; not quest progress.
    };

    // Starts a fresh placement set for a successfully loaded zone.
    void ResetForZone(int zoneId);
    void Clear();

    int ZoneId();
    std::size_t Count();
    std::vector<Placement> Snapshot();
    // Returns the current authoritative placement for an entity, if present.
    bool Find(std::uint32_t entityId, Placement &placement);

    // Appearance helpers shared by catalog parsing and the renderer's model
    // cache. A missing pair of bytes decodes as zero.
    std::uint16_t LookU16(const std::vector<std::uint8_t> &look, std::size_t offset);
    std::string AppearanceKey(const Appearance &appearance);

    // Loads the normally visible retail-style NPC set for zoneId from the
    // catalog copied beside DATura.exe. An explicit path is useful for tools.
    // Verified titles load from npc_roles.csv beside that catalog unless overridden.
    bool LoadCatalogForZone(int zoneId, const char *catalogPath = nullptr,
                            const char *roleCatalogPath = nullptr,
                            const char *questCatalogPath = nullptr);

    // Entity IDs are the stable key used by retail entity updates. Upsert
    // replaces an existing entity while preserving registry order.
    bool Upsert(const Placement &placement);
    bool Remove(std::uint32_t entityId);

    // Builds a row-vector Direct3D world transform from a placement. Model-
    // specific forward-axis corrections belong in the appearance renderer.
    D3DMATRIX BuildWorldTransform(const Transform &transform);
    Transform ToSceneTransform(const Transform &transform, bool mirrorX);
}
