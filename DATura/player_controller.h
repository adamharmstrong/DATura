#pragma once

namespace ZoneCollision
{
class Mesh;
}

namespace PlayerController
{
inline constexpr float kCollisionRadius = 0.72f;
inline constexpr float kCollisionHeight = 3.2f;
inline constexpr float kStepHeight = 0.85f;
inline constexpr float kDefaultCameraTargetLocalY = -2.0f;

struct Checkpoint
{
    float position[3] = { 0.0f, 0.0f, 0.0f };
    float yaw = 0.0f;
    bool valid = false;
};

struct State
{
    bool cameraActive = false;
    float position[3] = { 0.0f, 0.0f, 0.0f };
    float yaw = 0.0f;
    float groundOffset = 0.0f;
    float cameraTargetLocalY = kDefaultCameraTargetLocalY;
    float verticalVelocity = 0.0f;
    bool onGround = false;
    Checkpoint respawn;
    Checkpoint lastSafe;
};

struct InputSnapshot
{
    float turn = 0.0f;
    float forward = 0.0f;
    float vertical = 0.0f;
    bool boost = false;
    bool slow = false;
};

struct UpdateResult
{
    bool reachedStableFloor = false;
    bool respawned = false;
};

struct SimulationContext
{
    const ZoneCollision::Mesh* collisionMesh = nullptr;
};

void SetPose(State& state, float x, float y, float z, float yaw, bool onGround);
void SetRespawnPoint(State& state);
void SetLastSafePoint(State& state);
bool Respawn(State& state);
bool RestoreLastSafePoint(State& state);
void ClearCollisionState(State& state);
void ResetModelPlacement(State& state);
bool IsOutOfBounds(const State& state, const float minBounds[3],
                   const float maxBounds[3], bool haveBounds);
void WriteCameraTarget(const State& state, float target[3]);
UpdateResult UpdateMovement(State& state, const InputSnapshot& input, float dt,
                            const SimulationContext& context);
bool Unstick(State& state, const ZoneCollision::Mesh& collisionMesh);
}
