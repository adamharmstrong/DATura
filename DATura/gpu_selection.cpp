#include "stdafx.h"
#include "gpu_selection.h"

#include <d3d9.h>
#include <dxgi.h>
#include <algorithm>
#include <cstdio>
#include <cstring>

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "advapi32.lib")

namespace GpuSelection
{
namespace
{
constexpr char kSettingsKey[] = "Software\\FFXIViewer";
constexpr char kNameValue[] = "GraphicsDeviceName";
constexpr char kVendorValue[] = "GraphicsDeviceVendor";
constexpr char kDeviceValue[] = "GraphicsDeviceId";
constexpr wchar_t kWindowsPreferencesKey[] = L"Software\\Microsoft\\DirectX\\UserGpuPreferences";
constexpr wchar_t kPreferenceToken[] = L"GpuPreference=";

std::string g_activeAdapterName;
bool g_restartPending = false;

std::string Narrow(const wchar_t* text)
{
    const int length = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    if (length <= 1)
        return {};
    std::string result(static_cast<size_t>(length - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text, -1, &result[0], length, nullptr, nullptr);
    return result;
}

std::wstring ExecutablePath()
{
    wchar_t path[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);
    return length && length < MAX_PATH ? std::wstring(path, length) : std::wstring();
}

bool SameLuid(const LUID& a, const LUID& b)
{
    return a.LowPart == b.LowPart && a.HighPart == b.HighPart;
}

std::vector<Adapter> Enumerate()
{
    std::vector<Adapter> adapters;
    IDXGIFactory1* factory = nullptr;
    if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&factory))))
        return adapters;

    const auto add = [&adapters](IDXGIAdapter1* adapter)
    {
        DXGI_ADAPTER_DESC1 description = {};
        if (FAILED(adapter->GetDesc1(&description)) ||
            (description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE))
            return;
        for (const Adapter& existing : adapters)
            if (SameLuid(existing.luid, description.AdapterLuid))
                return;
        Adapter entry;
        entry.name = Narrow(description.Description);
        entry.vendorId = description.VendorId;
        entry.deviceId = description.DeviceId;
        entry.luid = description.AdapterLuid;
        entry.dedicatedVideoMemory = description.DedicatedVideoMemory;
        adapters.push_back(entry);
    };

    // EnumAdapterByGpuPreference is not used for classification: Windows
    // reorders it to honour this process's own saved preference, so with
    // "Power saving" stored the integrated GPU is reported as high performance.
    IDXGIAdapter1* adapter = nullptr;
    for (UINT index = 0; SUCCEEDED(factory->EnumAdapters1(index, &adapter)); ++index)
    {
        add(adapter);
        adapter->Release();
    }
    factory->Release();

    // Windows' preference classes split integrated (shared memory) from
    // discrete (dedicated memory) GPUs, so classify by dedicated video memory.
    std::stable_sort(adapters.begin(), adapters.end(), [](const Adapter& a, const Adapter& b)
        { return a.dedicatedVideoMemory > b.dedicatedVideoMemory; });
    if (adapters.size() >= 2 &&
        adapters.front().dedicatedVideoMemory > adapters.back().dedicatedVideoMemory)
    {
        adapters.front().windowsPreference = WindowsPreferenceHighPerformance;
        adapters.back().windowsPreference = WindowsPreferencePowerSaving;
    }
    return adapters;
}

DWORD ReadDword(HKEY key, const char* name)
{
    DWORD value = 0;
    DWORD size = sizeof(value);
    DWORD type = 0;
    if (RegQueryValueExA(key, name, nullptr, &type, reinterpret_cast<BYTE*>(&value), &size) != ERROR_SUCCESS ||
        type != REG_DWORD)
        return 0;
    return value;
}

std::wstring ReadWindowsPreferenceString()
{
    const std::wstring executable = ExecutablePath();
    HKEY key = nullptr;
    if (executable.empty() ||
        RegOpenKeyExW(HKEY_CURRENT_USER, kWindowsPreferencesKey, 0, KEY_READ, &key) != ERROR_SUCCESS)
        return {};
    wchar_t value[512] = {};
    DWORD size = sizeof(value) - sizeof(wchar_t);
    DWORD type = 0;
    const bool ok = RegQueryValueExW(key, executable.c_str(), nullptr, &type,
        reinterpret_cast<BYTE*>(value), &size) == ERROR_SUCCESS && type == REG_SZ;
    RegCloseKey(key);
    return ok ? std::wstring(value) : std::wstring();
}

std::string AdapterNameForLuid(const LUID& luid)
{
    for (const Adapter& adapter : Adapters())
        if (SameLuid(adapter.luid, luid))
            return adapter.name;
    return {};
}
}

const std::vector<Adapter>& Adapters()
{
    static const std::vector<Adapter> adapters = Enumerate();
    return adapters;
}

Choice LoadChoice()
{
    Choice choice;
    HKEY key = nullptr;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, kSettingsKey, 0, KEY_READ, &key) != ERROR_SUCCESS)
        return choice;
    char name[256] = {};
    DWORD size = sizeof(name) - 1;
    DWORD type = 0;
    if (RegQueryValueExA(key, kNameValue, nullptr, &type, reinterpret_cast<BYTE*>(name), &size) == ERROR_SUCCESS &&
        type == REG_SZ)
        choice.name = name;
    choice.vendorId = ReadDword(key, kVendorValue);
    choice.deviceId = ReadDword(key, kDeviceValue);
    RegCloseKey(key);
    return choice;
}

void SaveChoice(const Choice& choice)
{
    HKEY key = nullptr;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, kSettingsKey, 0, nullptr, 0,
            KEY_WRITE, nullptr, &key, nullptr) != ERROR_SUCCESS)
        return;
    RegSetValueExA(key, kNameValue, 0, REG_SZ, reinterpret_cast<const BYTE*>(choice.name.c_str()),
        static_cast<DWORD>(choice.name.size() + 1));
    const DWORD vendor = choice.vendorId;
    const DWORD device = choice.deviceId;
    RegSetValueExA(key, kVendorValue, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&vendor), sizeof(vendor));
    RegSetValueExA(key, kDeviceValue, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&device), sizeof(device));
    RegCloseKey(key);
}

bool IsAutomatic(const Choice& choice)
{
    return choice.name.empty();
}

bool Matches(const Choice& choice, const Adapter& adapter)
{
    if (IsAutomatic(choice))
        return false;
    if (choice.vendorId && choice.deviceId)
        return choice.vendorId == adapter.vendorId && choice.deviceId == adapter.deviceId;
    return _stricmp(choice.name.c_str(), adapter.name.c_str()) == 0;
}

int ReadWindowsPreference()
{
    const std::wstring value = ReadWindowsPreferenceString();
    const size_t at = value.find(kPreferenceToken);
    if (at == std::wstring::npos)
        return WindowsPreferenceNone;
    const wchar_t digit = at + wcslen(kPreferenceToken) < value.size() ?
        value[at + wcslen(kPreferenceToken)] : L'\0';
    return digit >= L'0' && digit <= L'2' ? digit - L'0' : WindowsPreferenceNone;
}

bool WriteWindowsPreference(const int preference)
{
    const std::wstring executable = ExecutablePath();
    if (executable.empty())
        return false;

    // Keep any other per-application DirectX settings Windows stored here.
    std::wstring value = ReadWindowsPreferenceString();
    const size_t at = value.find(kPreferenceToken);
    if (at != std::wstring::npos)
    {
        const size_t end = value.find(L';', at);
        value.erase(at, end == std::wstring::npos ? std::wstring::npos : end - at + 1);
    }
    if (preference >= WindowsPreferenceDefault && preference <= WindowsPreferenceHighPerformance)
        value = kPreferenceToken + std::to_wstring(preference) + L";" + value;

    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kWindowsPreferencesKey, 0, nullptr, 0,
            KEY_WRITE, nullptr, &key, nullptr) != ERROR_SUCCESS)
        return false;
    const LSTATUS status = value.empty() ?
        RegDeleteValueW(key, executable.c_str()) :
        RegSetValueExW(key, executable.c_str(), 0, REG_SZ, reinterpret_cast<const BYTE*>(value.c_str()),
            static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(key);
    return status == ERROR_SUCCESS || (value.empty() && status == ERROR_FILE_NOT_FOUND);
}

bool Apply(const Choice& choice)
{
    int preference = WindowsPreferenceHighPerformance;
    if (!IsAutomatic(choice))
    {
        preference = WindowsPreferenceDefault;
        for (const Adapter& adapter : Adapters())
            if (Matches(choice, adapter))
                preference = adapter.windowsPreference;
    }
    SaveChoice(choice);
    return WriteWindowsPreference(preference);
}

UINT ChooseD3D9Adapter(IDirect3D9* const d3d)
{
    const Choice choice = LoadChoice();
    if (!d3d || IsAutomatic(choice) || d3d->GetAdapterCount() < 2)
        return D3DADAPTER_DEFAULT;
    const auto matchesAdapter = [&](const UINT index)
    {
        D3DADAPTER_IDENTIFIER9 identifier = {};
        if (FAILED(d3d->GetAdapterIdentifier(index, 0, &identifier)))
            return false;
        Adapter candidate;
        candidate.name = identifier.Description;
        candidate.vendorId = identifier.VendorId;
        candidate.deviceId = identifier.DeviceId;
        return Matches(choice, candidate);
    };
    // On hybrid laptops the Windows preference already routes the primary
    // display's adapter to the chosen GPU. Only move to another adapter
    // (another monitor's) when the default one does not match.
    if (matchesAdapter(D3DADAPTER_DEFAULT))
        return D3DADAPTER_DEFAULT;
    for (UINT index = 0; index < d3d->GetAdapterCount(); ++index)
        if (matchesAdapter(index))
            return index;
    return D3DADAPTER_DEFAULT;
}

void RecordActiveD3D9Adapter(IDirect3D9* const d3d, const UINT adapter)
{
    g_activeAdapterName.clear();
    // The Ex interface reports the LUID of the GPU that renders for this
    // process, which on hybrid laptops differs from the display adapter.
    IDirect3D9Ex* d3dEx = nullptr;
    LUID luid = {};
    if (SUCCEEDED(Direct3DCreate9Ex(D3D_SDK_VERSION, &d3dEx)) && d3dEx &&
        SUCCEEDED(d3dEx->GetAdapterLUID(adapter, &luid)))
        g_activeAdapterName = AdapterNameForLuid(luid);
    if (d3dEx)
        d3dEx->Release();

    D3DADAPTER_IDENTIFIER9 identifier = {};
    if (g_activeAdapterName.empty() && d3d &&
        SUCCEEDED(d3d->GetAdapterIdentifier(adapter, 0, &identifier)))
        g_activeAdapterName = identifier.Description;

    char message[320] = {};
    std::snprintf(message, sizeof(message), "DATura rendering on: %s\n", g_activeAdapterName.c_str());
    OutputDebugStringA(message);
}

std::string ActiveAdapterName()
{
    return g_activeAdapterName;
}

bool RequiresRestart(const Choice& choice)
{
    const auto& adapters = Adapters();
    const Adapter* target = nullptr;
    for (const Adapter& adapter : adapters)
        if (IsAutomatic(choice) ? adapter.windowsPreference == WindowsPreferenceHighPerformance :
            Matches(choice, adapter))
        {
            target = &adapter;
            break;
        }
    if (!target)
        return false;
    return _stricmp(target->name.c_str(), g_activeAdapterName.c_str()) != 0;
}

void RequestRestart()
{
    g_restartPending = true;
}

void LaunchPendingRestart()
{
    if (!g_restartPending)
        return;
    g_restartPending = false;
    const std::wstring executable = ExecutablePath();
    if (executable.empty())
        return;
    std::wstring commandLine = GetCommandLineW();
    STARTUPINFOW startup = {};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process = {};
    if (CreateProcessW(executable.c_str(), &commandLine[0], nullptr, nullptr, FALSE, 0,
            nullptr, nullptr, &startup, &process))
    {
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
    }
}
}
