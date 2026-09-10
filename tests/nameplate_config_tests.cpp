#include "stdafx.h"
#include "config_dialog.h"
#include "ffxi_player_icons.h"
#include "npc_chat_window.h"

namespace
{
struct Fixture
{
    ApplicationSettings::State settings;
    GameUiConfig config = {};
    Win32Theme::State theme;
    ConfigDialog::State dialog;
    HWND owner = nullptr;
    char temporary[MAX_PATH] = {};
    int notifications = 0;
    ~Fixture()
    {
        ConfigDialog::Close(dialog);
        if (owner) DestroyWindow(owner);
        Win32Theme::ReleaseState(theme);
        if (temporary[0]) DeleteFileA(temporary);
    }
};

void Changed(void* context, const ConfigDialog::Event& event)
{
    if (event.command == ConfigDialog::Command::PlayerNameplateChanged)
        ++static_cast<Fixture*>(context)->notifications;
}
}

bool TestNameplateConfig()
{
    Fixture f;
    GameUiConfig_SetDefaults(f.config);
    if (f.config.chatLogTimeoutSeconds != 15) return false;
    if (f.config.chatLogWidthPercent != 50 || f.config.chatLogHeightPercent != 25) return false;
    Win32Theme::RecreateResources(f.theme.resources, true);
    f.owner = CreateWindowExA(0, "STATIC", "Nameplate config test", WS_POPUP,
        -10000, -10000, 640, 716, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
    if (!f.owner) return false;
    ConfigDialog::Initialize(f.dialog, f.owner, "", f.settings, f.theme, f.config,
        Changed, nullptr, &f);
    ConfigDialog::Show(f.dialog);
    const HWND window = ConfigDialog::Window(f.dialog);
    if (!window) return false;
    const HWND combo = GetDlgItem(window, 8325);
    const HWND stars = GetDlgItem(window, 8326);
    const int count = static_cast<int>(std::size(FFXIPlayerIcons::Catalog));
    if (!combo || !stars || SendMessageA(combo, CB_GETCOUNT, 0, 0) != count + 1) return false;
    for (int i = 0; i <= count; ++i)
    {
        SendMessageA(combo, CB_SETCURSEL, i, 0);
        SendMessageA(window, WM_COMMAND, MAKEWPARAM(8325, CBN_SELCHANGE), reinterpret_cast<LPARAM>(combo));
        const char* expected = i ? FFXIPlayerIcons::Catalog[i - 1].id : "none";
        if (strcmp(f.config.playerNameplate.icon, expected) || f.notifications != i + 1) return false;
        if (SendMessageA(combo, CB_GETCURSEL, 0, 0) != i) return false;
    }
    strcpy_s(f.config.playerNameplate.icon, "busy");
    ConfigDialog::Sync(f.dialog);
    const int migrated = static_cast<int>(SendMessageA(combo, CB_GETCURSEL, 0, 0));
    if (migrated <= 0 || strcmp(FFXIPlayerIcons::Catalog[migrated - 1].id, "auto-party")) return false;
    SendMessageA(stars, BM_SETCHECK, BST_CHECKED, 0);
    SendMessageA(window, WM_COMMAND, MAKEWPARAM(8326, BN_CLICKED), reinterpret_cast<LPARAM>(stars));
    if (!f.config.playerNameplate.jobMaster) return false;
    const auto select = [&](int id, int index) {
        SendDlgItemMessageA(window, id, CB_SETCURSEL, index, 0);
        SendMessageA(window, WM_COMMAND, MAKEWPARAM(id, CBN_SELCHANGE), reinterpret_cast<LPARAM>(GetDlgItem(window, id)));
    };
    const auto edit = [&](int id, const char* text) {
        SetDlgItemTextA(window, id, text);
        SendMessageA(window, WM_COMMAND, MAKEWPARAM(id, EN_KILLFOCUS), reinterpret_cast<LPARAM>(GetDlgItem(window, id)));
    };
    edit(8337, "30");
    edit(8338, "60");
    edit(8339, "40");
    if (f.config.chatLogWidthPercent != 60 || f.config.chatLogHeightPercent != 40) return false;
    if (f.config.chatLogTimeoutSeconds != 30) return false;
    edit(8337, "0");
    if (f.config.chatLogTimeoutSeconds != 1) return false;
    edit(8337, "999");
    if (f.config.chatLogTimeoutSeconds != 300) return false;
    edit(8337, "");
    if (f.config.chatLogTimeoutSeconds != 300) return false;
    edit(8337, "15");
    select(8327, 1);
    edit(8329, "Vana'diel Explorers");
    if (GameUiConfig_PlayerSubtitle(f.config.playerNameplate) != "Vana'diel Explorers") return false;
    select(8327, 2);
    select(8330, 0); // WAR
    select(8332, 13); // NIN
    edit(8331, "99");
    edit(8333, "37"); // Deliberately not half the main level.
    if (GameUiConfig_PlayerSubtitle(f.config.playerNameplate) != "WAR 99 / NIN 37") return false;
    select(8332, 0);
    if (GameUiConfig_PlayerSubtitle(f.config.playerNameplate) != "WAR 99") return false;
    select(8332, 13);
    select(8327, 0);
    if (!GameUiConfig_PlayerSubtitle(f.config.playerNameplate).empty()) return false;
    select(8327, 1);
    if (GameUiConfig_PlayerSubtitle(f.config.playerNameplate) != "Vana'diel Explorers") return false;
    select(8327, 2);
    ConfigDialog::Close(f.dialog);
    ConfigDialog::Show(f.dialog);
    if (SendDlgItemMessageA(f.dialog.window, 8326, BM_GETCHECK, 0, 0) != BST_CHECKED) return false;
    if (SendDlgItemMessageA(f.dialog.window, 8327, CB_GETCURSEL, 0, 0) != 2) return false;

    char directory[MAX_PATH] = {};
    if (!GetTempPathA(MAX_PATH, directory) || !GetTempFileNameA(directory, "NPI", 0, f.temporary)) return false;
    strcpy_s(f.config.loadedPath, f.temporary);
    if (!GameUiConfig_SaveChatLog(f.config) ||
        GetPrivateProfileIntA("ChatLog", "TimeoutSeconds", 0, f.temporary) != 15) return false;
    if (GetPrivateProfileIntA("ChatLog", "WidthPercent", 0, f.temporary) != 60 ||
        GetPrivateProfileIntA("ChatLog", "HeightPercent", 0, f.temporary) != 40) return false;
    WritePrivateProfileStringA("Unrelated", "Preserve", "yes", f.temporary);
    strcpy_s(f.config.playerNameplate.icon, "monipulator-hnm");
    if (!GameUiConfig_SavePlayerNameplate(f.config)) return false;
    char stored[64] = {};
    GetPrivateProfileStringA("Player.Nameplate", "Icon", "", stored, sizeof(stored), f.temporary);
    if (strcmp(stored, "monipulator-hnm") ||
        GetPrivateProfileIntA("Player.Nameplate", "JobMaster", 0, f.temporary) != 1) return false;
    GetPrivateProfileStringA("Unrelated", "Preserve", "", stored, sizeof(stored), f.temporary);
    if (strcmp(stored, "yes")) return false;
    GetPrivateProfileStringA("Player.Nameplate", "Subtitle", "", stored, sizeof(stored), f.temporary);
    if (strcmp(stored, "jobs") || GetPrivateProfileIntA("Player.Nameplate", "SubLevel", 0, f.temporary) != 37) return false;
    GetPrivateProfileStringA("Player.Nameplate", "LinkshellName", "", stored, sizeof(stored), f.temporary);
    if (strcmp(stored, "Vana'diel Explorers")) return false;
    strcat_s(f.config.loadedPath, "\\missing\\config.ini");
    if (GameUiConfig_SavePlayerNameplate(f.config)) return false;
    if (GameUiConfig_SaveChatLog(f.config)) return false;

    ShowWindow(f.owner, SW_SHOWNOACTIVATE);
    NpcChatWindow::SetTimeout(15);
    const ULONGLONG before = GetTickCount64();
    NpcChatWindow::Show(f.owner, "Test NPC", "Time to read this dialogue.");
    if (!IsWindowVisible(NpcChatWindow::window) || NpcChatWindow::hideAt < before + 15000) return false;
    RECT logBounds{}, ownerBounds{};
    GetClientRect(NpcChatWindow::window, &logBounds);
    GetClientRect(f.owner, &ownerBounds);
    if (logBounds.right != (ownerBounds.right - 16) / 2) return false;
    HDC screen = GetDC(NpcChatWindow::transcript);
    HDC rendered = CreateCompatibleDC(screen);
    HBITMAP bitmap = CreateCompatibleBitmap(screen, logBounds.right, logBounds.bottom);
    HGDIOBJ oldBitmap = SelectObject(rendered, bitmap);
    SendMessageW(NpcChatWindow::transcript, WM_PRINT, (WPARAM)rendered, PRF_CLIENT);
    const COLORREF stripeA = GetPixel(rendered, 4, 60);
    const COLORREF stripeB = GetPixel(rendered, 4, 62);
    SelectObject(rendered, oldBitmap);
    DeleteObject(bitmap); DeleteDC(rendered); ReleaseDC(NpcChatWindow::transcript, screen);
    if (stripeA != RGB(17, 27, 70) || stripeB != NpcChatWindow::background) return false;
    SendMessageW(NpcChatWindow::window, WM_TIMER, NpcChatWindow::closeTimer, 0);
    if (!IsWindowVisible(NpcChatWindow::window)) return false;
    NpcChatWindow::hideAt = GetTickCount64() - 1;
    NpcChatWindow::Show(f.owner, "Test NPC", "A new message restarts the countdown.");
    SendMessageW(NpcChatWindow::window, WM_TIMER, NpcChatWindow::closeTimer, 0);
    if (!IsWindowVisible(NpcChatWindow::window)) return false;
    SendMessageW(NpcChatWindow::transcript, WM_KEYDOWN, VK_ESCAPE, 0);
    if (!IsWindowVisible(NpcChatWindow::window) || NpcChatWindow::compactedLines != 1) return false;
    SendMessageW(NpcChatWindow::transcript, WM_KEYDOWN, VK_ESCAPE, (1LL << 30));
    if (NpcChatWindow::compactedLines != 1) return false;
    SendMessageW(NpcChatWindow::window, WM_TIMER, NpcChatWindow::closeTimer, 0);
    if (NpcChatWindow::compactedLines != 1) return false;
    NpcChatWindow::Show(f.owner, "Test NPC", "Restore before checking automatic compaction.");
    NpcChatWindow::hideAt = GetTickCount64() - 1;
    SendMessageW(NpcChatWindow::window, WM_TIMER, NpcChatWindow::closeTimer, 0);
    if (!IsWindowVisible(NpcChatWindow::window) || NpcChatWindow::compactedLines != 1) return false;
    RECT compacted{}; GetWindowRect(NpcChatWindow::window, &compacted);
    SendMessageW(NpcChatWindow::window, WM_TIMER, NpcChatWindow::closeTimer, 0);
    if (NpcChatWindow::compactedLines != 1) return false;
    NpcChatWindow::Show(f.owner, "Test NPC", "Restore the configured height.");
    RECT expanded{}; GetWindowRect(NpcChatWindow::window, &expanded);
    if (expanded.bottom != compacted.bottom ||
        compacted.top - expanded.top != NpcChatWindow::lineHeight) return false;
    for (int i = 0; i < 100 && IsWindowVisible(NpcChatWindow::window); ++i)
    {
        NpcChatWindow::hideAt = GetTickCount64() - 1;
        SendMessageW(NpcChatWindow::window, WM_TIMER, NpcChatWindow::closeTimer, 0);
    }
    if (IsWindowVisible(NpcChatWindow::window) || NpcChatWindow::history.empty()) return false;
    NpcChatWindow::Show(f.owner, "Test NPC", "Escape can close every remaining row.");
    for (int i = 0; i < 100 && IsWindowVisible(NpcChatWindow::window); ++i)
        if (!NpcChatWindow::HandleEscape(0)) return false;
    if (IsWindowVisible(NpcChatWindow::window) || NpcChatWindow::hideAt ||
        NpcChatWindow::HandleEscape(0)) return false;
    NpcChatWindow::Show(f.owner, "Test NPC", "Reset cancels the pending close.");
    NpcChatWindow::Reset();
    if (IsWindowVisible(NpcChatWindow::window) || NpcChatWindow::hideAt || !NpcChatWindow::history.empty()) return false;
    puts("PASS: Chat dimensions/save, striped rendering, timed line compaction, expansion, expiry and reset.");
    puts("PASS: Config dropdown selection, legacy alias, Job Master, reopen, persistence and save failure.");
    return true;
}
