#pragma once

namespace OrbitCamera
{
    struct State
    {
        float yaw = 0.0f;
        float pitch = -0.25f;
        float distance = 5.0f;
        float target[3] = { 0.0f, 0.0f, 0.0f };
    };

    struct FlyInput
    {
        float right = 0.0f;
        float vertical = 0.0f;
        float forward = 0.0f;
        bool boost = false;
        bool slow = false;
    };

    void MoveFly(State& state, const FlyInput& input, float dt);
    void FollowTarget(State& state, const float target[3], float dt);
    void FollowYaw(State& state, float yaw, float dt);
    void Pan(State& state, int deltaX, int deltaY);
    void Rotate(State& state, int deltaX, int deltaY);
    void RotateByTrackpadScroll(State& state, float horizontalSteps, float verticalSteps);
    void Zoom(State& state, float wheelSteps);
    void GetPosition(const State& state, float& outX, float& outY, float& outZ);
}
