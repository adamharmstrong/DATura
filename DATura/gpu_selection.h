#pragma once

#include <windows.h>
#include <string>
#include <vector>

struct IDirect3D9;

// Graphics-device selection. On hybrid laptops (NVIDIA Optimus, AMD
// switchable graphics) D3D9 only exposes the display adapter, and the GPU
// that actually renders is chosen by Windows when the process starts. A
// choice therefore writes the per-application Windows GPU preference and
// takes effect after a restart. Desktops with several display adapters
// expose each one to D3D9, so the saved choice also selects the adapter
// index directly.
namespace GpuSelection
{
// Values of the Windows "GpuPreference" per-application setting.
enum WindowsPreference
{
    WindowsPreferenceNone = -1,       // No entry: Windows/driver default.
    WindowsPreferenceDefault = 0,
    WindowsPreferencePowerSaving = 1,
    WindowsPreferenceHighPerformance = 2
};

struct Adapter
{
    std::string name;
    unsigned vendorId = 0;
    unsigned deviceId = 0;
    LUID luid = {};
    unsigned long long dedicatedVideoMemory = 0;
    // The Windows preference that routes rendering to this adapter, or
    // WindowsPreferenceDefault when no preference class selects it.
    int windowsPreference = WindowsPreferenceDefault;
};

// The saved choice. An empty name means "Automatic (prefer high performance)".
struct Choice
{
    std::string name;
    unsigned vendorId = 0;
    unsigned deviceId = 0;
};

// Hardware adapters, highest-performance first. Software adapters are skipped.
const std::vector<Adapter>& Adapters();

Choice LoadChoice();
void SaveChoice(const Choice& choice);
bool IsAutomatic(const Choice& choice);
bool Matches(const Choice& choice, const Adapter& adapter);

int ReadWindowsPreference();
bool WriteWindowsPreference(int preference);
// Persists the choice and the matching Windows preference.
bool Apply(const Choice& choice);

// Picks the D3D9 adapter for the saved choice; D3DADAPTER_DEFAULT otherwise.
UINT ChooseD3D9Adapter(IDirect3D9* d3d);
// Records the adapter the device was created on, for "Rendering on" text.
void RecordActiveD3D9Adapter(IDirect3D9* d3d, UINT adapter);
// Name of the GPU rendering this process, or empty before device creation.
std::string ActiveAdapterName();
// True when the choice should render on a different GPU than the active one.
bool RequiresRestart(const Choice& choice);

// Schedules a relaunch of this executable after the main loop exits.
void RequestRestart();
void LaunchPendingRestart();
}
