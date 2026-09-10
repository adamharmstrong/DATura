#include "player_controller.h"
#include "zone_collision_geometry.h"

#include <cmath>

namespace
{
bool UsesRunAnimation(const PlayerController::InputSnapshot& input)
{
    return (input.boost || input.fastRunning) && input.forward > 0.0f && input.strafe == 0.0f;
}

void CopyPosition(float destination[3], const float source[3])
{
    destination[0] = source[0];
    destination[1] = source[1];
    destination[2] = source[2];
}

void CaptureCheckpoint(PlayerController::Checkpoint& checkpoint,
                       const PlayerController::State& state)
{
    CopyPosition(checkpoint.position, state.position);
    checkpoint.yaw = state.yaw;
    checkpoint.valid = true;
}

void RestoreCheckpoint(PlayerController::State& state,
                       const PlayerController::Checkpoint& checkpoint)
{
    CopyPosition(state.position, checkpoint.position);
    state.yaw = checkpoint.yaw;
    state.verticalVelocity = 0.0f;
    state.onGround = true;
    state.jumping = false;
}
}

namespace PlayerController
{
void SetPose(State& state, const float x, const float y, const float z,
             const float yaw, const bool onGround)
{
    state.position[0] = x;
    state.position[1] = y;
    state.position[2] = z;
    state.yaw = yaw;
    state.verticalVelocity = 0.0f;
    state.onGround = onGround;
    state.jumping = false;
}

void SetRespawnPoint(State& state)
{
    CaptureCheckpoint(state.respawn, state);
    CaptureCheckpoint(state.lastSafe, state);
}

void SetLastSafePoint(State& state)
{
    CaptureCheckpoint(state.lastSafe, state);
}

bool Respawn(State& state)
{
    if (!state.respawn.valid)
        return false;

    RestoreCheckpoint(state, state.respawn);
    SetLastSafePoint(state);
    return true;
}

bool RestoreLastSafePoint(State& state)
{
    if (!state.lastSafe.valid)
        return false;

    RestoreCheckpoint(state, state.lastSafe);
    return true;
}

void ClearCollisionState(State& state)
{
    state.verticalVelocity = 0.0f;
    state.onGround = false;
    state.jumping = false;
    state.lastSafe.valid = false;
}

void ResetModelPlacement(State& state)
{
    state.groundOffset = 0.0f;
    state.cameraTargetLocalY = kDefaultCameraTargetLocalY;
}

bool IsOutOfBounds(const State& state, const float minBounds[3],
                   const float maxBounds[3], const bool haveBounds)
{
    if (!haveBounds || !state.respawn.valid || !minBounds || !maxBounds)
        return false;

    constexpr float horizontalMargin = 80.0f;
    constexpr float downwardMargin = 80.0f;
    constexpr float upwardMargin = 120.0f;
    return state.position[0] < minBounds[0] - horizontalMargin ||
           state.position[0] > maxBounds[0] + horizontalMargin ||
           state.position[2] < minBounds[2] - horizontalMargin ||
           state.position[2] > maxBounds[2] + horizontalMargin ||
           state.position[1] > maxBounds[1] + downwardMargin ||
           state.position[1] < minBounds[1] - upwardMargin;
}

void WriteCameraTarget(const State& state, float target[3])
{
    if (!target)
        return;

    target[0] = state.position[0];
    target[1] = state.position[1] + state.cameraTargetLocalY;
    target[2] = state.position[2];
}

UpdateResult UpdateMovement(State& state, const InputSnapshot& input, float dt,
                            const SimulationContext& context)
{
    UpdateResult result;
    if (dt <= 0.0f)
        return result;
    if (dt > 0.1f)
        dt = 0.1f;

    constexpr float turnSpeed = 2.5f;
    // Backpedal and strafe clips have a walking stride, even with run toggled.
    float moveSpeed = UsesRunAnimation(input) ?
        (input.fastRunning ? kFastRunSpeed : kRunSpeed) : kWalkSpeed;
    if (input.slow)
        moveSpeed *= kSlowMultiplier;

    state.yaw += input.turn * turnSpeed * dt;
    const float magnitude = std::sqrt(input.forward * input.forward + input.strafe * input.strafe);
    const float step = moveSpeed * dt / (magnitude > 1.0f ? magnitude : 1.0f);
    const float deltaX = (-std::sin(state.yaw) * input.forward + std::cos(state.yaw) * input.strafe) * step;
    const float deltaZ = (-std::cos(state.yaw) * input.forward - std::sin(state.yaw) * input.strafe) * step;

    if (context.beforeHorizontalMove)
        context.beforeHorizontalMove(state, deltaX, deltaZ, dt);

    const ZoneCollision::Mesh* collisionMesh = context.collisionMesh;
    const bool haveCollision = collisionMesh && !collisionMesh->Empty();
    if (input.jump && !state.jumping && (state.onGround || !haveCollision))
    {
        state.jumpOriginY = state.position[1];
        state.verticalVelocity = -kJumpSpeed; // FFXI's world Y increases downward.
        state.onGround = false;
        state.jumping = true;
    }
    if (haveCollision)
    {
        ZoneCollision::MoveHorizontal(
            *collisionMesh, kCollisionRadius, kCollisionHeight, kStepHeight,
            1.25f, state.position, state.verticalVelocity, state.onGround,
            deltaX, deltaZ);

        const bool reachedFloor = ZoneCollision::UpdateVerticalMotion(
            *collisionMesh, kCollisionRadius, kStepHeight,
            0.35f, 80.0f, kGravity, 80.0f, dt,
            state.position, state.verticalVelocity, state.onGround, kCollisionHeight);
        if (state.onGround) state.jumping = false;
        if (reachedFloor &&
            !ZoneCollision::OverlapsWallAt(
                collisionMesh->Triangles(), collisionMesh->Index(),
                state.position[0], state.position[1], state.position[2],
                kCollisionRadius, kCollisionHeight, kStepHeight))
        {
            SetLastSafePoint(state);
            result.reachedStableFloor = true;
        }

        if (IsOutOfBounds(state, collisionMesh->MinBounds(),
                          collisionMesh->MaxBounds(), collisionMesh->HasBounds()))
        {
            result.respawned = Respawn(state);
        }
        return result;
    }

    state.position[0] += deltaX;
    state.position[2] += deltaZ;
    if (state.jumping)
    {
        state.verticalVelocity += kGravity * dt;
        state.position[1] += state.verticalVelocity * dt;
        if (state.verticalVelocity >= 0.0f && state.position[1] >= state.jumpOriginY)
        {
            state.position[1] = state.jumpOriginY;
            state.verticalVelocity = 0.0f;
            state.onGround = true;
            state.jumping = false;
        }
    }
    else
        state.position[1] += input.vertical * moveSpeed * dt;
    return result;
}

const char* MovementAnimation(const State& state, const InputSnapshot& input)
{
    if (state.jumping) return "jmp";
    // DAT motion names use the opposite left/right convention to player input.
    if (input.strafe < 0.0f) return "mvr";
    if (input.strafe > 0.0f) return "mvl";
    if (input.forward < 0.0f) return "mvb_relaxed";
    if (input.forward > 0.0f) return UsesRunAnimation(input) ? "run_relaxed" : "wlk_relaxed";
    return "idl_relaxed";
}

float MovementAnimationRate(const State& state, const InputSnapshot& input)
{
    if (state.jumping || (input.forward == 0.0f && input.strafe == 0.0f))
        return 1.0f;
    const float rate = UsesRunAnimation(input) && input.fastRunning ? kFastRunSpeed / kRunSpeed : 1.0f;
    return rate * (input.slow ? kSlowMultiplier : 1.0f);
}

bool Unstick(State& state, const ZoneCollision::Mesh& collisionMesh)
{
    if (collisionMesh.Empty())
        return false;

    float safeX = 0.0f;
    float safeY = 0.0f;
    float safeZ = 0.0f;
    if (ZoneCollision::FindNearestSafeFloor(
            collisionMesh.Triangles(), collisionMesh.Index(),
            kCollisionRadius, kCollisionHeight, kStepHeight,
            state.position[0], state.position[1], state.position[2],
            &safeX, &safeY, &safeZ))
    {
        SetPose(state, safeX, safeY, safeZ, state.yaw, true);
        SetLastSafePoint(state);
        return true;
    }

    if (RestoreLastSafePoint(state))
        return true;
    return Respawn(state);
}
}
