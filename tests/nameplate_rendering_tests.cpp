#include "stdafx.h"
#include "ffxi_nameplate_font.h"
#include "npc_nameplate_renderer.h"
#include "ffxi_player_icons.h"
#include <fstream>

bool TestNameplateConfig();
bool TestStarterQuests();

template<class T> struct Com
{
    T* p = nullptr;
    ~Com() { if (p) p->Release(); }
    T* operator->() { return p; }
};

std::vector<DWORD> Capture(IDirect3DDevice9* device)
{
    Com<IDirect3DSurface9> target, copy;
    if (FAILED(device->GetRenderTarget(0, &target.p)) ||
        FAILED(device->CreateOffscreenPlainSurface(512, 128, D3DFMT_X8R8G8B8,
            D3DPOOL_SYSTEMMEM, &copy.p, nullptr)) ||
        FAILED(device->GetRenderTargetData(target.p, copy.p))) return {};
    D3DLOCKED_RECT lock = {};
    if (FAILED(copy->LockRect(&lock, nullptr, D3DLOCK_READONLY))) return {};
    std::vector<DWORD> pixels(512 * 128);
    for (int y = 0; y < 128; ++y)
        memcpy(pixels.data() + y * 512, static_cast<BYTE*>(lock.pBits) + y * lock.Pitch, 512 * 4);
    copy->UnlockRect();
    return pixels;
}

int main(int argc, char** argv)
{
    if (argc == 2 && std::string(argv[1]) == "--config-only")
        return TestNameplateConfig() ? 0 : 1;
    if (argc != 3) { puts("usage: NameplateRenderingTests <FFXI root> <preview.bmp>"); return 2; }
    FFXIBitmapFont::SetRootPath(argv[1]);
    if (!FFXINameplateFont::Load()) return 1;
    if (!TestNameplateConfig()) { puts("FAIL: Config nameplate settings"); return 1; }
    if (!TestStarterQuests()) { puts("FAIL: starter quest catalog"); return 1; }
    HWND window = CreateWindowExA(0, "STATIC", "Nameplate regression", WS_POPUP,
        0, 0, 512, 128, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
    Com<IDirect3D9> d3d;
    Com<IDirect3DDevice9> device;
    d3d.p = Direct3DCreate9(D3D_SDK_VERSION);
    D3DPRESENT_PARAMETERS pp = {};
    pp.Windowed = TRUE;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.hDeviceWindow = window;
    pp.BackBufferWidth = 512; pp.BackBufferHeight = 128;
    pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.EnableAutoDepthStencil = TRUE;
    pp.AutoDepthStencilFormat = D3DFMT_D24S8;
    if (!window || !d3d.p || FAILED(d3d->CreateDevice(D3DADAPTER_DEFAULT,
        D3DDEVTYPE_HAL, window, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &device.p))) return 1;
    std::vector<NpcNameplateRenderer::DrawItem> labels = {
        {"Jeanvirgaud", "Home Point", 256, 85, 0.5f, 16.0f}
    };
    auto render = [&](float depth) {
        DWORD scissorEnabled = 0;
        device->GetRenderState(D3DRS_SCISSORTESTENABLE, &scissorEnabled);
        device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
        device->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL,
            0xff101830, depth, 0);
        device->SetRenderState(D3DRS_SCISSORTESTENABLE, scissorEnabled);
        device->BeginScene();
        NpcNameplateRenderer::Draw(device.p, labels);
        device->EndScene();
        return Capture(device.p);
    };
    const auto baseline = render(1.0f);
    int green = 0;
    for (DWORD pixel : baseline)
        green += ((pixel >> 8) & 255) > 150 && ((pixel >> 8) & 255) > ((pixel >> 16) & 255) + 30;
    if (green < 20) { puts("FAIL: expected green NPC glyphs"); return 1; }
    labels[0].starterQuestAvailable = true;
    const auto marked = render(1.0f);
    int gold = 0;
    for (int y = 0; y < 128; ++y)
        for (int x = 0; x < 512; ++x)
        {
            const DWORD p = marked[y * 512 + x];
            gold += ((p >> 16) & 255) > 180 && ((p >> 8) & 255) > 140 && (p & 255) < 120;
            // Appending the marker above must preserve the existing label pixels.
            if (y >= 49 && p != baseline[y * 512 + x])
            { puts("FAIL: quest marker moves or recolours NPC label"); return 1; }
        }
    if (gold < 15) { puts("FAIL: official gold quest glyph missing"); return 1; }
    const auto markedHidden = render(0.1f);
    if (std::any_of(markedHidden.begin(), markedHidden.end(), [](DWORD p) { return (p & 0xFFFFFF) != 0x101830; }))
    { puts("FAIL: quest marker bypasses depth occlusion"); return 1; }
    // A separate preview uses the same real GPU pass as the application.
    {
        const auto original = labels[0];
        labels[0].name = "Arawn";
        labels[0].roleTitle = "Quest Giver";
        labels[0].screenY = 115; // Leave room for the enlarged marker in the preview.
        const auto previewPixels = render(1.0f);
        labels[0] = original;
        BITMAPFILEHEADER header = {};
        BITMAPINFOHEADER info = {};
        info.biSize = sizeof(info); info.biWidth = 512; info.biHeight = -128;
        info.biPlanes = 1; info.biBitCount = 32;
        header.bfType = 0x4d42; header.bfOffBits = sizeof(header) + sizeof(info);
        header.bfSize = header.bfOffBits + DWORD(previewPixels.size() * 4);
        std::ofstream preview(std::string(argv[2]) + ".quest.bmp", std::ios::binary);
        preview.write(reinterpret_cast<char*>(&header), sizeof(header));
        preview.write(reinterpret_cast<char*>(&info), sizeof(info));
        preview.write(reinterpret_cast<const char*>(previewPixels.data()), previewPixels.size() * 4);
        if (!preview) return 1;
    }
    labels[0].starterQuestAvailable = false;
    if (render(1.0f) != baseline) { puts("FAIL: quest marker cache leaks into unmarked NPCs"); return 1; }
    // Simulate materials that switch as the camera changes visible geometry.
    for (DWORD cull : {DWORD(D3DCULL_CW), DWORD(D3DCULL_CCW)})
    {
        device->SetRenderState(D3DRS_CULLMODE, cull);
        device->SetRenderState(D3DRS_FOGENABLE, TRUE);
        device->SetRenderState(D3DRS_FOGCOLOR, 0xff807060);
        device->SetRenderState(D3DRS_FOGTABLEMODE, D3DFOG_LINEAR);
        device->SetRenderState(D3DRS_STENCILENABLE, TRUE);
        device->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_NEVER);
        device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_REVSUBTRACT);
        device->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED);
        device->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
        RECT scissor = {0, 0, 1, 1}; device->SetScissorRect(&scissor);
        device->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);
        device->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_CAMERASPACEPOSITION);
        device->SetTextureStageState(0, D3DTSS_RESULTARG, D3DTA_TEMP);
        device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
        device->SetTextureStageState(1, D3DTSS_COLORARG1, D3DTA_TFACTOR);
        device->SetRenderState(D3DRS_TEXTUREFACTOR, 0xff807060);
        if (render(1.0f) != baseline) { puts("FAIL: inherited scene state changes nameplate pixels"); return 1; }
        labels[0].starterQuestAvailable = true;
        if (render(1.0f) != marked) { puts("FAIL: inherited scene state changes quest marker pixels"); return 1; }
        labels[0].starterQuestAvailable = false;
        DWORD value = 0;
        device->GetRenderState(D3DRS_CULLMODE, &value);
        if (value != cull) return 1;
        device->GetRenderState(D3DRS_FOGENABLE, &value);
        if (value != TRUE) return 1;
        device->GetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, &value);
        if (value != D3DTTFF_COUNT2) return 1;
    }
    const auto occluded = render(0.25f);
    if (occluded.empty()) return 1;
    for (DWORD pixel : occluded)
        if ((pixel & 0xffffff) != 0x101830) { puts("FAIL: labels draw through foreground depth"); return 1; }

    labels[0].nameColor = 0xFFFFFFFF;
    const auto whiteName = render(1.0f);
    if (whiteName.empty() || whiteName == baseline) { puts("FAIL: cached NPC colour leaked into player label"); return 1; }
    labels[0].icon = "unknown-icon";
    if (render(1.0f) != whiteName) { puts("FAIL: unknown icon should render name only"); return 1; }
    labels[0].icon = "linkshell";
    labels[0].iconColor = 0xFFFF4040;
    const auto redShell = render(1.0f);
    labels[0].iconColor = 0xFF40FF40;
    const auto greenShell = render(1.0f);
    int iconDifferences = 0;
    for (size_t i = 0; i < redShell.size(); ++i)
        if (redShell[i] != greenShell[i])
        {
            ++iconDifferences;
            if (i % 512 >= 220) { puts("FAIL: Linkshell tint leaked into player text"); return 1; }
        }
    if (iconDifferences < 20 || redShell == whiteName) { puts("FAIL: missing Linkshell tint/icon"); return 1; }
    labels[0].icon = "mentor";
    const auto mentor = render(1.0f);
    labels[0].iconColor = 0xFFFF4040;
    if (render(1.0f) != mentor) { puts("FAIL: non-Linkshell icon was recoloured"); return 1; }
    // Retail connection-problem badge is red; catch decoder byte-order swaps.
    labels[0].icon = "disconnecting";
    const auto disconnected = render(1.0f);
    int redPixels = 0;
    for (size_t i = 0; i < disconnected.size(); ++i)
        if (i % 512 < 220)
        {
            const DWORD pixel = disconnected[i];
            redPixels += ((pixel >> 16) & 255) > 100 &&
                ((pixel >> 16) & 255) > (pixel & 255) + 40;
        }
    if (redPixels < 10) { puts("FAIL: retail icon red/blue channels swapped"); return 1; }
    labels[0].icon.clear(); labels[0].nameColor = 0xFFA4FFA4;
    if (render(1.0f) != baseline) { puts("FAIL: NPC rendering changed after player icons"); return 1; }

    if (FFXIPlayerIcons::Find("busy") != FFXIPlayerIcons::Find("auto-party")) return 1;
    // Every entry must have visible atlas content and a unique persistent ID.
    for (const auto& icon : FFXIPlayerIcons::Catalog)
    {
        if (FFXIPlayerIcons::Find(icon.id) != &icon || strlen(icon.id) >= 32) return 1;
        std::vector<DWORD> sprite(40 * 20);
        FFXIPlayerIcons::Composite(sprite, 40, 20, 0, 0, icon, 0xFFFFFFFF);
        if (std::count_if(sprite.begin(), sprite.end(), [](DWORD p) { return p >> 24; }) < 20)
        { puts("FAIL: empty retail icon"); return 1; }
    }
    for (const auto& pair : {std::pair{"campaign", "campaign-seeking"},
        std::pair{"campaign-seeking", "campaign-auto-party"},
        std::pair{"gladiator", "gladiator-cg"}, std::pair{"gladiator-cg", "gladiator-hcg"},
        std::pair{"monipulator", "monipulator-nm"}, std::pair{"monipulator-nm", "monipulator-hnm"},
        std::pair{"mentor-bronze", "mentor-silver"}, std::pair{"mentor-silver", "mentor-gold"}})
    {
        labels[0].icon = pair.first;
        const auto first = render(1.0f);
        labels[0].icon = pair.second;
        if (first == render(1.0f)) { puts("FAIL: missing composite/rank variant"); return 1; }
    }
    labels[0].icon = "linkshell";
    const auto withoutStars = render(1.0f);
    labels[0].jobMaster = true;
    const auto withStars = render(1.0f);
    if (withoutStars == withStars) { puts("FAIL: Job Master stars missing"); return 1; }
    // Adding the row above must not move or recolour the existing name/icon/title.
    for (int y = 49; y < 128; ++y)
        for (int x = 0; x < 512; ++x)
            if (withoutStars[y * 512 + x] != withStars[y * 512 + x])
            { puts("FAIL: Job Master stars alter existing label"); return 1; }
    labels[0].jobMaster = false;
    if (render(1.0f) != withoutStars) { puts("FAIL: Job Master cache isolation"); return 1; }

    // Produce a gallery through the real cached D3D pass, not a separate mockup.
    constexpr int iconCount = int(std::size(FFXIPlayerIcons::Catalog));
    const int galleryHeight = ((iconCount + 2) / 2) * 80;
    constexpr int galleryWidth = 768, cellWidth = galleryWidth / 2;
    std::vector<DWORD> gallery(galleryWidth * galleryHeight, 0xFF101830);
    labels[0].name = "Adventurer";
    labels[0].nameColor = 0xFFFFFFFF;
    labels[0].iconColor = 0xFFA080E0;
    for (int index = 0; index <= iconCount; ++index)
    {
        const auto& icon = FFXIPlayerIcons::Catalog[index < iconCount ? index : 0];
        if (!FFXIPlayerIcons::Available(icon.id)) return 1;
        labels[0].icon = icon.id; labels[0].roleTitle = icon.label;
        labels[0].jobMaster = index == iconCount;
        if (labels[0].jobMaster) labels[0].roleTitle = "Job Master + Linkshell";
        const auto rendered = render(1.0f);
        if (rendered.empty()) return 1;
        for (int y = 0; y < 80; ++y)
            memcpy(gallery.data() + (index / 2 * 80 + y) * galleryWidth + index % 2 * cellWidth,
                rendered.data() + (y + 24) * 512 + (512 - cellWidth) / 2, cellWidth * 4);
    }
    BITMAPFILEHEADER header = {};
    BITMAPINFOHEADER info = {};
    info.biSize = sizeof(info); info.biWidth = galleryWidth; info.biHeight = -galleryHeight;
    info.biPlanes = 1; info.biBitCount = 32;
    header.bfType = 0x4d42; header.bfOffBits = sizeof(header) + sizeof(info);
    header.bfSize = header.bfOffBits + DWORD(gallery.size() * 4);
    std::ofstream output(argv[2], std::ios::binary);
    output.write(reinterpret_cast<char*>(&header), sizeof(header));
    output.write(reinterpret_cast<char*>(&info), sizeof(info));
    output.write(reinterpret_cast<const char*>(gallery.data()), gallery.size() * 4);
    if (!output) return 1;
    NpcNameplateRenderer::ClearCachedTextures();
    device.p->Release(); device.p = nullptr;
    DestroyWindow(window);
    puts("PASS: GPU colours, retail/composite/rank icons, Job Master, Linkshell tint/cache isolation, scene state and occlusion.");
    return 0;
}
