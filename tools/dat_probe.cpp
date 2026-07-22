#include <cstdio>
#include <vector>

#include "../DATura/stdafx.h"
#include "../DATura/noesis_rapi.h"
#include "../DATura/model_ff11.h"

static bool ReadFileBytes(const char* path, std::vector<BYTE>& bytes)
{
    FILE* f = nullptr;
    if (fopen_s(&f, path, "rb") != 0 || !f)
        return false;

    fseek(f, 0, SEEK_END);
    const long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size <= 0)
    {
        fclose(f);
        return false;
    }

    bytes.resize((size_t)size);
    const size_t read = fread(bytes.data(), 1, bytes.size(), f);
    fclose(f);
    return read == bytes.size();
}

static bool IsDATSet(const std::vector<BYTE>& bytes)
{
    const char* tag = NOESIS_FF11_DAT_SET;
    const size_t tagLen = std::strlen(tag);
    return bytes.size() >= tagLen && std::memcmp(bytes.data(), tag, tagLen) == 0;
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::fprintf(stderr, "usage: dat_probe <file.dat> [...]\n");
        return 2;
    }

    int failures = 0;
    for (int i = 1; i < argc; ++i)
    {
        const char* path = argv[i];
        std::vector<BYTE> bytes;
        if (!ReadFileBytes(path, bytes))
        {
            std::printf("%s | read=FAIL\n", path);
            ++failures;
            continue;
        }

        noeRAPI_t rapi(nullptr);
        rapi.SetCurrentFilePath(path);
        const bool isSet = IsDATSet(bytes);
        const bool check = isSet
            ? Model_FF11_CheckDATSet(bytes.data(), (int)bytes.size(), &rapi)
            : Model_FF11_CheckDAT(bytes.data(), (int)bytes.size(), &rapi);
        int numMdl = 0;
        noesisModel_t* model = check
            ? (isSet
                ? Model_FF11_LoadDATSet(bytes.data(), (int)bytes.size(), numMdl, &rapi)
                : Model_FF11_LoadDAT(bytes.data(), (int)bytes.size(), numMdl, &rapi))
            : nullptr;

        int verts = 0;
        int tris = 0;
        const int submeshes = model ? (int)model->submeshes.size() : 0;
        if (model)
        {
            for (const noesisModel_t::Submesh& sm : model->submeshes)
            {
                verts += sm.vertCount;
                tris += sm.triCount;
            }
        }

        const int mats = (model && model->pMatData) ? model->pMatData->matCount : 0;
        const int tex = (model && model->pMatData) ? model->pMatData->texCount : 0;
        const int bones = model ? model->boneCount : 0;
        std::printf("%s | check=%s load=%s numMdl=%d submeshes=%d verts=%d tris=%d mats=%d tex=%d bones=%d\n",
            path, check ? "OK" : "FAIL", model ? "OK" : "FAIL", numMdl,
            submeshes, verts, tris, mats, tex, bones);

        if (!check || !model || verts == 0 || tris == 0)
            ++failures;
    }

    return failures == 0 ? 0 : 1;
}
