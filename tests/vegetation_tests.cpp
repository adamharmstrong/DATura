#include "stdafx.h"
#include "noesis_rapi.h"
#include "model_ff11.h"
#include "ffxi_file_io.h"
#include "d3d_model_buffers.h"
#include "zone_vegetation_animation.h"
#include "zone_model_transform.h"
#include <iostream>
#include <memory>
#include <limits>

namespace
{
int failures = 0;
void Check(bool ok, const char* message)
{
    if (!ok) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}
bool Near(float a, float b) { return std::abs(a - b) < 0.00001f; }

void TestLayouts()
{
    for (unsigned char config = 0; config < 4; ++config)
    {
        const bool blend = (config & 2) != 0, strip = (config & 1) != 0;
        const size_t stride = blend ? 48 : 36;
        std::vector<BYTE> bytes(16 + 116 + 4 * stride + 4 + (strip ? 8 : 12));
        bytes.resize((bytes.size() + 15) & ~size_t(15));
        auto write = [&](size_t offset, const auto& value) { memcpy(bytes.data() + offset, &value, sizeof(value)); };
        memcpy(bytes.data(), "test", 4);
        write(4, static_cast<unsigned int>((bytes.size() << 3) | 0x2e));
        bytes[16 + 4] = config;
        memcpy(bytes.data() + 16 + 16, "_kusa_test", 10);
        write(16 + 32, 1); write(16 + 60, 64); write(16 + 64, 1);
        // Deliberately unrelated to topology; only the config byte selects it.
        write(16 + 92, 0);
        memcpy(bytes.data() + 16 + 96, "test_texture", 12);
        write(16 + 112, 4u);
        for (int i = 0; i < 4; ++i)
        {
            const size_t offset = 16 + 116 + i * stride;
            write(offset, static_cast<float>(i & 1));
            write(offset + 4, static_cast<float>(i / 2));
            if (blend) write(offset + 12, i >= 2 ? 0.25f : 0.0f);
            write(offset + (blend ? 32 : 20), 1.0f);
            write(offset + (blend ? 36 : 24), 0x80808080u);
        }
        const size_t indices = 16 + 116 + 4 * stride;
        write(indices, static_cast<unsigned short>(strip ? 4 : 6));
        const unsigned short list[] = {0, 1, 2, 2, 1, 3}, stripIndices[] = {0, 1, 2, 3};
        memcpy(bytes.data() + indices + 4, strip ? stripIndices : list, strip ? 8 : 12);
        noeRAPI_t rapi(nullptr);
        ff11Opts_t options = {}; options.renderEnvironment = true;
        gpFF11Opts = &options;
        int count = 0;
        auto* model = Model_FF11_LoadDAT(bytes.data(), static_cast<int>(bytes.size()), count, &rapi);
        Check(model && model->submeshes.size() == 1, "decode each independent MMB topology/blend combination");
        if (model && !model->submeshes.empty())
        {
            const auto& sm = model->submeshes[0];
            Check(sm.triCount == 2 && sm.vertCount == 6, "list and strip both produce two valid triangles");
            Check(!sm.windDisplacements.empty() == blend, "only config bit 1 enables wind data");
        }
        gpFF11Opts = nullptr;
    }
}

void TestPipeline(IDirect3DDevice9* device)
{
    noeRAPI_t rapi(nullptr);
    rapi.rpgCreateContext();
    modelMatrix_t matrix = { { 0, 2, 0 }, { -3, 0, 0 }, { 0, 0, -4 }, { 10, 20, 30 } };
    rapi.rpgSetTransform(&matrix);
    float pos[] = { 1, 2, 3 }, wind[] = { 0.25f, -0.5f, 1 };
    rapi.rpgBegin(RPGEO_TRIANGLE);
    for (int i = 0; i < 3; ++i) rapi.rpgVertex3f(pos);
    rapi.rpgEnd();
    rapi.rpgBegin(RPGEO_TRIANGLE);
    rapi.rpgVertWind3f(wind);
    rapi.rpgVertex3f(pos);
    rapi.rpgVertex3f(pos); // pending wind must reset: root stays fixed
    const float invalid[] = { std::numeric_limits<float>::quiet_NaN(), 0, 0 };
    rapi.rpgVertWind3f(invalid);
    rapi.rpgVertex3f(pos);
    rapi.rpgEnd();
    auto* model = rapi.rpgConstructModel();
    Check(model && model->submeshes.size() == 1, "construct mixed static/moving triangles");
    if (!model || model->submeshes.empty()) return;
    auto& sm = model->submeshes[0];
    Check(sm.windDisplacements.size() == 6, "pad earlier static vertices when a moving triangle arrives");
    if (sm.windDisplacements.size() != 6) return;
    Check(sm.windDisplacements[0] == std::array<float, 3>{} &&
          sm.windDisplacements[4] == std::array<float, 3>{} &&
          sm.windDisplacements[5] == std::array<float, 3>{}, "roots, unset attributes and invalid vectors remain static");
    Check(sm.windDisplacements[3] == std::array<float, 3>{1.5f, 0.5f, -4.0f},
          "wind rotates/scales including reflection, without placement translation");
    Check(Near(sm.cpuVerts[3].pos[0], 4) && Near(sm.cpuVerts[3].pos[2], 18), "base position retains translation");
    model->UpdateSubmeshBounds();
    Check(Near(sm.boundsMin[2], 14) && Near(sm.boundsMax[0], 5.5f), "bounds include wind endpoints");
    if (!device) return;

    // Readable test buffers allow exact verification of the production upload path.
    // Include a leading sentinel vertex to exercise shared-buffer byte offsets.
    for (bool shared : { false, true })
    {
        IDirect3DVertexBuffer9* buffer = nullptr;
        const UINT offset = shared ? sizeof(FFXIVertex) : 0;
        Check(SUCCEEDED(device->CreateVertexBuffer(offset + 6 * sizeof(FFXIVertex), 0,
            FFXI_VERTEX_FVF, D3DPOOL_MANAGED, &buffer, nullptr)), "create validation vertex buffer");
        if (!buffer) continue;
        if (shared)
        {
            model->staticBufferGroups.emplace_back();
            model->staticBufferGroups.back().pVB = buffer;
            sm.staticBufferGroupIndex = 0;
            sm.staticVertexOffset = 1;
        }
        else sm.pVB = buffer;
        void* data = nullptr;
        buffer->Lock(0, 0, &data, 0);
        memset(data, 0x55, offset);
        memcpy(static_cast<char*>(data) + offset, sm.cpuVerts.data(), 6 * sizeof(FFXIVertex));
        buffer->Unlock();
        for (float factor : { 1.0f, 0.5f, 0.0f, 0.75f, 0.0f })
        {
            Check(D3DModelBuffers::UploadVegetation(model, sm, factor), "upload wind to D3D9");
            buffer->Lock(0, 0, &data, D3DLOCK_READONLY);
            const auto* vertices = reinterpret_cast<const FFXIVertex*>(static_cast<char*>(data) + offset);
            Check(Near(vertices[3].pos[0], 4 + 1.5f * factor) &&
                  Near(vertices[3].pos[2], 18 - 4 * factor), "GPU positions follow authored displacement without drift");
            Check(memcmp(&vertices[4], &sm.cpuVerts[4], sizeof(FFXIVertex)) == 0, "root GPU vertex remains identical");
            Check(vertices[3].diffuse == sm.cpuVerts[3].diffuse &&
                  Near(vertices[3].uv[0], sm.cpuVerts[3].uv[0]), "wind preserves material attributes");
            if (shared) Check(static_cast<unsigned char*>(data)[0] == 0x55, "shared neighbor not overwritten");
            buffer->Unlock();
        }
        model->ReleaseD3DBuffers();
    }
    model->BuildD3DBuffers(device);
    Check(D3DModelBuffers::UploadVegetation(model, sm, 1), "animate packed production buffer");
    model->ReleaseD3DBuffers();
    model->BuildD3DBuffers(device);
    Check(sm.uploadedWindWeight == 0 && D3DModelBuffers::UploadVegetation(model, sm, 1),
          "buffer recreation resets cached wind and reapplies animation");
    ZoneModelTransform::MirrorOnX(model, device);
    Check(Near(sm.cpuVerts[3].pos[0], -4) && Near(sm.windDisplacements[3][0], -1.5f),
          "zone coordinate-system mirror also mirrors wind");
    Check(D3DModelBuffers::UploadVegetation(model, sm, 1), "mirrored buffer animates after rebuild");
}

void TestDat(const char* path, const bool expectStatic)
{
    BYTE* raw = nullptr; DWORD size = 0;
    Check(FFXIFileIO::ReadWholeFile(path, &raw, &size), "read installed zone DAT");
    if (!raw) return;
    std::unique_ptr<BYTE[]> bytes(raw);
    noeRAPI_t rapi(nullptr);
    rapi.SetCurrentFilePath(path);
    ff11Opts_t options = {};
    options.renderEnvironment = true;
    gpFF11Opts = &options;
    int count = 0;
    auto* model = Model_FF11_LoadDAT(raw, static_cast<int>(size), count, &rapi);
    Check(model != nullptr, "load installed zone");
    size_t moving = 0, grass = 0, roots = 0;
    if (model)
    {
        model->UpdateSubmeshBounds();
        for (const auto& sm : model->submeshes)
        {
            if (sm.windDisplacements.empty()) continue;
            ++moving;
            Check(sm.windDisplacements.size() == sm.cpuVerts.size(), "DAT wind survives triangulation");
            if (sm.objectName.find("_kusa_") == std::string::npos) continue;
            ++grass;
            for (size_t i = 0; i < sm.windDisplacements.size(); ++i)
            {
                if (sm.windDisplacements[i] == std::array<float, 3>{}) ++roots;
                for (int axis = 0; axis < 3; ++axis)
                {
                    const float end = sm.cpuVerts[i].pos[axis] + sm.windDisplacements[i][axis];
                    Check(end >= sm.boundsMin[axis] && end <= sm.boundsMax[axis], "installed grass sway fits bounds");
                }
            }
        }
        std::cout << path << ": meshes=" << model->submeshes.size() << " wind=" << moving
                  << " grass=" << grass << " fixed roots=" << roots << '\n';
    }
    if (expectStatic) Check(moving == 0, "zone without authored wind stays static");
    else Check(moving > 0 && grass > 0 && roots > 0, "installed grass has animated tips and fixed roots");
    gpFF11Opts = nullptr;
}
}

int main(int argc, char** argv)
{
    using ZoneVegetationAnimation::Weight;
    Check(Weight(0, 2) == 0 && Weight(1, 0) == 0 && Weight(1, 2) == 1 && Weight(1, 4) == 0,
          "Off and Simple cycle endpoints");
    Check(Near(Weight(2, 2), 1) && Near(Weight(2, 4), 0) && Weight(2, 0.5) < Weight(1, 0.5),
          "Smooth eases into cycle endpoints");
    Check(Weight(2, std::numeric_limits<double>::infinity()) == 0, "nonfinite clock is safe");
    TestLayouts();
    HWND window = CreateWindowExA(0, "STATIC", "DATura vegetation tests", WS_POPUP,
        0, 0, 64, 64, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    IDirect3DDevice9* device = nullptr;
    D3DPRESENT_PARAMETERS pp = {};
    pp.Windowed = TRUE; pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.hDeviceWindow = window; pp.BackBufferWidth = 64; pp.BackBufferHeight = 64;
    if (d3d && window) d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &device);
    Check(device != nullptr, "create hidden D3D9 test device");
    TestPipeline(device);
    for (int i = 1; i < argc; ++i)
    {
        const bool expectStatic = std::string(argv[i]) == "--static";
        if (expectStatic && ++i >= argc) { Check(false, "--static requires a DAT path"); break; }
        TestDat(argv[i], expectStatic);
    }
    if (device) device->Release();
    if (d3d) d3d->Release();
    if (window) DestroyWindow(window);
    std::cout << (failures ? "FAIL" : "PASS") << ": vegetation checks\n";
    return failures ? 1 : 0;
}
