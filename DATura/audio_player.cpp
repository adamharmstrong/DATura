#include "stdafx.h"
#include "audio_player.h"
#include "bgw_player.h"
#include "resource.h"
#include "win32_panel_controls.h"
#include "win32_tool_window.h"

#include <commctrl.h>
#include <algorithm>
#include <atomic>
#include <cctype>
#include <climits>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace
{
    constexpr int IDC_AUDIO_SEARCH = 8400;
    constexpr int IDC_AUDIO_KIND = 8401;
    constexpr int IDC_AUDIO_REFRESH = 8402;
    constexpr int IDC_AUDIO_LIST = 8403;
    constexpr int IDC_AUDIO_DETAILS = 8404;
    constexpr int IDC_AUDIO_PLAY = 8405;
    constexpr int IDC_AUDIO_STOP = 8406;
    constexpr int IDC_AUDIO_LOOP = 8407;
    constexpr int IDC_AUDIO_OPEN = 8408;
    constexpr int IDC_AUDIO_STATUS = 8409;
    constexpr UINT_PTR IDT_AUDIO_FINISHED = 1;
    constexpr UINT WM_AUDIO_SCAN_COMPLETE = WM_APP + 43;
    constexpr UINT WM_AUDIO_PREPARE_COMPLETE = WM_APP + 44;

    const char kAudioPlayerClassName[] = "DATuraAudioPlayerClass";

    struct AudioCatalogEntry
    {
        std::string path;
        std::string relativePath;
        std::string bank;
        std::string id;
        std::string name;
        FFXIAudioInfo info;
    };

    struct AudioScanResult
    {
        uint32_t generation = 0;
        std::string rootPath;
        std::vector<AudioCatalogEntry> entries;
        bool completed = false;
    };

    struct AudioPrepareResult
    {
        uint32_t generation = 0;
        size_t catalogIndex = (size_t)-1;
        std::string sourcePath;
        std::string wavPath;
        bool loop = false;
        bool prepared = false;
    };

    HWND g_window = nullptr;
    HWND g_search = nullptr;
    HWND g_kind = nullptr;
    HWND g_list = nullptr;
    HWND g_details = nullptr;
    HWND g_loop = nullptr;
    HWND g_status = nullptr;
    HWND g_owner = nullptr;
    char g_rootPath[MAX_PATH] = {};
    std::vector<AudioCatalogEntry> g_catalog;
    std::vector<size_t> g_visibleCatalog;
    std::string g_catalogRootPath;
    std::string g_currentWavPath;
    std::atomic<uint32_t> g_scanGeneration = 0;
    std::atomic<uint32_t> g_prepareGeneration = 0;
    bool g_scanning = false;
    bool g_preparing = false;
    bool g_playbackAllowed = true;
    bool g_playing = false;
    bool g_suspended = false;
    size_t g_currentIndex = (size_t)-1;

    void SetRootPath(const char* rootPath)
    {
        strcpy_s(g_rootPath, rootPath ? rootPath : "");
        const size_t length = strlen(g_rootPath);
        if (length > 0 && length + 1 < sizeof(g_rootPath) &&
            g_rootPath[length - 1] != '\\' && g_rootPath[length - 1] != '/')
        {
            g_rootPath[length] = '\\';
            g_rootPath[length + 1] = '\0';
        }
    }

    bool EqualsNoCase(const char* left, const char* right)
    {
        if (!left || !right)
            return false;
        const size_t leftLength = strlen(left);
        const size_t rightLength = strlen(right);
        return leftLength == rightLength && _strnicmp(left, right, leftLength) == 0;
    }

    std::string Lowercase(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(),
            [](unsigned char ch) { return (char)std::tolower(ch); });
        return value;
    }

    std::string RelativeAudioPath(const std::string& rootPath, const char* path)
    {
        const size_t rootLength = rootPath.size();
        if (rootLength > 0 && _strnicmp(path, rootPath.c_str(), rootLength) == 0)
            return path + rootLength;
        return path ? path : "";
    }

    std::string FormatDuration(double seconds)
    {
        if (seconds <= 0.0)
            return "--:--";
        if (seconds < 60.0)
        {
            char shortText[32] = {};
            sprintf_s(shortText, "%.1fs", seconds);
            return shortText;
        }
        const unsigned int totalSeconds = (unsigned int)seconds;
        const unsigned int minutes = totalSeconds / 60;
        const unsigned int remainder = totalSeconds % 60;
        char text[32] = {};
        sprintf_s(text, "%u:%02u", minutes, remainder);
        return text;
    }

    std::string FormatAudioDescription(const AudioCatalogEntry& entry)
    {
        char text[384] = {};
        const char* channels = entry.info.channels == 1 ? "mono" :
                               entry.info.channels == 2 ? "stereo" : "channels unknown";
        sprintf_s(text, "%s | %u Hz | %s | %s | %s",
            FFXIAudio_CodecName(entry.info.codec), entry.info.sampleRate, channels,
            FormatDuration(entry.info.durationSeconds).c_str(), entry.relativePath.c_str());
        return text;
    }

    void SetStatus(const std::string& text)
    {
        if (g_status)
            SetWindowTextA(g_status, text.c_str());
    }

    void SetDetailsForSelection()
    {
        if (!g_list || !g_details)
            return;
        const int selected = ListView_GetNextItem(g_list, -1, LVNI_SELECTED);
        if (selected < 0 || (size_t)selected >= g_visibleCatalog.size())
        {
            SetWindowTextA(g_details, "Select an installed music or sound-effect file to preview it.");
            return;
        }
        const AudioCatalogEntry& entry = g_catalog[g_visibleCatalog[(size_t)selected]];
        SetWindowTextA(g_details, FormatAudioDescription(entry).c_str());
        if (!g_playing)
        {
            SendMessageA(g_loop, BM_SETCHECK,
                entry.info.kind == FFXIAudioKind::Music ? BST_CHECKED : BST_UNCHECKED, 0);
        }
    }

    bool AddCatalogFile(const std::string& rootPath, const char* path, const char* bank,
                        const char* id, const char* displayName,
                        std::vector<AudioCatalogEntry>& entries)
    {
        FFXIAudioInfo info;
        if (!FFXIAudio_ReadInfo(path, &info))
            return false;

        AudioCatalogEntry entry;
        entry.path = path;
        entry.relativePath = RelativeAudioPath(rootPath, path);
        entry.bank = bank ? bank : "";
        entry.id = id ? id : "";
        entry.name = displayName ? displayName : "";
        entry.info = info;
        entries.push_back(std::move(entry));
        return true;
    }

    bool ScanMusicBank(const std::string& rootPath, const char* soundRoot,
                       uint32_t generation, std::vector<AudioCatalogEntry>& entries)
    {
        char directory[MAX_PATH] = {};
        char pattern[MAX_PATH] = {};
        sprintf_s(directory, "%s%s\\win\\music\\data", rootPath.c_str(), soundRoot);
        sprintf_s(pattern, "%s\\*.bgw", directory);

        WIN32_FIND_DATAA found = {};
        HANDLE search = FindFirstFileA(pattern, &found);
        if (search == INVALID_HANDLE_VALUE)
            return true;
        do
        {
            if (g_scanGeneration.load() != generation)
            {
                FindClose(search);
                return false;
            }
            if (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                continue;
            char path[MAX_PATH] = {};
            sprintf_s(path, "%s\\%s", directory, found.cFileName);

            int musicId = 0;
            if (sscanf_s(found.cFileName, "music%d.bgw", &musicId) != 1)
                musicId = 0;
            char id[32] = {};
            sprintf_s(id, "%03d", musicId);
            AddCatalogFile(rootPath, path, soundRoot, id, found.cFileName, entries);
        } while (FindNextFileA(search, &found));
        FindClose(search);
        return true;
    }

    bool ScanSoundEffectBank(const std::string& rootPath, const char* soundRoot,
                             uint32_t generation, std::vector<AudioCatalogEntry>& entries)
    {
        char seRoot[MAX_PATH] = {};
        char folderPattern[MAX_PATH] = {};
        sprintf_s(seRoot, "%s%s\\win\\se", rootPath.c_str(), soundRoot);
        sprintf_s(folderPattern, "%s\\se*", seRoot);

        WIN32_FIND_DATAA folder = {};
        HANDLE folderSearch = FindFirstFileA(folderPattern, &folder);
        if (folderSearch == INVALID_HANDLE_VALUE)
            return true;
        do
        {
            if (g_scanGeneration.load() != generation)
            {
                FindClose(folderSearch);
                return false;
            }
            if (!(folder.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || folder.cFileName[0] == '.')
                continue;

            char directory[MAX_PATH] = {};
            char filePattern[MAX_PATH] = {};
            sprintf_s(directory, "%s\\%s", seRoot, folder.cFileName);
            sprintf_s(filePattern, "%s\\*.spw", directory);

            WIN32_FIND_DATAA found = {};
            HANDLE fileSearch = FindFirstFileA(filePattern, &found);
            if (fileSearch == INVALID_HANDLE_VALUE)
                continue;
            do
            {
                if (g_scanGeneration.load() != generation)
                {
                    FindClose(fileSearch);
                    FindClose(folderSearch);
                    return false;
                }
                if (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                    continue;
                char path[MAX_PATH] = {};
                sprintf_s(path, "%s\\%s", directory, found.cFileName);

                int numericId = 0;
                if (_strnicmp(found.cFileName, "se", 2) == 0)
                    numericId = atoi(found.cFileName + 2);
                char id[32] = {};
                sprintf_s(id, "%03d/%03d", numericId / 1000, numericId % 1000);

                char bank[64] = {};
                sprintf_s(bank, "%s / %s", soundRoot, folder.cFileName);
                AddCatalogFile(rootPath, path, bank, id, found.cFileName, entries);
            } while (FindNextFileA(fileSearch, &found));
            FindClose(fileSearch);
        } while (FindNextFileA(folderSearch, &folder));
        FindClose(folderSearch);
        return true;
    }

    bool ScanAudioCatalog(const std::string& rootPath, uint32_t generation,
                          std::vector<AudioCatalogEntry>& entries)
    {
        static const char* soundRoots[] =
        {
            "sound", "sound2", "sound3", "sound4", "sound5", "sound6", "sound9"
        };

        for (const char* soundRoot : soundRoots)
            if (!ScanMusicBank(rootPath, soundRoot, generation, entries))
                return false;
        for (const char* soundRoot : soundRoots)
            if (!ScanSoundEffectBank(rootPath, soundRoot, generation, entries))
                return false;
        return g_scanGeneration.load() == generation;
    }

    bool EntryMatchesFilter(const AudioCatalogEntry& entry, const std::string& filter, int kind)
    {
        if (kind == 1 && entry.info.kind != FFXIAudioKind::Music)
            return false;
        if (kind == 2 && entry.info.kind != FFXIAudioKind::SoundEffect)
            return false;
        if (filter.empty())
            return true;

        std::string haystack = entry.id + " " + entry.name + " " + entry.bank + " " +
                               entry.relativePath + " " + FFXIAudio_CodecName(entry.info.codec);
        return Lowercase(haystack).find(filter) != std::string::npos;
    }

    void PopulateAudioList()
    {
        if (!g_list)
            return;

        char searchText[256] = {};
        if (g_search)
            GetWindowTextA(g_search, searchText, sizeof(searchText));
        const std::string filter = Lowercase(searchText);
        int kind = g_kind ? (int)SendMessageA(g_kind, CB_GETCURSEL, 0, 0) : 0;
        if (kind < 0)
            kind = 0;

        SendMessageA(g_list, WM_SETREDRAW, FALSE, 0);
        SendMessageA(g_list, LVM_SETITEMCOUNT, 0, 0);
        g_visibleCatalog.clear();
        g_visibleCatalog.reserve(g_catalog.size());
        for (size_t index = 0; index < g_catalog.size(); ++index)
        {
            const AudioCatalogEntry& entry = g_catalog[index];
            if (!EntryMatchesFilter(entry, filter, kind))
                continue;
            g_visibleCatalog.push_back(index);
        }
        const int visibleCount = (int)g_visibleCatalog.size();
        SendMessageA(g_list, LVM_SETITEMCOUNT, visibleCount,
                     LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);
        SendMessageA(g_list, WM_SETREDRAW, TRUE, 0);
        InvalidateRect(g_list, nullptr, TRUE);

        char status[160] = {};
        if (g_catalog.empty())
        {
            sprintf_s(status, "No FFXI BGMStream or SeWave files were found under %s", g_rootPath);
        }
        else
        {
            sprintf_s(status, "%d of %zu installed audio files shown.", visibleCount, g_catalog.size());
        }
        SetStatus(status);
        if (visibleCount > 0)
        {
            ListView_SetItemState(g_list, 0, LVIS_SELECTED | LVIS_FOCUSED,
                                  LVIS_SELECTED | LVIS_FOCUSED);
            ListView_EnsureVisible(g_list, 0, FALSE);
        }
        SetDetailsForSelection();
    }

    void SetCatalogControlsEnabled(bool enabled)
    {
        const BOOL value = enabled ? TRUE : FALSE;
        if (g_search) EnableWindow(g_search, value);
        if (g_kind) EnableWindow(g_kind, value);
        if (g_list) EnableWindow(g_list, value);
        if (g_window)
        {
            EnableWindow(GetDlgItem(g_window, IDC_AUDIO_REFRESH), value);
            EnableWindow(GetDlgItem(g_window, IDC_AUDIO_PLAY), value);
            EnableWindow(GetDlgItem(g_window, IDC_AUDIO_OPEN), value);
        }
    }

    void StartAudioCatalogScan()
    {
        if (!g_window || !IsWindow(g_window))
            return;

        const uint32_t generation = g_scanGeneration.fetch_add(1) + 1;
        const HWND targetWindow = g_window;
        const std::string rootPath = g_rootPath;
        g_scanning = true;
        g_catalog.clear();
        g_visibleCatalog.clear();
        g_catalogRootPath.clear();
        SendMessageA(g_list, LVM_SETITEMCOUNT, 0, 0);
        SetDetailsForSelection();
        SetCatalogControlsEnabled(false);
        SetStatus("Scanning installed audio in the background...");

        try
        {
            std::thread([targetWindow, generation, rootPath]()
            {
                std::unique_ptr<AudioScanResult> result(new AudioScanResult());
                result->generation = generation;
                result->rootPath = rootPath;
                result->completed = ScanAudioCatalog(rootPath, generation, result->entries);
                if (g_scanGeneration.load() != generation ||
                    !PostMessageA(targetWindow, WM_AUDIO_SCAN_COMPLETE, 0, (LPARAM)result.get()))
                {
                    return;
                }
                result.release();
            }).detach();
        }
        catch (...)
        {
            g_scanning = false;
            SetCatalogControlsEnabled(true);
            SetStatus("Could not start the installed-audio scan.");
        }
    }

    bool SelectedCatalogIndex(size_t& index)
    {
        if (!g_list)
            return false;
        const int selected = ListView_GetNextItem(g_list, -1, LVNI_SELECTED);
        if (selected < 0 || (size_t)selected >= g_visibleCatalog.size())
            return false;
        index = g_visibleCatalog[(size_t)selected];
        return true;
    }

    void StopPlayback(bool updateStatus)
    {
        g_prepareGeneration.fetch_add(1);
        KillTimer(g_window, IDT_AUDIO_FINISHED);
        BGM_Stop();
        g_preparing = false;
        g_playing = false;
        g_suspended = false;
        g_currentIndex = (size_t)-1;
        g_currentWavPath.clear();
        if (updateStatus)
            SetStatus("Playback stopped.");
    }

    void ScheduleFinishedTimer(const AudioCatalogEntry& entry, bool loop)
    {
        KillTimer(g_window, IDT_AUDIO_FINISHED);
        if (loop || entry.info.durationSeconds <= 0.0)
            return;
        const double milliseconds = entry.info.durationSeconds * 1000.0 + 250.0;
        const UINT interval = (UINT)std::min(milliseconds, (double)UINT_MAX);
        SetTimer(g_window, IDT_AUDIO_FINISHED, std::max(1u, interval), nullptr);
    }

    void PrepareSelectedEntry(size_t index, bool loop)
    {
        const uint32_t generation = g_prepareGeneration.fetch_add(1) + 1;
        const HWND targetWindow = g_window;
        const std::string sourcePath = g_catalog[index].path;
        const std::string displayName = g_catalog[index].name;

        KillTimer(g_window, IDT_AUDIO_FINISHED);
        BGM_Stop();
        g_preparing = true;
        g_playing = false;
        g_suspended = false;
        g_currentIndex = index;
        g_currentWavPath.clear();
        SetStatus(std::string("Preparing ") + displayName + " in the background...");

        try
        {
            std::thread([targetWindow, generation, index, sourcePath, loop]()
            {
                std::unique_ptr<AudioPrepareResult> result(new AudioPrepareResult());
                result->generation = generation;
                result->catalogIndex = index;
                result->sourcePath = sourcePath;
                result->loop = loop;
                char wavPath[MAX_PATH] = {};
                result->prepared = FFXIAudio_PrepareFile(sourcePath.c_str(), wavPath, sizeof(wavPath));
                if (result->prepared)
                    result->wavPath = wavPath;
                if (g_prepareGeneration.load() != generation ||
                    !PostMessageA(targetWindow, WM_AUDIO_PREPARE_COMPLETE, 0, (LPARAM)result.get()))
                {
                    return;
                }
                result.release();
            }).detach();
        }
        catch (...)
        {
            g_preparing = false;
            g_currentIndex = (size_t)-1;
            SetStatus("Could not start background audio preparation.");
        }
    }

    void PlaySelectedEntry()
    {
        size_t index = 0;
        if (!SelectedCatalogIndex(index))
        {
            SetStatus("Select an audio file first.");
            return;
        }
        if (!g_playbackAllowed)
        {
            SetStatus("Playback is disabled by the current DATura audio settings.");
            return;
        }

        const AudioCatalogEntry& entry = g_catalog[index];
        if (entry.info.codec == FFXIAudioCodec::ATRAC3 ||
            entry.info.codec == FFXIAudioCodec::Unknown)
        {
            SetStatus("This file's codec is recognized but is not supported by the preview player.");
            return;
        }

        const bool loop = SendMessageA(g_loop, BM_GETCHECK, 0, 0) == BST_CHECKED;
        PrepareSelectedEntry(index, loop);
    }

    void AddExternalAudioFile()
    {
        char path[MAX_PATH] = {};
        OPENFILENAMEA open = {};
        open.lStructSize = sizeof(open);
        open.hwndOwner = g_window;
        open.lpstrFile = path;
        open.nMaxFile = sizeof(path);
        open.lpstrFilter = "FFXI Audio (*.bgw;*.spw)\0*.bgw;*.spw\0All Files (*.*)\0*.*\0\0";
        open.nFilterIndex = 1;
        open.lpstrInitialDir = g_rootPath[0] ? g_rootPath : nullptr;
        open.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
        if (!GetOpenFileNameA(&open))
            return;

        for (size_t index = 0; index < g_catalog.size(); ++index)
        {
            if (EqualsNoCase(g_catalog[index].path.c_str(), path))
            {
                SetWindowTextA(g_search, "");
                SendMessageA(g_kind, CB_SETCURSEL, 0, 0);
                PopulateAudioList();
                return;
            }
        }

        const char* fileName = strrchr(path, '\\');
        fileName = fileName ? fileName + 1 : path;
        if (!AddCatalogFile(g_rootPath, path, "External", "--", fileName, g_catalog))
        {
            SetStatus("The selected file is not a recognized BGMStream or SeWave file.");
            return;
        }
        SetWindowTextA(g_search, fileName);
        SendMessageA(g_kind, CB_SETCURSEL, 0, 0);
        PopulateAudioList();
    }

    void LayoutAudioPlayer(HWND window)
    {
        RECT client = {};
        GetClientRect(window, &client);
        const int width = client.right - client.left;
        const int height = client.bottom - client.top;
        const int margin = 10;
        const int topHeight = 24;
        const int bottomHeight = 88;
        const int listY = margin + topHeight + 8;
        const int listHeight = std::max(100, height - listY - bottomHeight);

        HWND label = GetDlgItem(window, -1);
        if (label)
            MoveWindow(label, margin, margin + 3, 38, 18, TRUE);
        if (g_search)
            MoveWindow(g_search, margin + 42, margin, std::max(120, width - 410), topHeight, TRUE);
        if (g_kind)
            MoveWindow(g_kind, std::max(margin + 166, width - 246), margin, 150, 200, TRUE);
        HWND refresh = GetDlgItem(window, IDC_AUDIO_REFRESH);
        if (refresh)
            MoveWindow(refresh, std::max(margin + 320, width - 86), margin, 76, topHeight, TRUE);
        if (g_list)
            MoveWindow(g_list, margin, listY, std::max(100, width - margin * 2), listHeight, TRUE);
        if (g_details)
            MoveWindow(g_details, margin, listY + listHeight + 7,
                       std::max(100, width - margin * 2), 22, TRUE);

        const int buttonY = height - 37;
        HWND play = GetDlgItem(window, IDC_AUDIO_PLAY);
        HWND stop = GetDlgItem(window, IDC_AUDIO_STOP);
        HWND open = GetDlgItem(window, IDC_AUDIO_OPEN);
        if (play) MoveWindow(play, margin, buttonY, 76, 26, TRUE);
        if (stop) MoveWindow(stop, margin + 82, buttonY, 76, 26, TRUE);
        if (g_loop) MoveWindow(g_loop, margin + 170, buttonY + 3, 72, 22, TRUE);
        if (open) MoveWindow(open, margin + 250, buttonY, 104, 26, TRUE);
        if (g_status)
            MoveWindow(g_status, margin + 368, buttonY + 4,
                       std::max(100, width - margin - (margin + 368)), 20, TRUE);
    }

    LRESULT CALLBACK AudioPlayerWndProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch (message)
        {
        case WM_CREATE:
        {
            g_window = window;
            Win32PanelControls::AddPanelControl(
                window, "STATIC", "Filter:", 0, -1, 0, 0, 0, 0);
            g_search = Win32PanelControls::AddPanelControl(
                window, "EDIT", "", WS_TABSTOP | ES_AUTOHSCROLL,
                IDC_AUDIO_SEARCH, 0, 0, 0, 0, WS_EX_CLIENTEDGE);
            SendMessageA(g_search, EM_SETCUEBANNER, TRUE,
                         (LPARAM)L"Name, ID, bank, format, or path");
            g_kind = Win32PanelControls::AddPanelControl(
                window, "COMBOBOX", "",
                WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
                IDC_AUDIO_KIND, 0, 0, 0, 0);
            SendMessageA(g_kind, CB_ADDSTRING, 0, (LPARAM)"All audio");
            SendMessageA(g_kind, CB_ADDSTRING, 0, (LPARAM)"Music");
            SendMessageA(g_kind, CB_ADDSTRING, 0, (LPARAM)"Sound effects");
            SendMessageA(g_kind, CB_SETCURSEL, 0, 0);
            Win32PanelControls::AddPanelControl(
                window, "BUTTON", "Refresh", WS_TABSTOP,
                IDC_AUDIO_REFRESH, 0, 0, 0, 0);

            g_list = Win32PanelControls::AddPanelControl(
                window, WC_LISTVIEWA, "",
                WS_TABSTOP | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SINGLESEL | LVS_OWNERDATA,
                IDC_AUDIO_LIST, 0, 0, 0, 0, WS_EX_CLIENTEDGE);
            ListView_SetExtendedListViewStyle(g_list,
                LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER | LVS_EX_LABELTIP);
            const char* headings[] = { "Type", "ID", "Name", "Bank", "Format", "Length", "Path" };
            const int widths[] = { 64, 78, 150, 126, 74, 66, 470 };
            for (int columnIndex = 0; columnIndex < 7; ++columnIndex)
            {
                LVCOLUMNA column = {};
                column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
                column.iSubItem = columnIndex;
                column.cx = widths[columnIndex];
                column.pszText = const_cast<LPSTR>(headings[columnIndex]);
                SendMessageA(g_list, LVM_INSERTCOLUMNA, columnIndex, (LPARAM)&column);
            }

            g_details = Win32PanelControls::AddPanelControl(window, "STATIC",
                "Select an installed music or sound-effect file to preview it.",
                SS_LEFTNOWORDWRAP, IDC_AUDIO_DETAILS, 0, 0, 0, 0);
            Win32PanelControls::AddPanelControl(
                window, "BUTTON", "Play", WS_TABSTOP,
                IDC_AUDIO_PLAY, 0, 0, 0, 0);
            Win32PanelControls::AddPanelControl(
                window, "BUTTON", "Stop", WS_TABSTOP,
                IDC_AUDIO_STOP, 0, 0, 0, 0);
            g_loop = Win32PanelControls::AddPanelControl(
                window, "BUTTON", "Loop", WS_TABSTOP | BS_AUTOCHECKBOX,
                IDC_AUDIO_LOOP, 0, 0, 0, 0);
            Win32PanelControls::AddPanelControl(
                window, "BUTTON", "Open File...", WS_TABSTOP,
                IDC_AUDIO_OPEN, 0, 0, 0, 0);
            g_status = Win32PanelControls::AddPanelControl(
                window, "STATIC", "Scanning installed audio...",
                SS_LEFTNOWORDWRAP, IDC_AUDIO_STATUS, 0, 0, 0, 0);

            const HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            EnumChildWindows(window, [](HWND child, LPARAM fontParam) -> BOOL
            {
                SendMessage(child, WM_SETFONT, (WPARAM)fontParam, TRUE);
                return TRUE;
            }, (LPARAM)font);
            LayoutAudioPlayer(window);
            if (!g_catalog.empty() && EqualsNoCase(g_catalogRootPath.c_str(), g_rootPath))
            {
                SetCatalogControlsEnabled(true);
                PopulateAudioList();
            }
            else
            {
                StartAudioCatalogScan();
            }
            return 0;
        }

        case WM_SIZE:
            LayoutAudioPlayer(window);
            return 0;

        case WM_AUDIO_SCAN_COMPLETE:
        {
            std::unique_ptr<AudioScanResult> result((AudioScanResult*)lParam);
            if (!result || result->generation != g_scanGeneration.load() || window != g_window)
                return 0;

            g_scanning = false;
            SetCatalogControlsEnabled(true);
            if (!result->completed)
            {
                SetStatus("The installed-audio scan was canceled.");
                return 0;
            }
            g_catalog = std::move(result->entries);
            g_catalogRootPath = std::move(result->rootPath);
            PopulateAudioList();
            return 0;
        }

        case WM_AUDIO_PREPARE_COMPLETE:
        {
            std::unique_ptr<AudioPrepareResult> result((AudioPrepareResult*)lParam);
            if (!result || result->generation != g_prepareGeneration.load() ||
                window != g_window || result->catalogIndex >= g_catalog.size() ||
                !EqualsNoCase(g_catalog[result->catalogIndex].path.c_str(), result->sourcePath.c_str()))
            {
                return 0;
            }

            g_preparing = false;
            if (!result->prepared)
            {
                g_currentIndex = (size_t)-1;
                SetStatus("Could not decode the selected audio file.");
                return 0;
            }
            if (!g_playbackAllowed)
            {
                g_currentIndex = (size_t)-1;
                SetStatus("Playback is disabled by the current DATura audio settings.");
                return 0;
            }
            if (!FFXIAudio_PlayPreparedFile(result->wavPath.c_str(), result->loop))
            {
                g_currentIndex = (size_t)-1;
                SetStatus("Could not start the prepared audio file.");
                return 0;
            }

            const AudioCatalogEntry& entry = g_catalog[result->catalogIndex];
            g_currentWavPath = std::move(result->wavPath);
            g_playing = true;
            g_suspended = false;
            g_currentIndex = result->catalogIndex;
            SetStatus(std::string("Playing ") + entry.name +
                      (result->loop ? " (looping)." : "."));
            ScheduleFinishedTimer(entry, result->loop);
            return 0;
        }

        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
            case IDC_AUDIO_SEARCH:
                if (HIWORD(wParam) == EN_CHANGE)
                    PopulateAudioList();
                return 0;
            case IDC_AUDIO_KIND:
                if (HIWORD(wParam) == CBN_SELCHANGE)
                    PopulateAudioList();
                return 0;
            case IDC_AUDIO_REFRESH:
                if (HIWORD(wParam) == BN_CLICKED)
                {
                    StopPlayback(false);
                    StartAudioCatalogScan();
                }
                return 0;
            case IDC_AUDIO_PLAY:
                if (HIWORD(wParam) == BN_CLICKED)
                    PlaySelectedEntry();
                return 0;
            case IDC_AUDIO_STOP:
                if (HIWORD(wParam) == BN_CLICKED)
                    StopPlayback(true);
                return 0;
            case IDC_AUDIO_OPEN:
                if (HIWORD(wParam) == BN_CLICKED)
                    AddExternalAudioFile();
                return 0;
            }
            break;

        case WM_NOTIFY:
            if (((LPNMHDR)lParam)->idFrom == IDC_AUDIO_LIST)
            {
                if (((LPNMHDR)lParam)->code == LVN_GETDISPINFOA)
                {
                    NMLVDISPINFOA* display = (NMLVDISPINFOA*)lParam;
                    if ((display->item.mask & LVIF_TEXT) && display->item.pszText &&
                        display->item.iItem >= 0 &&
                        (size_t)display->item.iItem < g_visibleCatalog.size())
                    {
                        const AudioCatalogEntry& entry =
                            g_catalog[g_visibleCatalog[(size_t)display->item.iItem]];
                        std::string text;
                        switch (display->item.iSubItem)
                        {
                        case 0: text = entry.info.kind == FFXIAudioKind::Music ? "Music" : "SFX"; break;
                        case 1: text = entry.id; break;
                        case 2: text = entry.name; break;
                        case 3: text = entry.bank; break;
                        case 4: text = FFXIAudio_CodecName(entry.info.codec); break;
                        case 5: text = FormatDuration(entry.info.durationSeconds); break;
                        case 6: text = entry.relativePath; break;
                        }
                        strcpy_s(display->item.pszText, display->item.cchTextMax, text.c_str());
                    }
                }
                else if (((LPNMHDR)lParam)->code == LVN_ITEMCHANGED)
                    SetDetailsForSelection();
                else if (((LPNMHDR)lParam)->code == NM_DBLCLK)
                    PlaySelectedEntry();
                else if (((LPNMHDR)lParam)->code == LVN_KEYDOWN)
                {
                    const NMLVKEYDOWN* key = (const NMLVKEYDOWN*)lParam;
                    if (key->wVKey == VK_RETURN || key->wVKey == VK_SPACE)
                        PlaySelectedEntry();
                }
            }
            return 0;

        case WM_TIMER:
            if (wParam == IDT_AUDIO_FINISHED)
            {
                KillTimer(window, IDT_AUDIO_FINISHED);
                g_playing = false;
                g_suspended = false;
                g_currentIndex = (size_t)-1;
                g_currentWavPath.clear();
                SetStatus("Playback finished.");
                return 0;
            }
            break;

        case WM_CLOSE:
            DestroyWindow(window);
            return 0;

        case WM_DESTROY:
            g_scanGeneration.fetch_add(1);
            StopPlayback(false);
            g_scanning = false;
            g_window = nullptr;
            g_search = nullptr;
            g_kind = nullptr;
            g_list = nullptr;
            g_details = nullptr;
            g_loop = nullptr;
            g_status = nullptr;
            g_visibleCatalog.clear();
            if (g_owner)
                PostMessage(g_owner, WM_DATURA_AUDIO_PLAYER_CLOSED, 0, 0);
            return 0;
        }
        return DefWindowProcA(window, message, wParam, lParam);
    }
}

void AudioPlayer_Show(HWND owner, const char* ffxiRootPath)
{
    g_owner = owner;
    SetRootPath(ffxiRootPath);
    if (g_window && IsWindow(g_window))
    {
        Win32ToolWindow::Show(g_window, true, SW_RESTORE);
        return;
    }

    BGM_Stop();
    Win32ToolWindow::Spec spec =
    {
        AudioPlayerWndProc, kAudioPlayerClassName, "DATura Music / SFX Player", 1180, 720,
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        (HBRUSH)(COLOR_WINDOW + 1)
    };
    spec.extendedStyle = WS_EX_APPWINDOW;
    spec.icon = LoadIcon(GetModuleHandleA(nullptr), MAKEINTRESOURCE(IDI_DATURA));
    spec.classStyle = CS_HREDRAW | CS_VREDRAW;
    g_window = Win32ToolWindow::Create(owner, spec);
    if (g_window)
    {
        Win32ToolWindow::Show(g_window, false);
        UpdateWindow(g_window);
        SetForegroundWindow(g_window);
    }
}

void AudioPlayer_SetRootPath(const char* ffxiRootPath)
{
    const std::string previous = g_rootPath;
    SetRootPath(ffxiRootPath);
    if (!AudioPlayer_IsOpen() || EqualsNoCase(previous.c_str(), g_rootPath))
        return;

    StopPlayback(false);
    SetWindowTextA(g_search, "");
    SendMessageA(g_kind, CB_SETCURSEL, 0, 0);
    StartAudioCatalogScan();
}

bool AudioPlayer_IsOpen()
{
    return g_window && IsWindow(g_window);
}

bool AudioPlayer_OwnsWindow(HWND window)
{
    return AudioPlayer_IsOpen() && (window == g_window || IsChild(g_window, window));
}

void AudioPlayer_SetPlaybackAllowed(bool allowed)
{
    g_playbackAllowed = allowed;
    if (!AudioPlayer_IsOpen())
        return;

    if (!allowed && g_preparing)
    {
        g_prepareGeneration.fetch_add(1);
        g_preparing = false;
        g_currentIndex = (size_t)-1;
        SetStatus("Audio preparation canceled by DATura's audio/background settings.");
        return;
    }
    if (!g_playing)
        return;

    if (!allowed && !g_suspended)
    {
        BGM_Stop();
        KillTimer(g_window, IDT_AUDIO_FINISHED);
        g_suspended = true;
        SetStatus("Playback paused by DATura's audio/background settings.");
    }
    else if (allowed && g_suspended && g_currentIndex < g_catalog.size() &&
             !g_currentWavPath.empty())
    {
        const bool loop = SendMessageA(g_loop, BM_GETCHECK, 0, 0) == BST_CHECKED;
        if (FFXIAudio_PlayPreparedFile(g_currentWavPath.c_str(), loop))
        {
            g_suspended = false;
            SetStatus(std::string("Playing ") + g_catalog[g_currentIndex].name +
                      (loop ? " (looping)." : "."));
            ScheduleFinishedTimer(g_catalog[g_currentIndex], loop);
        }
    }
}

void AudioPlayer_StopPlayback()
{
    if (AudioPlayer_IsOpen())
        StopPlayback(true);
    else
        BGM_Stop();
}
