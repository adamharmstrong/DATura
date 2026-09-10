#pragma once

#include <windows.h>

namespace InputController
{
enum class DragMode
{
    None,
    Orbit,
    Pan,
};

enum class Action
{
    None,
    Back,
    Confirm,
    OpenDat,
    ToggleGameMode,
    UnstickPlayer,
    CycleWeather,
    ToggleCameraDebugOverlay,
};

struct MovementSnapshot
{
    bool fastRunning = false;
    bool running = false;
    float right = 0.0f;
    float forward = 0.0f;
    float vertical = 0.0f;
    float strafe = 0.0f;
    bool boost = false;
    bool slow = false;
};

struct DragDelta
{
    DragMode mode = DragMode::None;
    int x = 0;
    int y = 0;
};

struct State
{
    bool leftMouseHeld = false;
    bool rightMouseHeld = false;
    bool pendingWorldClick = false;
    bool fastRunning = false;
    bool altHeld = false;
    bool running = false;
    HWND window = nullptr;
    DragMode dragMode = DragMode::None;
    POINT lastMouse = {};
    POINT clientMouse = { -1, -1 };
    bool wantsCursorHidden = false;
    bool cursorHidden = false;
    bool hardwareCursorEnabled = true;
    bool forward = false;
    bool backward = false;
    bool left = false;
    bool right = false;
    bool up = false;
    bool down = false;
    bool boost = false;
    bool slow = false;
    bool cameraDebugToggle = false;
    bool jumpHeld = false;
    bool jumpRequested = false;
};

void Initialize(State& state, HWND window, bool hardwareCursorEnabled);
void Shutdown(State& state);
void SetHardwareCursorEnabled(State& state, bool enabled);
void BeginOrbit(State& state, int x, int y);
void BeginPan(State& state, int x, int y);
void EndDrag(State& state);
void CaptureChanged(State& state, HWND newCapture);
void FocusLost(State& state);
DragDelta MouseMoved(State& state, int x, int y);
void MouseLeft(State& state);
Action KeyDown(State& state, unsigned int keyCode);
void KeyUp(State& state, unsigned int keyCode);
MovementSnapshot Movement(const State& state);
bool IsMovementActive(const State& state);
bool ConsumeJump(State& state);
bool MouseForwardActive(const State& state);
bool PlayerMouseButton(State& state, bool leftButton, bool down, int x, int y);
}
