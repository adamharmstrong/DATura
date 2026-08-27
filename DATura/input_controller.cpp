#include "input_controller.h"

namespace
{
constexpr unsigned int kKeyBack = 0x08;
constexpr unsigned int kKeyEnter = 0x0D;
constexpr unsigned int kKeyShift = 0x10;
constexpr unsigned int kKeyControl = 0x11;
constexpr unsigned int kKeyEscape = 0x1B;

void ApplyCursorVisibility(InputController::State& state)
{
    const bool shouldHide = state.wantsCursorHidden && !state.hardwareCursorEnabled;
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
    state.slow = false;
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
    case kKeyShift: state.boost = down; break;
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
    switch (keyCode)
    {
    case kKeyEscape: return Action::Exit;
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
}

MovementSnapshot Movement(const State& state)
{
    MovementSnapshot movement;
    movement.right = (state.right ? 1.0f : 0.0f) - (state.left ? 1.0f : 0.0f);
    movement.forward = (state.forward ? 1.0f : 0.0f) - (state.backward ? 1.0f : 0.0f);
    movement.vertical = (state.up ? 1.0f : 0.0f) - (state.down ? 1.0f : 0.0f);
    movement.boost = state.boost;
    movement.slow = state.slow;
    return movement;
}

bool IsMovementActive(const State& state)
{
    return state.forward || state.backward || state.left || state.right;
}
}
