#include "input_controller.h"
#include "application_settings.h"
#include <cstdio>

namespace
{
constexpr unsigned int kKeyBack = 0x08;
constexpr unsigned int kKeyEnter = 0x0D;
constexpr unsigned int kKeyShift = 0x10;
constexpr unsigned int kKeyControl = 0x11;

void ApplyCursorVisibility(InputController::State& state)
{
    const bool shouldHide = state.wantsCursorHidden;
    if (state.cursorHidden == shouldHide)
        return;

    state.cursorHidden = shouldHide;
    if (!state.window)
        return;

    if (shouldHide)
    {
        while (ShowCursor(FALSE) >= 0) {}
    }
    else
    {
        while (ShowCursor(TRUE) < 0) {}
    }
}

void ClearMovementKeys(InputController::State& state)
{
    state.forward = false;
    state.backward = false;
    state.left = false;
    state.right = false;
    state.up = false;
    state.down = false;
    state.boost = false;
    state.autoRunToggleHeld = false;
    state.altHeld = false;
    state.slow = false;
    state.cameraDebugToggle = false;
    state.zoneMapToggleHeld = false;
    state.bumpMappingToggleHeld = false;
    state.bumpMappingInversionToggleHeld = false;
    state.jumpHeld = false;
    state.jumpRequested = false;
}

void SetKeyState(InputController::State& state, const unsigned int keyCode, const bool down)
{
    const ApplicationSettings::State defaults;
    const ApplicationSettings::State& keys = state.settings ? *state.settings : defaults;
    if (keyCode == static_cast<unsigned int>(keys.keyForward)) state.forward = down;
    else if (keyCode == static_cast<unsigned int>(keys.keyBackward)) state.backward = down;
    else if (keyCode == static_cast<unsigned int>(keys.keyLeft)) state.left = down;
    else if (keyCode == static_cast<unsigned int>(keys.keyRight)) state.right = down;
    else if (keyCode == static_cast<unsigned int>(keys.keyAutoRun))
    {
        if (down && !state.autoRunToggleHeld)
            state.autoRun = !state.autoRun;
        state.autoRunToggleHeld = down;
    }
    else if (keyCode == static_cast<unsigned int>(keys.keyJump))
    {
        if (down && !state.jumpHeld) state.jumpRequested = true;
        state.jumpHeld = down;
    }
    else if (keyCode == static_cast<unsigned int>(keys.keyRunToggle))
    {
        if (down && !state.boost) state.running = !state.running;
        state.boost = down;
    }
    else if (keyCode == VK_MENU)
    {
        if (down && !state.altHeld) state.fastRunning = !state.fastRunning;
        state.altHeld = down;
    }
    else if (keyCode == 'Q') state.up = down;
    else if (keyCode == 'E') state.down = down;
    else if (keyCode == kKeyControl) state.slow = down;
}
}

namespace InputController
{
void SetCursorStyle(State& state, const int style, const char* ffxiRoot)
{
    if (state.customCursor)
    {
        DestroyCursor(state.customCursor);
        state.customCursor = NULL;
    }
    const char* file = style == ApplicationSettings::MouseCursorFfxiAnimated ? "mousenor.ani" :
        style == ApplicationSettings::MouseCursorFfxiStatic ? "mousenor.cur" :
        style == ApplicationSettings::MouseCursorFfxiInteraction ? "mousehit.ani" : nullptr;
    if (file && ffxiRoot)
    {
        char path[MAX_PATH] = {};
        sprintf_s(path, "%s%s", ffxiRoot, file);
        state.customCursor = LoadCursorFromFileA(path);
    }
    const HCURSOR cursor = state.customCursor ? state.customCursor : LoadCursor(NULL, IDC_ARROW);
    if (state.window)
    {
        SetClassLongPtrA(state.window, GCLP_HCURSOR, reinterpret_cast<LONG_PTR>(cursor));
        SetCursor(cursor);
    }
}

void Initialize(State& state, HWND window, const bool hardwareCursorEnabled)
{
    state = {};
    state.window = window;
    state.clientMouse = { -1, -1 };
    state.hardwareCursorEnabled = hardwareCursorEnabled;
}

void SetKeyBindings(State& state, const ApplicationSettings::State& settings)
{
    state.settings = &settings;
    ClearMovementKeys(state);
}

void Shutdown(State& state)
{
    FocusLost(state);
    if (state.customCursor)
        DestroyCursor(state.customCursor);
    state.window = nullptr;
}

void SetHardwareCursorEnabled(State& state, const bool enabled)
{
    state.hardwareCursorEnabled = enabled;
    ApplyCursorVisibility(state);
}

void BeginOrbit(State& state, const int x, const int y)
{
    state.dragMode = DragMode::Orbit;
    state.lastMouse = { x, y };
    state.wantsCursorHidden = true;
    if (state.window)
        SetCapture(state.window);
    ApplyCursorVisibility(state);
}

void BeginPan(State& state, const int x, const int y)
{
    state.dragMode = DragMode::Pan;
    state.lastMouse = { x, y };
    state.wantsCursorHidden = false;
    if (state.window)
        SetCapture(state.window);
    ApplyCursorVisibility(state);
}

void BeginLightAzimuth(State& state, const int x, const int y)
{
    state.dragMode = DragMode::LightAzimuth;
    state.lastMouse = { x, y };
    state.wantsCursorHidden = false;
    if (state.window)
        SetCapture(state.window);
    ApplyCursorVisibility(state);
}

void EndDrag(State& state)
{
    state.leftMouseHeld = false;
    state.rightMouseHeld = false;
    state.pendingWorldClick = false;
    state.dragMode = DragMode::None;
    state.wantsCursorHidden = false;
    if (state.window && GetCapture() == state.window)
        ReleaseCapture();
    ApplyCursorVisibility(state);
}

void CaptureChanged(State& state, const HWND newCapture)
{
    if (newCapture == state.window)
        return;

    state.leftMouseHeld = false;
    state.rightMouseHeld = false;
    state.pendingWorldClick = false;
    state.dragMode = DragMode::None;
    state.wantsCursorHidden = false;
    ApplyCursorVisibility(state);
}

void FocusLost(State& state)
{
    ClearMovementKeys(state);
    EndDrag(state);
}

DragDelta MouseMoved(State& state, const int x, const int y)
{
    state.clientMouse = { x, y };
    DragDelta delta;
    delta.mode = state.dragMode;
    if (state.dragMode == DragMode::None)
        return delta;

    delta.x = x - state.lastMouse.x;
    delta.y = y - state.lastMouse.y;
    state.lastMouse = { x, y };
    return delta;
}

void MouseLeft(State& state)
{
    state.clientMouse = { -1, -1 };
}

Action KeyDown(State& state, const unsigned int keyCode)
{
    SetKeyState(state, keyCode, true);
    const ApplicationSettings::State defaults;
    const ApplicationSettings::State& keys = state.settings ? *state.settings : defaults;
    if (keyCode == static_cast<unsigned int>(keys.keyBumpMapping))
    {
        if (state.bumpMappingToggleHeld)
            return Action::None;
        state.bumpMappingToggleHeld = true;
        return Action::ToggleBumpMapping;
    }
    if (keyCode == static_cast<unsigned int>(keys.keyCameraDebug))
    {
        if (state.cameraDebugToggle)
            return Action::None;
        state.cameraDebugToggle = true;
        return Action::ToggleCameraDebugOverlay;
    }
    if (keyCode == static_cast<unsigned int>(keys.keyZoneMap))
    {
        if (state.zoneMapToggleHeld)
            return Action::None;
        state.zoneMapToggleHeld = true;
        return Action::ToggleZoneMap;
    }
    if (keyCode == static_cast<unsigned int>(keys.keyGameMode)) return Action::ToggleGameMode;
    if (keyCode == static_cast<unsigned int>(keys.keyUnstick)) return Action::UnstickPlayer;
    if (keyCode == static_cast<unsigned int>(keys.keyCycleWeather)) return Action::CycleWeather;
    switch (keyCode)
    {
    case kKeyBack: return Action::Back;
    case kKeyEnter: return Action::Confirm;
    case 'O':
        if (state.slow)
            return Action::OpenDat;
        if (state.bumpMappingInversionToggleHeld)
            return Action::None;
        state.bumpMappingInversionToggleHeld = true;
        return Action::ToggleBumpMappingInversion;
    case VK_OEM_4: return Action::DecreaseBumpMappingIntensity;
    case VK_OEM_6: return Action::IncreaseBumpMappingIntensity;
    default: break;
    }
    return Action::None;
}

void KeyUp(State& state, const unsigned int keyCode)
{
    SetKeyState(state, keyCode, false);
    const ApplicationSettings::State defaults;
    const ApplicationSettings::State& keys = state.settings ? *state.settings : defaults;
    if (keyCode == static_cast<unsigned int>(keys.keyCameraDebug))
        state.cameraDebugToggle = false;
    if (keyCode == static_cast<unsigned int>(keys.keyZoneMap))
        state.zoneMapToggleHeld = false;
    if (keyCode == static_cast<unsigned int>(keys.keyBumpMapping))
        state.bumpMappingToggleHeld = false;
    if (keyCode == 'O')
        state.bumpMappingInversionToggleHeld = false;
}

MovementSnapshot Movement(const State& state)
{
    MovementSnapshot movement;
    movement.autoRun = state.autoRun;
    movement.right = (state.right ? 1.0f : 0.0f) - (state.left ? 1.0f : 0.0f);
    movement.forward = (state.forward ? 1.0f : 0.0f) - (state.backward ? 1.0f : 0.0f);
    movement.vertical = (state.up ? 1.0f : 0.0f) - (state.down ? 1.0f : 0.0f);
    movement.strafe = (state.down ? 1.0f : 0.0f) - (state.up ? 1.0f : 0.0f);
    movement.boost = state.boost;
    movement.running = state.running;
    movement.fastRunning = state.fastRunning;
    movement.slow = state.slow;
    return movement;
}

bool IsMovementActive(const State& state)
{
    return state.forward || state.backward || state.left || state.right || state.up || state.down;
}

bool MouseForwardActive(const State& state)
{
    return state.leftMouseHeld && state.rightMouseHeld;
}

bool PlayerMouseButton(State& state, bool leftButton, bool down, int x, int y)
{
    const bool click = leftButton && !down && state.leftMouseHeld && state.pendingWorldClick;
    if (leftButton)
    {
        state.leftMouseHeld = down;
        state.pendingWorldClick = down;
    }
    else
        state.rightMouseHeld = down;
    if (MouseForwardActive(state)) state.pendingWorldClick = false;
    state.lastMouse = { x, y };
    if (state.rightMouseHeld)
        BeginOrbit(state, x, y);
    else if (state.leftMouseHeld)
    {
        state.dragMode = DragMode::None;
        state.wantsCursorHidden = MouseForwardActive(state);
        if (state.window) SetCapture(state.window);
        ApplyCursorVisibility(state);
    }
    else
        EndDrag(state);
    return click;
}

bool ConsumeJump(State& state)
{
    const bool requested = state.jumpRequested;
    state.jumpRequested = false;
    return requested;
}
}
