#include "stdafx.h"
#include "noesis_rapi.h"
#include "model_ff11.h"
#include "character_dat_table.h"
#include "ffxi_dat_set_builder.h"
#include "npc_render_geometry.h"
#include "npc_placement.h"
#include "npc_nameplate_renderer.h"
#include "model_renderer.h"
#include "zone_model_render_metadata.h"
#include "d3d_math.h"
#include <filesystem>
#include <fstream>
#include <iostream>

namespace
{
int failures = 0;
void Check(bool value, const char *message)
{
    if (!value) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}

bool BoneMoves(const noesisAnim_t &clip, int bone)
{
    const auto &first = clip.frameWorldMats[bone];
    for (int frame = 1; frame < clip.frameCount; ++frame)
        for (int row = 0; row < 4; ++row)
            for (int axis = 0; axis < 3; ++axis)
                if (fabsf(first[row][axis] - clip.frameWorldMats[frame * clip.boneCount + bone][row][axis]) > 0.001f)
                    return true;
    return false;
}

void TestNpcRoles()
{
    const auto folder = std::filesystem::temp_directory_path() /
        ("datura-npc-roles-" + std::to_string(GetCurrentProcessId()));
    std::filesystem::create_directories(folder);
    const auto placements = folder / "npc_placements.csv";
    const auto roles = folder / "npc_roles.csv";
    {
        std::ofstream file(placements);
        for (int id = 1; id <= 7; ++id)
            file << "234," << id << ",1,\"Twin, NPC\",0,0,0,0,0,0000010000000000000000000000000000000000\n";
        std::ofstream titles(roles);
        titles << "234,Mines,1,\"Twin, NPC\",\"Maps, Supplies\",verified,https://example.com,2026-09-06,\n"
               << "234,Mines,2,\"Twin, NPC\",Storage,verified,https://example.com,2026-09-06,\n"
               << "234,Mines,3,\"Twin, NPC\",Guessed,pending,https://example.com,,\n"
               << "235,Markets,4,\"Twin, NPC\",Wrong zone,verified,https://example.com,2026-09-06,\n"
               << "234,Mines,5,Old name,Stale,verified,https://example.com,2026-09-06,\n"
               << "234,Mines,6,\"Twin, NPC\",Unsourced,verified,,2026-09-06,\n"
               << "234,Mines,7,\"Twin, NPC\",Unused,not_applicable,https://example.com,,\n";
    }
    Check(FFXINpcPlacement::LoadCatalogForZone(234, placements.string().c_str()), "load adjacent NPC role catalog");
    const auto snapshot = FFXINpcPlacement::Snapshot();
    Check(snapshot.size() == 7, "load all fixture placements");
    if (snapshot.size() == 7)
    {
        Check(snapshot[0].roleTitle == "Maps, Supplies", "quoted title and NPC name round trip");
        Check(snapshot[1].roleTitle == "Storage", "namesakes have independent roles");
        for (size_t i = 2; i < snapshot.size(); ++i)
            Check(snapshot[i].roleTitle.empty(), "hide unverified, unsourced, stale and wrong-zone titles");
    }
    const auto missing = folder / "missing.csv";
    Check(FFXINpcPlacement::LoadCatalogForZone(234, placements.string().c_str(), missing.string().c_str()),
        "missing role catalog does not prevent placements loading");
    for (const auto &npc : FFXINpcPlacement::Snapshot())
        Check(npc.roleTitle.empty(), "reload clears prior titles");
    FFXINpcPlacement::Clear();
    std::filesystem::remove(placements);
    std::filesystem::remove(roles);
    std::filesystem::remove(folder);
}

void TestNameplates(IDirect3DDevice9 *device, const char *previewPath)
{
    std::vector<NpcNameplateRenderer::DrawItem> items = {
        {"Abd-al-Raziq", "Alchemy Guildmaster", 160, 100, 0.5f, 1},
        {"Isakoth", "Records of Eminence / Sparks", 480, 100, 0.5f, 1},
        {"> Shamarhaan <", "Puppetmaster Unlock & Training", 160, 220, 0.5f, 1},
        {"Unresearched NPC", "", 480, 220, 0.5f, 1},
        {"Same Name", "Maps", 160, 340, 0.5f, 1},
        {"Same Name", "Equipment Storage", 480, 340, 0.5f, 1}
    };
    device->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_XRGB(20, 28, 40), 1, 0);
    Check(SUCCEEDED(device->BeginScene()), "begin nameplate scene");
    NpcNameplateRenderer::Draw(device, items);
    device->EndScene();
    IDirect3DSurface9 *target = nullptr, *readback = nullptr;
    device->GetRenderTarget(0, &target);
    D3DSURFACE_DESC desc = {};
    if (target) target->GetDesc(&desc);
    if (target) device->CreateOffscreenPlainSurface(desc.Width, desc.Height, desc.Format,
        D3DPOOL_SYSTEMMEM, &readback, nullptr);
    const bool copied = target && readback && SUCCEEDED(device->GetRenderTargetData(target, readback));
    Check(copied, "read back rendered nameplates");
    D3DLOCKED_RECT locked = {};
    if (copied && SUCCEEDED(readback->LockRect(&locked, nullptr, D3DLOCK_READONLY)))
    {
        int green = 0, pale = 0;
        for (UINT y = 0; y < desc.Height; ++y)
            for (UINT x = 0; x < desc.Width; ++x)
            {
                DWORD pixel = reinterpret_cast<DWORD*>(static_cast<BYTE*>(locked.pBits) + y * locked.Pitch)[x];
                int r = (pixel >> 16) & 255, g = (pixel >> 8) & 255, b = pixel & 255;
                if (g > 150 && g > r + 30) ++green;
                if (r > 150 && b > g && g > r) ++pale;
            }
        Check(green > 100 && pale > 100, "render both green names and pale role text");
        if (previewPath)
        {
            BITMAPFILEHEADER header = {};
            BITMAPINFOHEADER info = {};
            info.biSize = sizeof(info); info.biWidth = desc.Width; info.biHeight = -(LONG)desc.Height;
            info.biPlanes = 1; info.biBitCount = 32; info.biCompression = BI_RGB;
            header.bfType = 0x4D42; header.bfOffBits = sizeof(header) + sizeof(info);
            header.bfSize = header.bfOffBits + desc.Width * desc.Height * 4;
            std::ofstream output(previewPath, std::ios::binary);
            output.write(reinterpret_cast<char*>(&header), sizeof(header));
            output.write(reinterpret_cast<char*>(&info), sizeof(info));
            for (UINT y = 0; y < desc.Height; ++y)
                output.write(static_cast<char*>(locked.pBits) + y * locked.Pitch, desc.Width * 4);
            Check(output.good(), "save nameplate preview");
        }
        readback->UnlockRect();
    }
    if (readback) readback->Release();
    if (target) target->Release();
    NpcNameplateRenderer::ClearCachedTextures();
}

void TestNpcDefaults()
{
    noesisModel_t model;
    noesisAnim_t walk, idle, relaxed;
    walk.filename = const_cast<char*>("wlk");
    idle.filename = const_cast<char*>("idl");
    relaxed.filename = const_cast<char*>("idl_relaxed");
    model.pAnim = &walk;
    model.animationClips = { &walk, &idle, &relaxed };
    Check(NpcRenderGeometry::SelectIdleAnimation(&model) && model.pAnim == &relaxed,
        "humanoid NPC replaces walking with neutral idle");
    model.animationClips = { &walk, &idle };
    Check(NpcRenderGeometry::SelectIdleAnimation(&model) && model.pAnim == &idle,
        "standalone NPC selects its idle clip");
    model.animationClips = { &walk };
    Check(!NpcRenderGeometry::SelectIdleAnimation(&model) && !model.pAnim,
        "missing idle never falls back to walking");

    // Catalog headings describe +X at zero and -Z at 64. Character-local
    // forward is +X, so the first world-matrix row must match that direction.
    for (int rotation = 0; rotation < 256; ++rotation)
    {
        FFXINpcPlacement::Transform placement;
        placement.x = 12; placement.y = -3; placement.z = 27;
        placement.headingRadians = rotation * (6.28318530718f / 256.0f);
        for (bool mirror : { false, true })
        {
            const auto scene = FFXINpcPlacement::ToSceneTransform(placement, mirror);
            const auto world = FFXINpcPlacement::BuildWorldTransform(scene);
            const float expectedX = std::cos(placement.headingRadians) * (mirror ? -1.0f : 1.0f);
            const float expectedZ = -std::sin(placement.headingRadians);
            Check(std::fabs(world._11 - expectedX) < 0.00001f &&
                  std::fabs(world._13 - expectedZ) < 0.00001f,
                "NPC facing follows catalog heading in either world orientation");
            Check(world._41 == (mirror ? -12.0f : 12.0f) && world._42 == -3 && world._43 == 27,
                "NPC heading conversion preserves placement coordinates");
        }
    }
}

void TestNpcModels(const char* root, IDirect3DDevice9* device, const char* previewPath)
{
    const auto catalog = std::filesystem::path(__FILE__).parent_path().parent_path() / "DATura/npc_placements.csv";
    Check(FFXINpcPlacement::LoadCatalogForZone(235, catalog.string().c_str()), "load reported NPC appearances");
    const auto placements = FFXINpcPlacement::Snapshot();
    device->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_XRGB(35, 40, 48), 1, 0);
    device->BeginScene();
    int column = 0;
    for (const char* name : {"Isakoth", "A.M.A.N. Reclaimer", "Igsli", "A.M.A.N. Validator"})
    {
        const auto found = std::find_if(placements.begin(), placements.end(),
            [&](const auto& npc) { return npc.name == name; });
        Check(found != placements.end(), "reported NPC exists in catalog");
        if (found == placements.end()) continue;
        char datSet[4096] = {};
        Check(FFXIDatSet::BuildNpc(root, found->appearance.rawLook, datSet, sizeof(datSet)), "resolve complete reported NPC DAT set");
        if (strcmp(name, "A.M.A.N. Reclaimer") == 0)
        {
            Check(strstr(datSet, "ROM/125/89.DAT") != nullptr, "Reclaimer body uses nonadjacent equipment mapping");
            Check(strstr(datSet, "ROM/125/97.DAT") != nullptr, "Reclaimer gloves use nonadjacent equipment mapping");
        }
        noeRAPI_t rapi(device);
        rapi.SetCurrentFilePath("NPC appearance regression.ff11datset");
        int count = 0;
        auto* model = Model_FF11_LoadDATSet(reinterpret_cast<BYTE*>(datSet), static_cast<int>(strlen(datSet)), count, &rapi);
        Check(model && count > 0, "load reported NPC model");
        if (!model) continue;
        Check(NpcRenderGeometry::SelectIdleAnimation(model), "reported NPC has idle animation");
        model->UpdateAnimation(0.3f, device);
        size_t vertices = 0;
        for (const auto& mesh : model->submeshes) vertices += mesh.cpuVerts.size();
        std::cout << name << ": " << vertices << " vertices\n";
        Check(vertices > 500, "reported NPC contains assembled geometry");
        ZoneModelRenderMetadata::Prepare(model, device);
        D3DVIEWPORT9 viewport = {static_cast<DWORD>(column++ * 160), 0, 160, 480, 0, 1};
        device->SetViewport(&viewport);
        const auto view = D3DMath::BuildLookAtLH(6.5f, -1.3f, 0.5f, 0, -1.3f, 0);
        const auto projection = D3DMath::BuildPerspectiveFovLH(0.65f, 1.0f / 3.0f, 0.1f, 100);
        const auto world = D3DMath::BuildIdentity();
        device->SetTransform(D3DTS_VIEW, &view);
        device->SetTransform(D3DTS_PROJECTION, &projection);
        ModelRenderer::Context context;
        context.device = device;
        ModelRenderer::PrepareFixedFunctionPass(context, world);
        ModelRenderer::DrawGeometry(context, model, world);
    }
    device->EndScene();
    D3DVIEWPORT9 viewport = {0, 0, 640, 480, 0, 1};
    device->SetViewport(&viewport);
    if (previewPath)
    {
        IDirect3DSurface9 *target = nullptr, *surface = nullptr;
        device->GetRenderTarget(0, &target);
        D3DSURFACE_DESC desc = {};
        target->GetDesc(&desc);
        device->CreateOffscreenPlainSurface(desc.Width, desc.Height, desc.Format, D3DPOOL_SYSTEMMEM, &surface, nullptr);
        if (surface && SUCCEEDED(device->GetRenderTargetData(target, surface)))
        {
            D3DLOCKED_RECT pixels = {};
            if (SUCCEEDED(surface->LockRect(&pixels, nullptr, D3DLOCK_READONLY)))
            {
                BITMAPFILEHEADER header = {};
                BITMAPINFOHEADER info = {};
                info.biSize = sizeof(info); info.biWidth = desc.Width; info.biHeight = -(LONG)desc.Height;
                info.biPlanes = 1; info.biBitCount = 32;
                header.bfType = 0x4D42; header.bfOffBits = sizeof(header) + sizeof(info);
                header.bfSize = header.bfOffBits + desc.Width * desc.Height * 4;
                std::ofstream output(std::string(previewPath) + ".models.bmp", std::ios::binary);
                output.write(reinterpret_cast<char*>(&header), sizeof(header));
                output.write(reinterpret_cast<char*>(&info), sizeof(info));
                for (UINT y = 0; y < desc.Height; ++y)
                    output.write(static_cast<char*>(pixels.pBits) + y * pixels.Pitch, desc.Width * 4);
                Check(output.good(), "save reported NPC model preview");
                surface->UnlockRect();
            }
        }
        if (surface) surface->Release();
        if (target) target->Release();
    }
    // All existing humanoid looks must resolve, including both expansion banks.
    for (int zone : {234, 235, 236, 237})
    {
        FFXINpcPlacement::LoadCatalogForZone(zone, catalog.string().c_str());
        for (const auto& npc : FFXINpcPlacement::Snapshot())
            if (npc.appearance.kind == FFXINpcPlacement::AppearanceKind::HumanoidLook &&
                npc.appearance.rawLook[3] >= 1 && npc.appearance.rawLook[3] <= 8)
            {
                char datSet[4096];
                const bool ok = FFXIDatSet::BuildNpc(root, npc.appearance.rawLook, datSet, sizeof(datSet));
                if (!ok) std::cerr << "Unresolved NPC: " << npc.name << '\n';
                Check(ok, "Bastok humanoid resolves every part");
            }
    }
    FFXINpcPlacement::Clear();
}

void TestRace(const char *root, int race, IDirect3DDevice9 *device)
{
    noeRAPI_t rapi(device);
    rapi.SetCurrentFilePath("animation regression.ff11datset");
    char datSet[4096] = {};
    FFXIDatSet::PlayerOptions options;
    options.raceIndex = race;
    std::string assetRoot(root);
    if (assetRoot.back() != '/' && assetRoot.back() != '\\') assetRoot += '/';
    Check(FFXIDatSet::BuildPlayer(assetRoot.c_str(), options, datSet, sizeof(datSet)), "build player DAT set");
    int count = 0;
    noesisModel_t *model = Model_FF11_LoadDATSet(reinterpret_cast<BYTE *>(datSet),
        static_cast<int>(strlen(datSet)), count, &rapi);
    Check(model && count > 0, "load player");
    if (!model) return;
    std::cout << kFFXICharRaces[race].name << ": " << model->animationClips.size() << " clips\n";
    if (race == 1)
        Check(model->FindAnimation("std") && model->FindAnimation("std")->frameCount == 51,
            "different-length upper/lower tracks still compose safely");
    for (const char *name : { "idl_relaxed", "wlk_relaxed", "run_relaxed", "mvb_relaxed", "mvl", "mvr", "jmp" })
    {
        noesisAnim_t *clip = model->FindAnimation(name);
        Check(clip != nullptr, name);
        if (!clip) continue;
        Check(model->FindAnimation("wlk0") && model->FindAnimation("wlk1"), "retain both source halves");
        // Hume male legs are bones 25..37; upper-body bones are 3..24.
        // These assertions catch loading only the bank's upper-body tracks.
        if (race == 0 && strcmp(name, "idl_relaxed") != 0 && strcmp(name, "jmp") != 0)
        {
            Check(BoneMoves(*clip, 30), "leg moves in composited clip");
            Check(BoneMoves(*clip, 10), "upper body moves in composited clip");
        }
        model->pAnim = clip;
        bool verticesMove = false;
        auto previous = model->submeshes;
        for (int frame = 0; frame < clip->frameCount; ++frame)
        {
            model->UpdateAnimation(static_cast<float>(frame) / clip->fps, device);
            bool bindPose = true;
            for (size_t mesh = 0; mesh < model->submeshes.size(); ++mesh)
            {
                const auto &sm = model->submeshes[mesh];
                for (size_t v = 0; v < sm.cpuVerts.size(); ++v)
                    for (int axis = 0; axis < 3; ++axis)
                    {
                        const float pos = sm.cpuVerts[v].pos[axis];
                        Check(std::isfinite(pos), "finite animated vertex");
                        if (fabsf(pos - sm.cpuBindVerts[v].pos[axis]) > 0.001f) bindPose = false;
                        if (fabsf(pos - previous[mesh].cpuVerts[v].pos[axis]) > 0.001f) verticesMove = true;
                    }
                if (sm.pVB && !sm.cpuVerts.empty())
                {
                    void *gpu = nullptr;
                    Check(SUCCEEDED(sm.pVB->Lock(0, 0, &gpu, D3DLOCK_READONLY)), "read animated buffer");
                    if (gpu)
                    {
                        Check(memcmp(gpu, sm.cpuVerts.data(), sm.cpuVerts.size() * sizeof(FFXIVertex)) == 0,
                            "render buffer matches posed vertices");
                        sm.pVB->Unlock();
                    }
                }
            }
            if (bindPose) std::cerr << "Rejected " << name << " frame " << frame << '\n';
            Check(!bindPose, "valid motion frame is not rejected to bind pose");
            previous = model->submeshes;
        }
        Check(verticesMove, "clip changes the rendered mesh");
        if (strcmp(name, "jmp") == 0)
        {
            Check(!clip->looping, "jump is a one-shot animation");
            const auto last = model->submeshes;
            model->UpdateAnimation(100.0f, device);
            for (size_t mesh = 0; mesh < last.size(); ++mesh)
                Check(memcmp(last[mesh].cpuVerts.data(), model->submeshes[mesh].cpuVerts.data(),
                    last[mesh].cpuVerts.size() * sizeof(FFXIVertex)) == 0, "jump holds its final pose rather than looping");
        }
    }
    const auto *relaxed = model->FindAnimation("wlk_relaxed");
    const auto *battle = model->FindAnimation("wlk");
    if (race == 0 && relaxed && battle)
        Check(memcmp(relaxed->frameWorldMats.data(), battle->frameWorldMats.data(),
            relaxed->frameWorldMats.size() * sizeof(RichMat43)) != 0,
            "relaxed locomotion retains a different upper-body track from the weapon bank");
    Check(NpcRenderGeometry::SelectIdleAnimation(model) &&
          model->pAnim == model->FindAnimation("idl_relaxed"),
        "installed humanoid NPC selects neutral idle rather than default walking");
    Check(!model->FindAnimation("missing"), "unknown clip is absent");
}
}

int main(int argc, char **argv)
{
    TestNpcDefaults();
    TestNpcRoles();
    if (argc != 2 && argc != 3) { std::cerr << "Pass the FFXI installation root and optional nameplate BMP output path.\n"; return 2; }
    HWND window = CreateWindowExA(0, "STATIC", "Animation tests", WS_POPUP,
        0, 0, 640, 480, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
    IDirect3D9 *d3d = Direct3DCreate9(D3D_SDK_VERSION);
    IDirect3DDevice9 *device = nullptr;
    D3DPRESENT_PARAMETERS pp = {};
    pp.Windowed = TRUE;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.hDeviceWindow = window;
    pp.EnableAutoDepthStencil = TRUE;
    pp.AutoDepthStencilFormat = D3DFMT_D24S8;
    if (d3d && window)
        d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window,
            D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &device);
    Check(device != nullptr, "create hidden D3D9 test device");
    if (device)
        TestNpcModels(argv[1], device, argc == 3 ? argv[2] : nullptr);
    if (device)
        TestNameplates(device, argc == 3 ? argv[2] : nullptr);
    if (device)
        for (int race = 0; race < kFFXICharRaceCount; ++race) TestRace(argv[1], race, device);
    if (device) device->Release();
    if (d3d) d3d->Release();
    if (window) DestroyWindow(window);
    std::cout << (failures ? "FAIL" : "PASS") << ": player animation checks (" << failures << " failures)\n";
    return failures ? 1 : 0;
}
