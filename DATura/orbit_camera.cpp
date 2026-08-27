#include "stdafx.h"
#include "orbit_camera.h"

#include <cmath>
#include <algorithm>

namespace OrbitCamera
{
void MoveFly(State& state, const FlyInput& input, const float dt)
{
    float moveRight = input.right;
    float moveVertical = input.vertical;
    float moveForward = input.forward;
    if (moveRight == 0.0f && moveVertical == 0.0f && moveForward == 0.0f)
        return;

    const float length = std::sqrt(moveRight * moveRight + moveVertical * moveVertical + moveForward * moveForward);
    moveRight /= length;
    moveVertical /= length;
    moveForward /= length;

    const float forwardX = -std::sin(state.yaw);
    const float forwardZ = -std::cos(state.yaw);
    const float rightX = std::cos(state.yaw);
    const float rightZ = -std::sin(state.yaw);

    float speed = state.distance * 1.5f;
    if (speed < 2.0f)
        speed = 2.0f;
    if (input.boost)
        speed *= 4.0f;
    if (input.slow)
        speed *= 0.25f;

    const float step = speed * dt;
    state.target[0] += (rightX * moveRight + forwardX * moveForward) * step;
    state.target[1] += moveVertical * step;
    state.target[2] += (rightZ * moveRight + forwardZ * moveForward) * step;
}

void Pan(State& state, const int deltaX, const int deltaY)
{
    if (deltaX == 0 && deltaY == 0)
        return;

    const float forwardX = -std::sin(state.yaw) * std::cos(state.pitch);
    const float forwardY = -std::sin(state.pitch);
    const float forwardZ = -std::cos(state.yaw) * std::cos(state.pitch);
    const float rightX = std::cos(state.yaw);
    const float rightY = 0.0f;
    const float rightZ = -std::sin(state.yaw);

    float upX = rightY * forwardZ - rightZ * forwardY;
    float upY = rightZ * forwardX - rightX * forwardZ;
    float upZ = rightX * forwardY - rightY * forwardX;
    const float upLength = std::sqrt(upX * upX + upY * upY + upZ * upZ);
    if (upLength > 0.0f)
    {
        upX /= upLength;
        upY /= upLength;
        upZ /= upLength;
    }

    const float scale = state.distance * 0.0016f;
    state.target[0] += (-rightX * static_cast<float>(deltaX) - upX * static_cast<float>(deltaY)) * scale;
    state.target[1] += (-rightY * static_cast<float>(deltaX) - upY * static_cast<float>(deltaY)) * scale;
    state.target[2] += (-rightZ * static_cast<float>(deltaX) - upZ * static_cast<float>(deltaY)) * scale;
}

void Rotate(State& state, const int deltaX, const int deltaY)
{
    constexpr float sensitivity = 0.005f;
    state.yaw -= static_cast<float>(deltaX) * sensitivity;
    state.pitch -= static_cast<float>(deltaY) * sensitivity;
    state.pitch = std::clamp(state.pitch, -1.55f, 1.55f);
}

void Zoom(State& state, const float wheelSteps)
{
    state.distance -= wheelSteps * state.distance * 0.12f;
    state.distance = std::clamp(state.distance, 0.05f, 2000.0f);
}

void GetPosition(const State& state, float& outX, float& outY, float& outZ)
{
    outX = state.target[0] + state.distance * std::sin(state.yaw) * std::cos(state.pitch);
    outY = state.target[1] + state.distance * std::sin(state.pitch);
    outZ = state.target[2] + state.distance * std::cos(state.yaw) * std::cos(state.pitch);
}
}
