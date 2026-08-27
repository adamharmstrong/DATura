#include "stdafx.h"
#include "npc_placement.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <mutex>
#include <sstream>

namespace
{
    std::mutex g_placementMutex;
    int g_zoneId = -1;
    std::vector<FFXINpcPlacement::Placement> g_placements;

    constexpr float kTwoPi = 6.28318530717958647692f;

    bool IsFiniteTransform(const FFXINpcPlacement::Transform &transform)
    {
        return std::isfinite(transform.x) &&
               std::isfinite(transform.y) &&
               std::isfinite(transform.z) &&
               std::isfinite(transform.headingRadians) &&
               std::isfinite(transform.scale) &&
               transform.scale > 0.0f;
    }

    float NormalizeHeading(float heading)
    {
        heading = std::fmod(heading, kTwoPi);
        return heading < 0.0f ? heading + kTwoPi : heading;
    }

    std::vector<std::string> ParseCsvRow(const std::string &line)
    {
        std::vector<std::string> fields;
        std::string field;
        bool quoted = false;
        for (std::size_t i = 0; i < line.size(); ++i)
        {
            const char c = line[i];
            if (c == '"')
            {
                if (quoted && i + 1 < line.size() && line[i + 1] == '"')
                {
                    field.push_back('"');
                    ++i;
                }
                else
                    quoted = !quoted;
            }
            else if (c == ',' && !quoted)
            {
                fields.push_back(field);
                field.clear();
            }
            else
                field.push_back(c);
        }
        fields.push_back(field);
        return fields;
    }

    bool DecodeHexLook(const std::string &text, std::vector<std::uint8_t> &look)
    {
        if (text.size() != 40)
            return false;
        look.clear();
        look.reserve(20);
        for (std::size_t i = 0; i < text.size(); i += 2)
        {
            char digits[3] = { text[i], text[i + 1], 0 };
            char *end = nullptr;
            const unsigned long value = std::strtoul(digits, &end, 16);
            if (!end || *end != 0)
                return false;
            look.push_back(static_cast<std::uint8_t>(value));
        }
        return true;
    }

    std::string DefaultCatalogPath()
    {
        char exePath[MAX_PATH] = {};
        if (!GetModuleFileNameA(nullptr, exePath, MAX_PATH))
            return "npc_placements.csv";
        char *slash = std::strrchr(exePath, '\\');
        if (!slash)
            slash = std::strrchr(exePath, '/');
        if (slash)
            slash[1] = 0;
        else
            exePath[0] = 0;
        return std::string(exePath) + "npc_placements.csv";
    }
}

namespace FFXINpcPlacement
{
    void ResetForZone(int zoneId)
    {
        std::lock_guard<std::mutex> lock(g_placementMutex);
        g_zoneId = zoneId;
        g_placements.clear();
    }

    void Clear()
    {
        ResetForZone(-1);
    }

    int ZoneId()
    {
        std::lock_guard<std::mutex> lock(g_placementMutex);
        return g_zoneId;
    }

    std::size_t Count()
    {
        std::lock_guard<std::mutex> lock(g_placementMutex);
        return g_placements.size();
    }

    std::vector<Placement> Snapshot()
    {
        std::lock_guard<std::mutex> lock(g_placementMutex);
        return g_placements;
    }

    std::uint16_t LookU16(const std::vector<std::uint8_t> &look, std::size_t offset)
    {
        if (offset + 1 >= look.size())
            return 0;
        return static_cast<std::uint16_t>(look[offset]) |
               (static_cast<std::uint16_t>(look[offset + 1]) << 8);
    }

    std::string AppearanceKey(const Appearance &appearance)
    {
        if (appearance.kind == AppearanceKind::ModelId)
            return "model:" + std::to_string(appearance.modelId);

        static const char digits[] = "0123456789ABCDEF";
        std::string key("look:");
        key.reserve(5 + appearance.rawLook.size() * 2);
        for (std::uint8_t value : appearance.rawLook)
        {
            key.push_back(digits[value >> 4]);
            key.push_back(digits[value & 15]);
        }
        return key;
    }

    bool LoadCatalogForZone(int zoneId, const char *catalogPath)
    {
        ResetForZone(zoneId);
        if (zoneId < 0)
            return false;

        const std::string path = (catalogPath && catalogPath[0]) ? catalogPath : DefaultCatalogPath();
        std::ifstream stream(path);
        if (!stream)
            return false;

        std::string line;
        while (std::getline(stream, line))
        {
            if (line.empty() || line[0] == '#' || line.compare(0, 7, "zone_id") == 0)
                continue;
            const std::vector<std::string> fields = ParseCsvRow(line);
            if (fields.size() != 10)
                continue;

            char *end = nullptr;
            const long rowZone = std::strtol(fields[0].c_str(), &end, 10);
            if (!end || *end != 0 || rowZone != zoneId)
                continue;

            Placement placement;
            placement.entityId = static_cast<std::uint32_t>(std::strtoul(fields[1].c_str(), nullptr, 10));
            placement.entityIndex = static_cast<std::uint16_t>(std::strtoul(fields[2].c_str(), nullptr, 10));
            placement.name = fields[3];
            const unsigned long rotation = std::strtoul(fields[4].c_str(), nullptr, 10);
            placement.transform.x = std::strtof(fields[5].c_str(), nullptr);
            placement.transform.y = std::strtof(fields[6].c_str(), nullptr);
            placement.transform.z = std::strtof(fields[7].c_str(), nullptr);
            placement.transform.headingRadians = static_cast<float>(rotation) * (kTwoPi / 256.0f);

            if (!DecodeHexLook(fields[9], placement.appearance.rawLook))
                continue;
            const std::uint16_t lookKind = LookU16(placement.appearance.rawLook, 0);
            if (lookKind == 0)
            {
                placement.appearance.kind = AppearanceKind::ModelId;
                placement.appearance.modelId = static_cast<std::uint32_t>(placement.appearance.rawLook[2]) |
                    (static_cast<std::uint32_t>(placement.appearance.rawLook[3]) << 8);
            }
            else if (lookKind == 1)
                placement.appearance.kind = AppearanceKind::HumanoidLook;
            else
                continue;
            Upsert(placement);
        }
        return Count() != 0;
    }

    bool Upsert(const Placement &placement)
    {
        if (placement.entityId == 0 || !IsFiniteTransform(placement.transform))
            return false;

        Placement normalized = placement;
        normalized.transform.headingRadians = NormalizeHeading(normalized.transform.headingRadians);

        std::lock_guard<std::mutex> lock(g_placementMutex);
        if (g_zoneId < 0)
            return false;

        const auto existing = std::find_if(
            g_placements.begin(), g_placements.end(),
            [entityId = normalized.entityId](const Placement &candidate)
            {
                return candidate.entityId == entityId;
            });

        if (existing != g_placements.end())
            *existing = std::move(normalized);
        else
            g_placements.push_back(std::move(normalized));
        return true;
    }

    bool Remove(std::uint32_t entityId)
    {
        if (entityId == 0)
            return false;

        std::lock_guard<std::mutex> lock(g_placementMutex);
        const auto existing = std::find_if(
            g_placements.begin(), g_placements.end(),
            [entityId](const Placement &candidate)
            {
                return candidate.entityId == entityId;
            });
        if (existing == g_placements.end())
            return false;

        g_placements.erase(existing);
        return true;
    }

    D3DMATRIX BuildWorldTransform(const Transform &transform)
    {
        D3DMATRIX world = {};
        const float heading = NormalizeHeading(transform.headingRadians);
        const float sine = std::sinf(heading);
        const float cosine = std::cosf(heading);

        world._11 = cosine * transform.scale;
        world._13 = -sine * transform.scale;
        world._22 = transform.scale;
        world._31 = sine * transform.scale;
        world._33 = cosine * transform.scale;
        world._41 = transform.x;
        world._42 = transform.y;
        world._43 = transform.z;
        world._44 = 1.0f;
        return world;
    }
}
