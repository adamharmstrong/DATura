#include "input_controller.h"

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
    state.altHeld = false;
    state.slow = false;
    state.cameraDebugToggle = false;
    state.jumpHeld = false;
    state.jumpRequested = false;
}

void SetKeyState(InputController::State& state, const unsigned int keyCode, const bool down)
{
    switch (keyCode)
    {
    case 'W': state.forward = down; break;
    case 'S': state.backward = down; break;
    case 'A': state.left = down; break;
    case 'D': state.right = down; break;
    case 'Q': state.up = down; break;
    case 'E': state.down = down; break;
    case 0x20:
        if (down && !state.jumpHeld) state.jumpRequested = true;
        state.jumpHeld = down;
        break;
    case kKeyShift:
        if (down && !state.boost) state.running = !state.running;
        state.boost = down;
        break;
    case VK_MENU:
        if (down && !state.altHeld) state.fastRunning = !state.fastRunning;
        state.altHeld = down;
        break;
    case kKeyControl: state.slow = down; break;
    }
}
}

namespace InputController
{
void Initialize(State& state, HWND window, const bool hardwareCursorEnabled)
{
    state = {};
    state.window = window;
    state.clientMouse = { -1, -1 };
    state.hardwareCursorEnabled = hardwareCursorEnabled;
}

void Shutdown(State& state)
{
    FocusLost(state);
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
    if (keyCode == 'T')
    {
        if (state.cameraDebugToggle)
            return Action::None;
        state.cameraDebugToggle = true;
        return Action::ToggleCameraDebugOverlay;
    }
    switch (keyCode)
    {
    case kKeyBack: return Action::Back;
    case kKeyEnter: return Action::Confirm;
    case 'O': return state.slow ? Action::OpenDat : Action::None;
    case 'F': return Action::ToggleGameMode;
    case 'G': return Action::UnstickPlayer;
    case 'V': return Action::CycleWeather;
    default: return Action::None;
    }
}

void KeyUp(State& state, const unsigned int keyCode)
{
    SetKeyState(state, keyCode, false);
    if (keyCode == 'T')
        state.cameraDebugToggle = false;
}

MovementSnapshot Movement(const State& state)
{
    MovementSnapshot movement;
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
