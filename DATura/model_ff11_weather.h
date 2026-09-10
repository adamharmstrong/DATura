#pragma once

// Included after the effect/texture handlers. The common DAT is read without
// running its full parser, which would replace the current zone's metadata.
static std::vector<noesisModel_t::WeatherSprite> Model_FF11_LoadWeatherSprites(
    noeRAPI_t *rapi, CFFXIDefaultHandlerSet &handlers)
{
    std::vector<noesisModel_t::WeatherSprite> sprites;
    if (!gpFF11Opts || !gpFF11Opts->renderEnvironment) return sprites;
    std::set<std::string> wanted;
    for (const auto &g : gFF11LastGeneratorRecords)
        if (Model_FF11_IsWeatherDirectory(g.directoryPath) && g.linkedDataType == 0x0e &&
            (g.moreFlags & 0x20) && (g.generatorFlags & 0x10)) wanted.insert(g.linkedResource);
    if (wanted.empty()) return sprites;
    auto collect = [&](const CFFXIEffectHandler &effects) {
        for (const auto &mesh : effects.EffectMeshes())
        {
            if (!wanted.count(mesh.mChunkName)) continue;
            noesisModel_t::WeatherSprite sprite;
            sprite.resourceName = mesh.mChunkName;
            sprite.directoryPath = mesh.mDirectoryPath;
            char scoped[128] = {};
            Model_FF11_BuildScopedResourceName(scoped, sizeof(scoped), mesh.mDirectoryPath.c_str(), mesh.mMaterialName);
            sprite.textureName = scoped;
            for (int offset : mesh.mCardVertexOffsets)
                for (int i = 0; i < 6; ++i)
                {
                    const unsigned char *v = mesh.mpData + offset + i * 24;
                    FFXIVertex vertex = {};
                    memcpy(vertex.pos, v, 12);
                    vertex.diffuse = D3DCOLOR_ARGB(v[15], v[12], v[13], v[14]);
                    memcpy(vertex.uv, v + 16, 8);
                    sprite.vertices.push_back(vertex);
                }
            if (!sprite.vertices.empty()) sprites.push_back(std::move(sprite));
        }
    };
    collect(*handlers.EffectAnimatedHandler());
    auto root = std::filesystem::path(rapi->GetCurrentFilePath()).parent_path();
    while (!root.empty() && root != root.root_path())
    {
        const auto name = root.filename().string();
        if (name.size() >= 3 && _strnicmp(name.c_str(), "ROM", 3) == 0) break;
        root = root.parent_path();
    }
    if (root.empty() || root == root.root_path()) return sprites;
    const auto commonFile = root.parent_path() / "ROM/0/0.DAT";
    BYTE *bytes = nullptr; DWORD size = 0;
    if (!FFXIFileIO::ReadWholeFile(commonFile.string().c_str(), &bytes, &size)) return sprites;
    std::unique_ptr<BYTE[]> storage(bytes);
    CFFXIDat common(bytes, (int)size, rapi);
    CFFXIEffectHandler effects(CFFXIDat::skChunkType_EffectAnimated);
    auto visit = [&](auto callback) {
        for (size_t offset = 0; offset + 16 <= size; )
        {
            CFFXIDat::SChunk chunk(bytes + offset, (int)offset);
            if (chunk.mSize < 16 || (size_t)chunk.mSize > size - offset) break;
            chunk.mDataOffset = -1;
            callback(chunk, bytes + offset + 16, chunk.mSize - 16);
            offset += chunk.mSize;
        }
    };
    visit([&](const auto &chunk, const unsigned char *data, int length) {
        if (chunk.mType == CFFXIDat::skChunkType_EffectAnimated && wanted.count(chunk.mName))
            effects.HandleChunk(common, chunk, data, length);
    });
    collect(effects);
    visit([&](const auto &chunk, const unsigned char *data, int length) {
        if (chunk.mType != CFFXIDat::skChunkType_Texture || length < 57) return;
        for (const auto &mesh : effects.EffectMeshes())
            if (!memcmp(data + 1, mesh.mMaterialName, 16))
            {
                auto *textures = handlers.TextureHandler();
                bool present = false;
                for (int i = 0; i < textures->Textures().Num(); ++i)
                    if (!strcmp(textures->Textures()[i]->name, mesh.mMaterialName)) present = true;
                if (!present) textures->HandleChunk(common, chunk, data, length);
                break;
            }
    });
    return sprites;
}
