#pragma once

namespace ZoneCollision
{
class Mesh;
}

namespace PlayerController
{
inline constexpr float kCollisionRadius = 0.72f;
// Keep the movement body below retail doorway lintels (some openings are
// only 2.66 units tall). Model and camera height do not define clearance.
inline constexpr float kCollisionHeight = 2.0f;
inline constexpr float kStepHeight = 0.85f;
inline constexpr float kDefaultCameraTargetLocalY = -2.0f;
inline constexpr float kJumpSpeed = 12.0f;
inline constexpr float kGravity = 28.0f;
inline constexpr float kWalkSpeed = 4.0f;
inline constexpr float kRunSpeed = 6.0f;
inline constexpr float kFastRunSpeed = 16.0f;
inline constexpr float kSlowMultiplier = 0.35f;

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
    bool jumping = false;
    float jumpOriginY = 0.0f;
    Checkpoint respawn;
    Checkpoint lastSafe;
};

struct InputSnapshot
{
    bool fastRunning = false;
    float turn = 0.0f;
    float forward = 0.0f;
    float vertical = 0.0f;
    float strafe = 0.0f;
    bool jump = false;
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
    // Dynamic obstacles see intended movement before static collision stops it.
    void (*beforeHorizontalMove)(const State&, float deltaX, float deltaZ, float dt) = nullptr;
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
const char* MovementAnimation(const State& state, const InputSnapshot& input);
float MovementAnimationRate(const State& state, const InputSnapshot& input);
}
