#include "stdafx.h"
#include "noesis_rapi.h"
#include "model_ff11.h"
#include "zone_room_catalog.h"
#include "zone_room_loader.h"
#include "ffxi_file_io.h"

#include <memory>
#include <utility>
#include <set>

namespace
{
struct ScopedRoomOptions
{
    ff11Opts_t* previous = gpFF11Opts;
    ff11Opts_t options = previous ? *previous : ff11Opts_t{};
    ScopedRoomOptions()
    {
        options.renderEnvironment = false;
        options.renderUnreferenced = false;
        options.renderEffectMeshes = false;
        gpFF11Opts = &options;
    }
    ~ScopedRoomOptions() { gpFF11Opts = previous; }
};
// Room parsing has its own environment, MZB header and visibility tree. Keep
// the outdoor zone's state authoritative; rooms use ordinary frustum culling.
struct ZoneMetadata
{
    decltype(gFF11LastDatChunks) chunks = gFF11LastDatChunks;
    decltype(gFF11LastEnvironmentRecords) environment = gFF11LastEnvironmentRecords;
    decltype(gFF11LastGeneratorRecords) generators = gFF11LastGeneratorRecords;
    decltype(gFF11LastKeyframeRecords) keyframes = gFF11LastKeyframeRecords;
    decltype(gFF11LastMapHeader) header = gFF11LastMapHeader;
    decltype(gFF11LastZoneVisibilityLeaves) leaves = gFF11LastZoneVisibilityLeaves;
    decltype(gFF11LastZoneVisibilityRecords) records = gFF11LastZoneVisibilityRecords;
    decltype(gFF11LastZoneVisibilityTables) tables = gFF11LastZoneVisibilityTables;
    decltype(gFF11LastMapObjects) objects = gFF11LastMapObjects;
    decltype(gFF11LastMapGeoDrawBatches) batches = gFF11LastMapGeoDrawBatches;

    ~ZoneMetadata()
    {
        gFF11LastDatChunks = std::move(chunks);
        gFF11LastEnvironmentRecords = std::move(environment);
        gFF11LastGeneratorRecords = std::move(generators);
        gFF11LastKeyframeRecords = std::move(keyframes);
        gFF11LastMapHeader = header;
        gFF11LastZoneVisibilityLeaves = std::move(leaves);
        gFF11LastZoneVisibilityRecords = std::move(records);
        gFF11LastZoneVisibilityTables = std::move(tables);
        gFF11LastMapObjects = std::move(objects);
        gFF11LastMapGeoDrawBatches = std::move(batches);
    }
};

template<class T>
void AppendPointers(T**& destination, int& count, T** source, int sourceCount)
{
    if (!sourceCount) return;
    T** combined = new T*[count + sourceCount];
    for (int i = 0; i < count; ++i) combined[i] = destination[i];
    for (int i = 0; i < sourceCount; ++i) combined[count + i] = source[i];
    delete[] destination;
    destination = combined;
    count += sourceCount;
}

void Merge(noesisModel_t* zone, noesisModel_t* room, noeRAPI_t* rapi,
           const std::string& prefix, const std::vector<std::string>& objectNames)
{
    if (!zone->pMatData) zone->pMatData = new noesisMatData_t();
    if (room->pMatData)
    {
        const int textureOffset = zone->pMatData->texCount;
        for (int i = 0; i < room->pMatData->matCount; ++i)
        {
            auto* material = room->pMatData->mats[i];
            material->name = rapi->Noesis_PooledString((prefix + (material->name ? material->name : "")).c_str());
            if (material->texIdx >= 0) material->texIdx += textureOffset;
            if (material->normalTexIdx >= 0) material->normalTexIdx += textureOffset;
            if (material->specularTexIdx >= 0) material->specularTexIdx += textureOffset;
        }
        AppendPointers(zone->pMatData->mats, zone->pMatData->matCount,
                       room->pMatData->mats, room->pMatData->matCount);
        AppendPointers(zone->pMatData->textures, zone->pMatData->texCount,
                       room->pMatData->textures, room->pMatData->texCount);
    }
    // Transfer GPU buffers as well as CPU geometry. The empty room shell stays
    // in the RAPI pool and owns its material pointer arrays until scene unload.
    const int bufferOffset = static_cast<int>(zone->staticBufferGroups.size());
    for (auto& mesh : room->submeshes)
    {
        for (size_t i = 0; i < gFF11LastMapObjects.size(); ++i)
            if (mesh.objectName == gFF11LastMapObjects[i].displayName)
            {
                mesh.objectName = objectNames[i];
                break;
            }
        // A room may reference a texture supplied by the outdoor zone.
        // Prefer its own material when present, otherwise retain that lookup.
        const std::string localMaterial = prefix + mesh.materialName;
        if (zone->pMatData->FindMaterial(localMaterial.c_str()))
            mesh.materialName = localMaterial;
        if (mesh.staticBufferGroupIndex >= 0) mesh.staticBufferGroupIndex += bufferOffset;
        zone->submeshes.push_back(std::move(mesh));
    }
    zone->staticBufferGroups.insert(zone->staticBufferGroups.end(),
        room->staticBufferGroups.begin(), room->staticBufferGroups.end());
    room->submeshes.clear();
    room->staticBufferGroups.clear();
    zone->renderMetadataPrepared = false;
    zone->hasZoneLod = zone->hasZoneLod || room->hasZoneLod;
}
}

int ZoneRoomLoader::Append(noesisModel_t* zone, noeRAPI_t* rapi, const char* zonePath)
{
    if (!zone || !rapi || !zonePath || !gFF11LastMapHeader.valid) return 0;
    const auto rooms = ZoneRoomCatalog::Resolve(zonePath);
    if (rooms.empty()) return 0;
    ZoneMetadata metadata;
    std::set<std::string> replacedObjects;
    int loaded = 0;
    for (const auto& entry : rooms)
    {
        BYTE* raw = nullptr;
        DWORD size = 0;
        if (!FFXIFileIO::ReadWholeFile(entry.path.c_str(), &raw, &size))
        {
            rapi->LogOutput("Room DAT unavailable: %s\n", entry.path.c_str());
            continue;
        }
        std::unique_ptr<BYTE[]> bytes(raw);
        if (size < 16 || size > INT_MAX || memcmp(bytes.get(), entry.rootName.data(), 4) != 0 ||
            (bytes[4] & 0x7F) != CFFXIDat::skChunkType_DirectoryOpen)
        {
            rapi->LogOutput("Ignoring unexpected room DAT: %s\n", entry.path.c_str());
            continue;
        }
        const auto triangleCount = gFF11LastCollisionTriangles.size();
        const auto collisionMeshCount = gFF11LastCollisionMeshes.size();
        int count = 0;
        noesisModel_t* room = nullptr;
        {
            ScopedRoomOptions options;
            if (Model_FF11_CheckDAT(bytes.get(), static_cast<int>(size), rapi))
                room = Model_FF11_LoadDAT(bytes.get(), static_cast<int>(size), count, rapi);
        }
        if (!room || room->submeshes.empty())
        {
            gFF11LastCollisionTriangles.resize(triangleCount);
            gFF11LastCollisionMeshes.resize(collisionMeshCount);
            rapi->LogOutput("Room DAT has no geometry: %s\n", entry.path.c_str());
            continue;
        }
        const auto file = entry.path.substr(entry.path.find_last_of("/\\") + 1);
        const std::string prefix = "room/" + file + "/";
        const int objectOffset = static_cast<int>(metadata.objects.size());
        int geoOffset = 0;
        for (const auto& batch : metadata.batches)
            geoOffset = std::max(geoOffset, batch.mapGeoIndex + 1);
        std::vector<std::string> objectNames;
        for (size_t i = 0; i < gFF11LastMapObjects.size(); ++i)
        {
            auto object = gFF11LastMapObjects[i];
            object.roomObject = true;
            if (object.mapGeoIndex >= 0) object.mapGeoIndex += geoOffset;
            object.mapRecordIndex = objectOffset + static_cast<int>(i);
            sprintf_s(object.displayName, "%03d: room %s / %s", object.mapRecordIndex,
                      file.c_str(), object.objectName);
            objectNames.push_back(object.displayName);
            metadata.objects.push_back(object);
        }
        for (auto batch : gFF11LastMapGeoDrawBatches)
        {
            if (batch.mapRecordIndex >= 0) batch.mapRecordIndex += objectOffset;
            if (batch.mapGeoIndex >= 0) batch.mapGeoIndex += geoOffset;
            const std::string display = "room " + file + " / " + batch.displayName;
            strncpy_s(batch.displayName, display.c_str(), _TRUNCATE);
            const std::string material = prefix + batch.materialName;
            strncpy_s(batch.materialName, material.c_str(), _TRUNCATE);
            metadata.batches.push_back(batch);
        }
        for (size_t i = collisionMeshCount; i < gFF11LastCollisionMeshes.size(); ++i)
        {
            auto& collision = gFF11LastCollisionMeshes[i];
            const std::string display = "room " + file + " / " + collision.displayName;
            strncpy_s(collision.displayName, display.c_str(), _TRUNCATE);
        }
        Merge(zone, room, rapi, prefix, objectNames);
        for (auto& object : metadata.objects)
        {
            // MZB placement +0x50 is a sub-area ID, not a VTABLE file ID.
            // Only retire its outdoor proxy after that exact room loaded.
            if (!object.roomObject && object.data2[3] == entry.subAreaId)
            {
                object.replacedByRoom = true;
                replacedObjects.insert(object.displayName);
            }
        }
        ++loaded;
        rapi->LogOutput("Loaded room DAT: %s\n", entry.path.c_str());
    }
    if (!replacedObjects.empty())
    {
        const size_t before = zone->submeshes.size();
        // Shared buffers may contain both retained and replaced objects. Release
        // them before erasing CPU vectors, then rebuild from the surviving meshes.
        zone->ReleaseD3DBuffers();
        std::vector<bool> discarded(gFF11LastCollisionTriangles.size(), false);
        for (const auto& object : metadata.objects)
            if (object.replacedByRoom)
                for (size_t i = object.visualCollisionStart;
                     i < object.visualCollisionStart + object.visualCollisionCount && i < discarded.size(); ++i)
                    discarded[i] = true;
        std::vector<size_t> kept(discarded.size() + 1, 0);
        for (size_t i = 0; i < discarded.size(); ++i) kept[i + 1] = kept[i] + !discarded[i];
        for (auto& collision : gFF11LastCollisionMeshes)
        {
            const size_t start = static_cast<size_t>(collision.triStart);
            const size_t end = start + collision.triCount;
            if (end <= discarded.size())
            {
                collision.triStart = static_cast<int>(kept[start]);
                collision.triCount = static_cast<int>(kept[end] - kept[start]);
            }
        }
        for (auto& object : metadata.objects)
        {
            const size_t end = object.visualCollisionStart + object.visualCollisionCount;
            if (end <= discarded.size())
            {
                object.visualCollisionCount = kept[end] - kept[object.visualCollisionStart];
                object.visualCollisionStart = kept[object.visualCollisionStart];
            }
        }
        std::vector<ff11CollisionTriangle_t> collision;
        collision.reserve(kept.back());
        for (size_t i = 0; i < discarded.size(); ++i)
            if (!discarded[i]) collision.push_back(gFF11LastCollisionTriangles[i]);
        gFF11LastCollisionTriangles.swap(collision);
        std::erase_if(zone->submeshes, [&](const auto& mesh) {
            return replacedObjects.count(mesh.objectName) != 0;
        });
        std::erase_if(metadata.batches, [&](const auto& batch) {
            return batch.mapRecordIndex >= 0 &&
                static_cast<size_t>(batch.mapRecordIndex) < metadata.objects.size() &&
                metadata.objects[batch.mapRecordIndex].replacedByRoom;
        });
        zone->opaqueSubmeshOrder.clear();
        zone->softBlendSubmeshOrder.clear();
        zone->renderMetadataPrepared = false;
        zone->BuildD3DBuffers(rapi->GetDevice());
        rapi->LogOutput("Released %zu replaced room-proxy meshes from CPU/GPU memory.\n",
                        before - zone->submeshes.size());
    }
    return loaded;
}
