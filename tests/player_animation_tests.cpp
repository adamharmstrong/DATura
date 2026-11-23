#include "d3d9_graphics.h"
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
#include "d3d_model_buffers.h"
#include <filesystem>
#include <map>
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
                    Check(sm.pVB->Map(0, 0, &gpu, RendererBackend::MapAccess::Read), "read animated buffer");
                    if (gpu)
                    {
                        Check(memcmp(gpu, sm.cpuVerts.data(), sm.cpuVerts.size() * sizeof(FFXIVertex)) == 0,
                            "render buffer matches posed vertices");
                        sm.pVB->Unmap();
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

std::vector<DWORD> CaptureTarget(IDirect3DDevice9* device, const std::string& path)
{
    std::vector<DWORD> result;
    IDirect3DSurface9 *target = nullptr, *surface = nullptr;
    device->GetRenderTarget(0, &target);
    D3DSURFACE_DESC desc = {};
    if (target) target->GetDesc(&desc);
    if (target) device->CreateOffscreenPlainSurface(desc.Width, desc.Height, D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM, &surface, nullptr);
    D3DLOCKED_RECT pixels = {};
    if (surface && SUCCEEDED(device->GetRenderTargetData(target, surface)) &&
        SUCCEEDED(surface->LockRect(&pixels, nullptr, D3DLOCK_READONLY)))
    {
        result.resize(desc.Width * desc.Height);
        for (UINT y = 0; y < desc.Height; ++y)
            memcpy(result.data() + y * desc.Width, static_cast<char*>(pixels.pBits) + y * pixels.Pitch, desc.Width * 4);
        surface->UnlockRect();
        if (!path.empty())
        {
            BITMAPFILEHEADER header = {}; BITMAPINFOHEADER info = {};
            info.biSize = sizeof(info); info.biWidth = desc.Width; info.biHeight = -(LONG)desc.Height;
            info.biPlanes = 1; info.biBitCount = 32;
            header.bfType = 0x4D42; header.bfOffBits = sizeof(header) + sizeof(info);
            header.bfSize = header.bfOffBits + desc.Width * desc.Height * 4;
            std::ofstream output(path, std::ios::binary);
            output.write(reinterpret_cast<char*>(&header), sizeof(header));
            output.write(reinterpret_cast<char*>(&info), sizeof(info));
            output.write(reinterpret_cast<const char*>(result.data()), result.size() * 4);
        }
    }
    if (surface) surface->Release();
    if (target) target->Release();
    return result;
}

// Characters are Gouraud shaded in retail. Separate faceting caused by the
// mesh normals or fixed-function lighting from faceting added by the
// directional shadow map applied to the actor as a receiver.
void TestActorShading(const char* root, IDirect3DDevice9* device, const std::filesystem::path& output)
{
    std::filesystem::create_directories(output);
    noeRAPI_t rapi(device);
    rapi.SetCurrentFilePath("actor shading.ff11datset");
    char datSet[4096] = {};
    FFXIDatSet::PlayerOptions options; options.raceIndex = 0;
    std::string assetRoot(root);
    if (assetRoot.back() != '/' && assetRoot.back() != '\\') assetRoot += '/';
    Check(FFXIDatSet::BuildPlayer(assetRoot.c_str(), options, datSet, sizeof(datSet)), "build actor shading player");
    int count = 0;
    noesisModel_t* model = Model_FF11_LoadDATSet(reinterpret_cast<BYTE*>(datSet), static_cast<int>(strlen(datSet)), count, &rapi);
    Check(model != nullptr, "load actor shading player");
    if (!model) return;
    model->pAnim = model->FindAnimation("idl_relaxed");
    model->UpdateAnimation(0, device);
    ZoneModelRenderMetadata::Prepare(model, device);

    // Mesh normals: smooth meshes share normals between a triangle's corners.
    size_t triangles = 0, flat = 0;
    for (const auto& mesh : model->submeshes)
        for (size_t i = 0; i + 2 < mesh.cpuIndices.size(); i += 3)
        {
            const FFXIVertex* v[3] = {&mesh.cpuVerts[mesh.cpuIndices[i]], &mesh.cpuVerts[mesh.cpuIndices[i+1]],
                &mesh.cpuVerts[mesh.cpuIndices[i+2]]};
            const auto dot = [](const float* a, const float* b)
            {
                const float la = sqrtf(a[0]*a[0]+a[1]*a[1]+a[2]*a[2]), lb = sqrtf(b[0]*b[0]+b[1]*b[1]+b[2]*b[2]);
                return la > 0 && lb > 0 ? (a[0]*b[0]+a[1]*b[1]+a[2]*b[2])/(la*lb) : 1.0f;
            };
            ++triangles;
            if (dot(v[0]->nrm, v[1]->nrm) > .9995f && dot(v[0]->nrm, v[2]->nrm) > .9995f) ++flat;
        }
    std::cout << "Actor shading: " << triangles << " triangles, " << flat << " with identical corner normals\n";
    {
        DWORD lowest = 0xffffffff, highest = 0;
        for (const auto& mesh : model->submeshes) for (const auto& v : mesh.cpuVerts)
        { lowest = (std::min)(lowest, v.diffuse & 0xffffff); highest = (std::max)(highest, v.diffuse & 0xffffff); }
        std::cout << "Actor shading: vertex diffuse RGB " << std::hex << lowest << ".." << highest << std::dec << '\n';
    }
    {
        // Split normals: vertices at one position with different normals shade as a crease.
        std::map<std::array<long,3>, std::vector<std::array<float,3>>> byPosition;
        float lengthMin = 10, lengthMax = 0;
        for (const auto& mesh : model->submeshes) for (const auto& v : mesh.cpuVerts)
        {
            const float length = sqrtf(v.nrm[0]*v.nrm[0]+v.nrm[1]*v.nrm[1]+v.nrm[2]*v.nrm[2]);
            lengthMin = (std::min)(lengthMin, length); lengthMax = (std::max)(lengthMax, length);
            if (length > 0) byPosition[{lroundf(v.pos[0]*1000), lroundf(v.pos[1]*1000), lroundf(v.pos[2]*1000)}]
                .push_back({v.nrm[0]/length, v.nrm[1]/length, v.nrm[2]/length});
        }
        size_t shared = 0, creased = 0;
        for (const auto& [position, normals] : byPosition)
        {
            if (normals.size() < 2) continue;
            ++shared;
            float worst = 1;
            for (const auto& a : normals) for (const auto& b : normals)
                worst = (std::min)(worst, a[0]*b[0]+a[1]*b[1]+a[2]*b[2]);
            if (worst < cosf(20 * 3.14159265f / 180)) ++creased;
        }
        std::cout << "Actor shading: " << byPosition.size() << " positions, " << shared << " shared by several vertices, "
            << creased << " with normals >20 degrees apart; normal length " << lengthMin << ".." << lengthMax << '\n';
    }

    noesisModel_t::Submesh plate;
    plate.objectName = plate.materialName = "actor-shading-ground";
    for (const auto& corner : {std::array<float,2>{-20,-20}, std::array<float,2>{20,-20},
        std::array<float,2>{20,20}, std::array<float,2>{-20,20}})
    {
        FFXIVertex vertex = {};
        vertex.pos[0] = corner[0]; vertex.pos[2] = corner[1]; vertex.nrm[1] = -1; vertex.diffuse = 0xff808080;
        plate.cpuVerts.push_back(vertex);
    }
    plate.cpuIndices = {0,1,2,0,2,3}; plate.vertCount = 4; plate.triCount = 2;
    noesisMaterial_t groundMaterial; groundMaterial.name = const_cast<char*>("actor-shading-ground");
    groundMaterial.flags = NMATFLAG_TWOSIDED; groundMaterial.noDefaultBlend = true;
    noesisMaterial_t* groundMaterials[] = {&groundMaterial};
    noesisMatData_t groundData = {}; groundData.mats = groundMaterials; groundData.matCount = 1;
    noesisModel_t ground; ground.submeshes.push_back(plate); ground.pMatData = &groundData;
    ground.BuildD3DBuffers(device); ZoneModelRenderMetadata::Prepare(&ground, device);

    const auto world = D3DMath::BuildIdentity();
    // Bastok Markets' authored model lighting at 10:00 (ROM/1/35.DAT environment
    // records): ambient plus sun reaches ~1.9, past D3D's per-vertex clamp.
    bool bastokLight = false, actorClampDisabled = false;
    const auto render = [&](float minute, bool shadows, const float eye[3], const float target[3], const std::string& name)
    {
        device->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xff203040, 1, 0);
        device->BeginScene();
        const auto view = D3DMath::BuildLookAtLH(eye[0], eye[1], eye[2], target[0], target[1], target[2]);
        const auto projection = D3DMath::BuildPerspectiveFovLH(0.5f, 640.0f / 480.0f, 0.05f, 200);
        device->SetTransform(D3DTS_VIEW, &view); device->SetTransform(D3DTS_PROJECTION, &projection);
        ModelRenderer::Context context; context.device = device;
        context.lightingQuality = 2; context.automaticLightDirection = true; context.minuteOfDay = minute;
        if (bastokLight)
        {
            context.useAuthoredLightColor = true; context.authoredMainLightColor = 0xffc4c2ad;
            context.authoredSecondaryLightColor = 0xff3f3f38; context.authoredAmbientLightColor = 0xffa7a8a6;
            context.authoredLightPower = 1.63333f;
        }
        context.dynamicActorShadows = context.dynamicObjectShadows = true;
        context.shadowOpacity = .32f; context.shadowMaxDistance = 80;
        std::copy(eye, eye + 3, context.cameraPosition);
        context.shadowCenterValid = true; std::copy(target, target + 3, context.shadowCenter);
        if (shadows) ModelRenderer::BeginDirectionalShadowFrame();
        auto zone = context; zone.rendersZoneObjects = true; zone.geometryPass = ModelRenderer::GeometryPass::Opaque;
        ModelRenderer::PrepareFixedFunctionPass(zone, world);
        ModelRenderer::DrawGeometry(zone, &ground, world);
        ModelRenderer::PrepareFixedFunctionPass(context, world);
        if (actorClampDisabled)
        {
            // Same lighting, but the zone material path (vertex colour, per-vertex clamp).
            auto zoneLit = context; zoneLit.rendersZoneObjects = true; zoneLit.geometryPass = ModelRenderer::GeometryPass::Opaque;
            ModelRenderer::DrawGeometry(zoneLit, model, world);
            auto transparent = zoneLit; transparent.geometryPass = ModelRenderer::GeometryPass::Transparent;
            ModelRenderer::DrawGeometry(transparent, model, world);
        }
        else ModelRenderer::DrawGeometry(context, model, world);
        if (shadows) Check(ModelRenderer::DrawDirectionalShadowFrame(), "render actor shading shadow frame");
        device->EndScene();
        return CaptureTarget(device, (output / (name + ".bmp")).string());
    };
    float top = 0;
    for (const auto& mesh : model->submeshes) for (const auto& v : mesh.cpuVerts) top = (std::min)(top, v.pos[1]);
    const float head[3] = {0, top * .93f, 0};
    {
        // Outward normals on the crown point up, which is -Y in DATura scene space.
        double crown = 0; int samples = 0;
        for (const auto& mesh : model->submeshes) for (const auto& v : mesh.cpuVerts)
            if (v.pos[1] < top * .985f) { crown += v.nrm[1]; ++samples; }
        std::cout << "Actor shading: crown normal mean Y=" << (samples ? crown / samples : 0) << " over " << samples << " vertices\n";
    }
    {
        // Close-ups of the arm and shoulder with authored normals, then with
        // normals welded across vertices that share a position.
        const float armEye[3] = {1.3f, top * .55f, 1.1f}, arm[3] = {0, top * .55f, .25f};
        const float shoulderEye[3] = {1.2f, top * .78f, .9f}, shoulder[3] = {0, top * .78f, .2f};
        bastokLight = true;
        const float faceEye[3] = {1.6f, top * .9f, .4f};
        for (bool perPixel : {false, true})
        {
            // Emulate the old per-vertex clamp by drawing the actor as zone geometry would be lit.
            actorClampDisabled = !perPixel;
            const std::string mode = perPixel ? "per-pixel-clamp" : "per-vertex-clamp";
            render(600, false, armEye, arm, "arm-600-bastok-light-" + mode);
            render(600, false, shoulderEye, shoulder, "shoulder-600-bastok-light-" + mode);
            render(600, false, faceEye, head, "face-600-bastok-light-" + mode);
        }
        actorClampDisabled = false;
        bastokLight = false;
        render(900, false, armEye, arm, "arm-900-authored-normals");
        render(900, false, shoulderEye, shoulder, "shoulder-900-authored-normals");
        std::map<std::array<long,3>, std::vector<std::pair<size_t,size_t>>> groups;
        for (size_t m = 0; m < model->submeshes.size(); ++m)
            for (size_t v = 0; v < model->submeshes[m].cpuVerts.size(); ++v)
            {
                const auto& p = model->submeshes[m].cpuVerts[v].pos;
                groups[{lroundf(p[0]*1000), lroundf(p[1]*1000), lroundf(p[2]*1000)}].push_back({m, v});
            }
        auto saved = model->submeshes;
        for (const auto& [position, members] : groups)
        {
            float sum[3] = {};
            for (const auto& [m, v] : members) for (int a = 0; a < 3; ++a) sum[a] += saved[m].cpuVerts[v].nrm[a];
            const float length = sqrtf(sum[0]*sum[0]+sum[1]*sum[1]+sum[2]*sum[2]);
            if (length <= 0) continue;
            for (const auto& [m, v] : members) for (int a = 0; a < 3; ++a)
                model->submeshes[m].cpuVerts[v].nrm[a] = sum[a] / length;
        }
        for (auto& mesh : model->submeshes) D3DModelBuffers::UploadVertices(mesh);
        render(900, false, armEye, arm, "arm-900-welded-normals");
        render(900, false, shoulderEye, shoulder, "shoulder-900-welded-normals");
        for (size_t m = 0; m < model->submeshes.size(); ++m)
        { model->submeshes[m].cpuVerts = saved[m].cpuVerts; D3DModelBuffers::UploadVertices(model->submeshes[m]); }
    }
    for (float minute : {600.0f, 900.0f})
    {
        // Arms and torso from the front: lighting only, without any shadow map.
        const float bodyEye[3] = {2.4f, top * .62f, .9f}, body[3] = {0, top * .62f, 0};
        render(minute, false, bodyEye, body, "body-" + std::to_string(int(minute)) + "-lighting-only");
    }
    for (float minute : {600.0f, 900.0f})
    {
        const float eye[3] = {1.6f, top * .9f, .4f};
        const std::string tag = std::to_string(int(minute));
        const auto lit = render(minute, false, eye, head, "face-" + tag + "-lighting-only");
        const auto shadowed = render(minute, true, eye, head, "face-" + tag + "-with-shadow-map");
        int darkened = 0;
        std::vector<DWORD> diff(lit.size());
        for (size_t i = 0; i < lit.size() && i < shadowed.size(); ++i)
        {
            const int change = int((lit[i] >> 8) & 255) - int((shadowed[i] >> 8) & 255);
            if (change > 6) ++darkened;
            const int scaled = (std::min)(255, (std::max)(0, change * 4));
            diff[i] = 0xff000000 | (scaled << 16) | (scaled << 8) | scaled;
        }
        std::cout << "Actor shading " << tag << ": pixels darkened by the shadow map on the face view=" << darkened << '\n';
        // Nothing but the actor itself can occlude this view. Before normal offset
        // and terminator fading, self-shadowing faceted 2,855 (10:00) and 15,126
        // (15:00) pixels; the chin legitimately shades the neck.
        Check(darkened < 1000, "directional shadows do not facet an unoccluded actor with self-shadowing");
        BITMAPFILEHEADER header = {}; BITMAPINFOHEADER info = {};
        info.biSize = sizeof(info); info.biWidth = 640; info.biHeight = -480; info.biPlanes = 1; info.biBitCount = 32;
        header.bfType = 0x4D42; header.bfOffBits = sizeof(header) + sizeof(info);
        header.bfSize = header.bfOffBits + static_cast<DWORD>(diff.size() * 4);
        std::ofstream file((output / ("face-" + tag + "-shadow-difference-x4.bmp")).string(), std::ios::binary);
        file.write(reinterpret_cast<char*>(&header), sizeof(header));
        file.write(reinterpret_cast<char*>(&info), sizeof(info));
        file.write(reinterpret_cast<const char*>(diff.data()), diff.size() * 4);
    }
    ModelRenderer::ReleaseDirectionalShadowResources();
    ground.ReleaseD3DBuffers();
}

int main(int argc, char **argv)
{
    if (argc == 4 && std::string(argv[1]) == "--actor-shading")
    {
        HWND window = CreateWindowExA(0, "STATIC", "Actor shading", WS_POPUP, 0, 0, 640, 480,
            nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
        IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
        IDirect3DDevice9* device = nullptr;
        D3DPRESENT_PARAMETERS pp = {};
        pp.Windowed = TRUE; pp.SwapEffect = D3DSWAPEFFECT_DISCARD; pp.hDeviceWindow = window;
        pp.BackBufferWidth = 640; pp.BackBufferHeight = 480; pp.BackBufferFormat = D3DFMT_X8R8G8B8;
        pp.EnableAutoDepthStencil = TRUE; pp.AutoDepthStencilFormat = D3DFMT_D24S8;
        if (d3d && window)
            d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window, D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, &device);
        Check(device != nullptr, "create actor shading device");
        if (device) TestActorShading(argv[2], device, argv[3]);
        if (device) device->Release();
        if (d3d) d3d->Release();
        if (window) DestroyWindow(window);
        std::cout << (failures ? "FAIL" : "PASS") << ": actor shading (" << failures << " failures)\n";
        return failures ? 1 : 0;
    }
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
