#include "../DATura/application_settings.h"
#include "../DATura/creation_model_loader.h"
#include "../DATura/ffxi_dat_set_builder.h"
#include "../DATura/ffxi_model_lifetime.h"
#include "../DATura/high_poly_creation_panel.h"
#include "../DATura/input_controller.h"
#include "../DATura/interaction_controller.h"
#include "../DATura/low_poly_character_panel.h"
#include "../DATura/orbit_camera.h"
#include "../DATura/player_controller.h"
#include "../DATura/player_model_loader.h"
#include "../DATura/scene_load_context.h"
#include "../DATura/scene_model_loader.h"
#include "../DATura/scene_request_resolver.h"
#include "../DATura/title_scene_assets.h"
#include "../DATura/zone_collision_geometry.h"
#include "../DATura/zone_object_panel.h"

#include "../DATura/npc_interaction.h"
#include <cmath>
#include <iostream>
#include <string_view>
#include <type_traits>

namespace
{
int gChecks = 0;
int gFailures = 0;

void Check(const bool condition, const std::string_view description)
{
    ++gChecks;
    if (condition)
        return;

    ++gFailures;
    std::cerr << "FAIL: " << description << '\n';
}

bool NearlyEqual(const float left, const float right, const float epsilon = 0.0001f)
{
    return std::fabs(left - right) <= epsilon;
}

void TestDefaultState()
{
    const ApplicationSettings::State settings;

    Check(settings.doorInteractionMode == ApplicationSettings::DoorClassic, "doors default to Classic interaction");
    Check(settings.windowMode == ApplicationSettings::Windowed, "default window mode is windowed");
    Check(settings.resolutionIndex == 0, "default resolution index is zero");
    Check(settings.environmentalAnimationMode == ApplicationSettings::EnvironmentalAnimationSmooth,
        "default environmental animation mode is smooth");
    Check(settings.enableSounds, "sounds are enabled by default");
    Check(settings.playSoundsInBackground, "background sounds are enabled by default");
    Check(settings.maxSimultaneousSounds == -1, "simultaneous sounds are unlimited by default");
    Check(settings.enableHardwareMouseCursor, "hardware cursor is enabled by default");
    Check(settings.enableMipMapping, "mip mapping is enabled by default");
    Check(!settings.enableBumpMapping, "bump mapping is disabled by default");
    Check(settings.lightingQuality == ApplicationSettings::LightingDynamicShadows,
        "dynamic shadows are the default lighting quality");
    Check(settings.drawDistanceIndex == 4, "default draw-distance index selects 8000");
    Check(settings.enableTextureCompression, "texture compression is enabled by default");
    Check(!settings.mirrorWorldZones, "world-zone mirroring is disabled by default");
    Check(!settings.showCollisionGeometry, "collision geometry is hidden by default");
}

void TestResolutionOptions()
{
    Check(ApplicationSettings::ResolutionOptionCount() == 6, "six resolution options are available");
    Check(ApplicationSettings::ClampResolutionIndex(-1) == 0, "negative resolution index clamps to zero");
    Check(ApplicationSettings::ClampResolutionIndex(6) == 0, "large resolution index clamps to zero");
    Check(ApplicationSettings::ClampResolutionIndex(4) == 4, "valid resolution index is retained");

    const auto& first = ApplicationSettings::ResolutionOptionAt(0);
    Check(first.width == 1280 && first.height == 720, "first resolution is 1280 x 720");
    Check(std::string_view(first.label) == "1280 x 720", "first resolution label is stable");

    const auto& last = ApplicationSettings::ResolutionOptionAt(5);
    Check(last.width == 2560 && last.height == 1440, "last resolution is 2560 x 1440");
    Check(std::string_view(last.label) == "2560 x 1440", "last resolution label is stable");

    const auto& clamped = ApplicationSettings::ResolutionOptionAt(99);
    Check(clamped.width == first.width && clamped.height == first.height,
        "out-of-range resolution lookup uses the first option");
}

void TestWindowModes()
{
    Check(ApplicationSettings::ClampWindowMode(-1) == ApplicationSettings::Windowed,
        "negative window mode clamps to windowed");
    Check(ApplicationSettings::ClampWindowMode(3) == ApplicationSettings::Windowed,
        "large window mode clamps to windowed");
    Check(ApplicationSettings::ClampWindowMode(ApplicationSettings::Windowed) == ApplicationSettings::Windowed,
        "windowed mode is retained");
    Check(ApplicationSettings::ClampWindowMode(ApplicationSettings::Borderless) == ApplicationSettings::Borderless,
        "borderless mode is retained");
    Check(ApplicationSettings::ClampWindowMode(ApplicationSettings::Fullscreen) == ApplicationSettings::Fullscreen,
        "fullscreen mode is retained");
}

void TestSoundOptions()
{
    constexpr int expectedValues[] = { 8, 16, 20, 32, 64, 128, -1 };
    constexpr std::string_view expectedLabels[] = { "8", "16", "20", "32", "64", "128", "Unlimited" };

    Check(ApplicationSettings::MaxSoundOptionCount() == 7, "seven sound-limit options are available");
    for (int index = 0; index < ApplicationSettings::MaxSoundOptionCount(); ++index)
    {
        Check(ApplicationSettings::MaxSoundOptionValue(index) == expectedValues[index],
            "sound-limit value matches its option index");
        Check(std::string_view(ApplicationSettings::MaxSoundOptionLabel(index)) == expectedLabels[index],
            "sound-limit label matches its option index");
        Check(ApplicationSettings::MaxSoundOptionIndex(expectedValues[index]) == index,
            "sound-limit value maps back to its option index");
    }

    Check(ApplicationSettings::MaxSoundOptionValue(-1) == -1,
        "negative sound-limit index selects unlimited");
    Check(std::string_view(ApplicationSettings::MaxSoundOptionLabel(99)) == "Unlimited",
        "large sound-limit index selects the unlimited label");
    Check(ApplicationSettings::MaxSoundOptionIndex(12345) == 6,
        "unknown sound limit maps to unlimited");
}

void TestDrawDistanceOptions()
{
    constexpr float expectedValues[] = { 500.0f, 1000.0f, 2000.0f, 4000.0f, 8000.0f, 0.0f };
    constexpr std::string_view expectedLabels[] =
    {
        "500", "1000", "2000", "4000", "8000", "Unlimited"
    };

    Check(ApplicationSettings::DrawDistanceOptionCount() == 6, "six draw-distance options are available");
    for (int index = 0; index < ApplicationSettings::DrawDistanceOptionCount(); ++index)
    {
        Check(ApplicationSettings::DrawDistanceOptionValue(index) == expectedValues[index],
            "draw-distance value matches its option index");
        Check(std::string_view(ApplicationSettings::DrawDistanceOptionLabel(index)) == expectedLabels[index],
            "draw-distance label matches its option index");
        Check(ApplicationSettings::ClampDrawDistanceIndex(index) == index,
            "valid draw-distance index is retained");
    }

    Check(ApplicationSettings::ClampDrawDistanceIndex(-1) == 4,
        "negative draw-distance index clamps to the default");
    Check(ApplicationSettings::ClampDrawDistanceIndex(6) == 4,
        "large draw-distance index clamps to the default");
    Check(ApplicationSettings::DrawDistanceOptionValue(99) == 8000.0f,
        "out-of-range draw-distance lookup uses 8000");
    Check(!ApplicationSettings::DrawDistanceOptionIsUnlimited(4),
        "8000 draw distance is finite");
    Check(ApplicationSettings::DrawDistanceOptionIsUnlimited(5),
        "unlimited draw distance uses the final option");
}

void TestQualityModes()
{
    using namespace ApplicationSettings;

    Check(ClampEnvironmentalAnimationMode(-1) == EnvironmentalAnimationOff,
        "environment animation clamps below its minimum");
    Check(ClampEnvironmentalAnimationMode(3) == EnvironmentalAnimationSmooth,
        "environment animation clamps above its maximum");
    Check(std::string_view(EnvironmentalAnimationModeName(EnvironmentalAnimationOff)) == "Off",
        "off environment animation has a stable name");
    Check(std::string_view(EnvironmentalAnimationModeName(EnvironmentalAnimationSimple)) == "Simple",
        "simple environment animation has a stable name");
    Check(std::string_view(EnvironmentalAnimationModeName(EnvironmentalAnimationSmooth)) == "Smooth",
        "smooth environment animation has a stable name");

    Check(ClampLightingQuality(-1) == LightingOff, "lighting quality clamps below its minimum");
    Check(ClampLightingQuality(3) == LightingDynamicShadows, "lighting quality clamps above its maximum");
    Check(std::string_view(LightingQualityName(LightingOff)) == "Off", "off lighting has a stable name");
    Check(std::string_view(LightingQualityName(LightingSimplified)) == "Simplified",
        "simplified lighting has a stable name");
    Check(std::string_view(LightingQualityName(LightingDynamicShadows)) == "Dynamic Shadows",
        "dynamic-shadow lighting has a stable name");
}

void TestPlayerControllerState()
{
    PlayerController::State player;
    Check(!player.cameraActive, "player camera is inactive by default");
    Check(player.cameraTargetLocalY == PlayerController::kDefaultCameraTargetLocalY,
        "player camera target uses the default local height");
    Check(!player.respawn.valid && !player.lastSafe.valid,
        "player checkpoints are invalid by default");

    player.verticalVelocity = 12.0f;
    PlayerController::SetPose(player, 10.0f, 20.0f, 30.0f, 1.5f, true);
    Check(player.position[0] == 10.0f && player.position[1] == 20.0f && player.position[2] == 30.0f,
        "setting a player pose updates its position");
    Check(player.yaw == 1.5f && player.onGround && player.verticalVelocity == 0.0f,
        "setting a player pose updates orientation and grounding");

    PlayerController::SetRespawnPoint(player);
    Check(player.respawn.valid && player.lastSafe.valid,
        "setting a respawn point initializes both checkpoints");

    PlayerController::SetPose(player, 40.0f, 50.0f, 60.0f, 2.5f, false);
    PlayerController::SetLastSafePoint(player);
    PlayerController::SetPose(player, 70.0f, 80.0f, 90.0f, 3.5f, false);
    Check(PlayerController::RestoreLastSafePoint(player),
        "a valid last-safe checkpoint can be restored");
    Check(player.position[0] == 40.0f && player.position[1] == 50.0f && player.position[2] == 60.0f,
        "last-safe restoration recovers the captured position");
    Check(player.yaw == 2.5f && player.onGround && player.verticalVelocity == 0.0f,
        "last-safe restoration resets stable movement state");

    Check(PlayerController::Respawn(player), "a valid respawn checkpoint can be restored");
    Check(player.position[0] == 10.0f && player.position[1] == 20.0f && player.position[2] == 30.0f,
        "respawning recovers the captured spawn position");
    Check(player.yaw == 1.5f && player.lastSafe.valid,
        "respawning restores yaw and refreshes the last-safe checkpoint");

    float cameraTarget[3] = {};
    player.cameraTargetLocalY = -3.0f;
    PlayerController::WriteCameraTarget(player, cameraTarget);
    Check(cameraTarget[0] == 10.0f && cameraTarget[1] == 17.0f && cameraTarget[2] == 30.0f,
        "player state writes the expected orbit-camera target");

    const float minBounds[3] = { -100.0f, -100.0f, -100.0f };
    const float maxBounds[3] = { 100.0f, 100.0f, 100.0f };
    Check(!PlayerController::IsOutOfBounds(player, minBounds, maxBounds, true),
        "a spawned player inside zone margins remains in bounds");
    player.position[0] = 181.0f;
    Check(PlayerController::IsOutOfBounds(player, minBounds, maxBounds, true),
        "a spawned player beyond the horizontal margin is out of bounds");

    PlayerController::ClearCollisionState(player);
    Check(!player.onGround && player.verticalVelocity == 0.0f && !player.lastSafe.valid,
        "clearing collision state resets transient grounding data");
    Check(player.respawn.valid, "clearing collision state preserves the respawn checkpoint");

    player.groundOffset = 4.0f;
    player.cameraTargetLocalY = 5.0f;
    PlayerController::ResetModelPlacement(player);
    Check(player.groundOffset == 0.0f &&
          player.cameraTargetLocalY == PlayerController::kDefaultCameraTargetLocalY,
        "resetting model placement restores its alignment defaults");
}

void TestPlayerControllerMovement()
{
    PlayerController::State player;
    PlayerController::InputSnapshot input;
    input.forward = 1.0f;

    const PlayerController::UpdateResult freeMove =
        PlayerController::UpdateMovement(player, input, 0.5f, {});
    Check(NearlyEqual(player.position[2], -0.4f),
        "free player movement clamps long frame times");
    Check(!freeMove.reachedStableFloor && !freeMove.respawned,
        "free movement reports no collision outcome");

    for (int direction = 0; direction < 4; ++direction)
    {
        PlayerController::State moving;
        PlayerController::InputSnapshot keys;
        keys.boost = true;
        keys.forward = direction == 1 ? -1.0f : 1.0f;
        keys.strafe = direction >= 2 ? 1.0f : 0.0f;
        keys.slow = direction == 3;
        for (int frame = 0; frame < 60; ++frame)
            PlayerController::UpdateMovement(moving, keys, 1.0f / 60.0f, {});
        const float distance = std::sqrt(moving.position[0] * moving.position[0] +
            moving.position[2] * moving.position[2]);
        Check(NearlyEqual(distance, direction == 0 ? 6.0f : direction == 3 ? 1.4f : 4.0f),
            "run, backpedal, diagonal and slow movement cover the intended distance");
    }

    input = {};
    input.turn = 1.0f;
    PlayerController::UpdateMovement(player, input, 0.1f, {});
    Check(NearlyEqual(player.yaw, 0.25f),
        "player movement applies the controller turn rate");

    input = {};
    input.vertical = 1.0f;
    input.boost = true;
    input.slow = true;
    const float previousY = player.position[1];
    PlayerController::UpdateMovement(player, input, 0.1f, {});
    Check(NearlyEqual(player.position[1] - previousY, 0.14f),
        "free vertical movement uses slowed walking speed");

    const float previousX = player.position[0];
    PlayerController::UpdateMovement(player, input, -1.0f, {});
    Check(NearlyEqual(player.position[0], previousX),
        "non-positive frame times do not update player movement");

    const float floorPoints[9] =
    {
        -10.0f, 0.0f, -10.0f,
          0.0f, 0.0f,  10.0f,
         10.0f, 0.0f, -10.0f,
    };
    ZoneCollision::Triangle floor;
    ZoneCollision::Mesh collision;
    Check(ZoneCollision::BuildTriangle(floorPoints, false, floor),
        "movement test floor produces valid collision geometry");
    collision.AddTriangle(floor, PlayerController::kCollisionRadius);

    PlayerController::SetPose(player, 0.0f, 0.0f, 0.0f, 0.0f, true);
    PlayerController::SetRespawnPoint(player);
    input = {};
    const PlayerController::SimulationContext collisionContext = { &collision };
    const PlayerController::UpdateResult grounded =
        PlayerController::UpdateMovement(player, input, 0.1f, collisionContext);
    Check(grounded.reachedStableFloor && player.onGround,
        "collision movement retains a stable floor");
    Check(player.lastSafe.valid && NearlyEqual(player.lastSafe.position[1], 0.0f),
        "stable collision movement refreshes the last-safe checkpoint");

    PlayerController::SetPose(player, 100.0f, 0.0f, 0.0f, 1.0f, false);
    const PlayerController::UpdateResult outOfBounds =
        PlayerController::UpdateMovement(player, input, 0.1f, collisionContext);
    Check(outOfBounds.respawned, "collision movement respawns an out-of-bounds player");
    Check(NearlyEqual(player.position[0], 0.0f) && NearlyEqual(player.yaw, 0.0f),
        "movement respawn restores the controller checkpoint");

    PlayerController::SetPose(player, 0.0f, 0.0f, 0.0f, 0.75f, false);
    Check(PlayerController::Unstick(player, collision),
        "unstick finds a safe floor through the collision interface");
    Check(player.onGround && NearlyEqual(player.position[1], 0.0f) &&
          NearlyEqual(player.yaw, 0.75f),
        "unstick preserves yaw and restores stable grounding");

    PlayerController::SetPose(player, 0, 0, 0, 0, true);
    input = {};
    input.strafe = 1;
    PlayerController::UpdateMovement(player, input, 0.1f, collisionContext);
    Check(NearlyEqual(player.position[0], 0.4f) && NearlyEqual(player.yaw, 0),
        "right strafe moves sideways without turning");
    input.strafe = -1;
    PlayerController::UpdateMovement(player, input, 0.1f, collisionContext);
    Check(NearlyEqual(player.position[0], 0), "left strafe reverses sideways displacement");
    input.forward = 1;
    PlayerController::UpdateMovement(player, input, 0.1f, collisionContext);
    Check(NearlyEqual(std::sqrt(player.position[0] * player.position[0] +
                               player.position[2] * player.position[2]), 0.4f),
        "diagonal strafing does not increase movement speed");

    PlayerController::SetPose(player, 0, 0, 0, 0, true);
    input = {};
    input.jump = true;
    PlayerController::UpdateMovement(player, input, 0.001f, collisionContext);
    Check(player.jumping && !player.onGround && player.position[1] < 0,
        "Space impulse leaves the floor even within ground-snap tolerance");
    const float launchVelocity = player.verticalVelocity;
    PlayerController::UpdateMovement(player, input, 0.01f, collisionContext);
    Check(player.verticalVelocity > launchVelocity, "a second press in the air does not reset jump velocity");
    input.jump = false;
    for (int frame = 0; frame < 120; ++frame)
        PlayerController::UpdateMovement(player, input, 1.0f / 60.0f, collisionContext);
    Check(player.onGround && !player.jumping && NearlyEqual(player.position[1], 0),
        "jump falls back onto the collision floor");
    input.jump = true;
    PlayerController::UpdateMovement(player, input, 0.01f, collisionContext);
    Check(player.jumping, "a new press after landing starts another jump");

    Check(std::string_view(PlayerController::MovementAnimation(player, input)) == "jmp",
        "airborne jump selects the jump clip");
    PlayerController::SetPose(player, 0, 0, 0, 0, true);
    input = {};
    Check(std::string_view(PlayerController::MovementAnimation(player, input)) == "idl_relaxed",
        "stationary player uses relaxed idle");
    input.forward = -1;
    Check(std::string_view(PlayerController::MovementAnimation(player, input)) == "mvb_relaxed",
        "backward movement selects backward animation, not forward walk");
    input.forward = 1;
    Check(std::string_view(PlayerController::MovementAnimation(player, input)) == "wlk_relaxed",
        "ordinary forward movement uses relaxed upper body");
    input.boost = true;
    Check(std::string_view(PlayerController::MovementAnimation(player, input)) == "run_relaxed",
        "boosted forward movement uses relaxed run");
    input.strafe = -1;
    Check(std::string_view(PlayerController::MovementAnimation(player, input)) == "mvr",
        "left strafe selects mvr even while moving diagonally");
    input.strafe = 1;
    Check(std::string_view(PlayerController::MovementAnimation(player, input)) == "mvl",
        "right strafe selects mvl");

    float ceilingPoints[9];
    std::copy(std::begin(floorPoints), std::end(floorPoints), ceilingPoints);
    for (int vertex = 0; vertex < 3; ++vertex)
        ceilingPoints[vertex * 3 + 1] = -PlayerController::kCollisionHeight - 0.25f;
    ZoneCollision::Triangle ceiling;
    Check(ZoneCollision::BuildTriangle(ceilingPoints, false, ceiling), "jump test ceiling is valid");
    collision.AddTriangle(ceiling, PlayerController::kCollisionRadius);
    PlayerController::SetPose(player, 0, 0, 0, 0, true);
    input = {};
    input.jump = true;
    PlayerController::UpdateMovement(player, input, 0.1f, collisionContext);
    Check(NearlyEqual(player.position[1], -0.25f) && NearlyEqual(player.verticalVelocity, 0),
        "jump head sweep stops against an overhead surface");
    input.jump = false;
    for (int frame = 0; frame < 60; ++frame)
        PlayerController::UpdateMovement(player, input, 1.0f / 60.0f, collisionContext);
    Check(player.onGround && NearlyEqual(player.position[1], 0), "ceiling contact falls back to the floor");
}

void TestMouseForward()
{
    for (bool leftFirst : { false, true })
    {
        for (bool releaseLeft : { false, true })
        {
            InputController::State input;
            InputController::PlayerMouseButton(input, leftFirst, true, 10, 20);
            Check(!InputController::MouseForwardActive(input), "one mouse button does not move the player");
            InputController::PlayerMouseButton(input, !leftFirst, true, 10, 20);
            Check(InputController::MouseForwardActive(input), "both mouse button orders enable forward movement");
            const auto delta = InputController::MouseMoved(input, 25, 20);
            Check(delta.x == 15 && delta.mode == InputController::DragMode::Orbit,
                "mouse forward mode supplies horizontal steering deltas");
            Check(!InputController::PlayerMouseButton(input, releaseLeft, false, 25, 20),
                "releasing a movement chord never triggers a world click");
            Check(!InputController::MouseForwardActive(input), "releasing either button stops mouse movement");
            Check(!InputController::PlayerMouseButton(input, !releaseLeft, false, 25, 20),
                "releasing the remaining button does not trigger a world click");
        }
    }
    InputController::State input;
    InputController::PlayerMouseButton(input, true, true, 0, 0);
    Check(InputController::PlayerMouseButton(input, true, false, 0, 0),
        "a standalone left click remains a world interaction");
    for (bool loseFocus : { false, true })
    {
        InputController::PlayerMouseButton(input, true, true, 0, 0);
        InputController::PlayerMouseButton(input, false, true, 0, 0);
        if (loseFocus) InputController::FocusLost(input);
        else InputController::CaptureChanged(input, reinterpret_cast<HWND>(1));
        Check(!InputController::MouseForwardActive(input) && !input.pendingWorldClick,
            "focus or capture loss cancels mouse movement and pending clicks");
    }
}

void TestFastRunning()
{
    InputController::State keyboard;
    Check(!InputController::Movement(keyboard).fastRunning, "extra-fast mode starts off");
    InputController::KeyDown(keyboard, VK_MENU);
    InputController::KeyDown(keyboard, VK_MENU);
    InputController::KeyUp(keyboard, VK_MENU);
    Check(InputController::Movement(keyboard).fastRunning,
        "Alt toggles extra-fast mode once and retains it after release");

    PlayerController::State player;
    PlayerController::InputSnapshot input;
    input.forward = 1.0f;
    input.fastRunning = InputController::Movement(keyboard).fastRunning;
    for (int frame = 0; frame < 60; ++frame)
        PlayerController::UpdateMovement(player, input, 1.0f / 60.0f, {});
    Check(NearlyEqual(player.position[2], -16.0f), "extra-fast running restores 16 units per second");
    Check(std::string_view(PlayerController::MovementAnimation(player, input)) == "run_relaxed",
        "extra-fast mode runs even when Shift mode is walking");
    Check(NearlyEqual(PlayerController::MovementAnimationRate(player, input), 16.0f / 6.0f),
        "extra-fast run animation keeps pace with movement");
    input.slow = true;
    Check(NearlyEqual(PlayerController::MovementAnimationRate(player, input), 16.0f / 6.0f * 0.35f),
        "Ctrl slows extra-fast animation proportionally");
    input.slow = false;
    input.strafe = 1.0f;
    Check(NearlyEqual(PlayerController::MovementAnimationRate(player, input), 1.0f),
        "strafe retains its normal animation pace with extra-fast mode selected");
    player.jumping = true;
    input.strafe = 0.0f;
    Check(NearlyEqual(PlayerController::MovementAnimationRate(player, input), 1.0f),
        "extra-fast mode does not accelerate the jump animation");

    InputController::KeyDown(keyboard, VK_SHIFT);
    InputController::KeyUp(keyboard, VK_SHIFT);
    InputController::FocusLost(keyboard);
    Check(InputController::Movement(keyboard).fastRunning, "focus loss retains extra-fast mode");
    InputController::KeyDown(keyboard, VK_MENU);
    Check(!InputController::Movement(keyboard).fastRunning && InputController::Movement(keyboard).running,
        "Alt turns extra-fast mode off and restores the selected Shift mode");
}

void TestInputController()
{
    InputController::State input;
    InputController::Initialize(input, nullptr, false);
    Check(input.dragMode == InputController::DragMode::None,
        "input controller starts without an active drag");
    Check(input.clientMouse.x == -1 && input.clientMouse.y == -1,
        "input controller starts without a client mouse position");

    InputController::KeyDown(input, 'Q');
    Check(InputController::Movement(input).strafe == -1 &&
          InputController::Movement(input).vertical == 1,
        "Q provides left strafe and preserves edit-mode camera ascent");
    InputController::KeyDown(input, 'E');
    Check(InputController::Movement(input).strafe == 0, "opposing strafe keys cancel");
    InputController::KeyUp(input, 'Q');
    Check(InputController::Movement(input).strafe == 1, "E provides right strafe");
    InputController::KeyUp(input, 'E');
    InputController::KeyDown(input, 0x20);
    InputController::KeyUp(input, 0x20);
    Check(InputController::ConsumeJump(input), "quick Space tap is retained until the next movement update");
    Check(!InputController::ConsumeJump(input), "jump requests are consumed once");
    InputController::KeyDown(input, 0x20);
    Check(InputController::ConsumeJump(input), "new Space press requests a jump");
    InputController::KeyDown(input, 0x20);
    Check(!InputController::ConsumeJump(input), "keyboard auto-repeat does not request repeated jumps");
    InputController::FocusLost(input);
    Check(!input.jumpHeld && !InputController::ConsumeJump(input), "focus loss clears jump input");

    Check(InputController::KeyDown(input, 'W') == InputController::Action::None,
        "movement keys do not emit application actions");
    InputController::MovementSnapshot movement = InputController::Movement(input);
    Check(movement.forward == 1.0f && InputController::IsMovementActive(input),
        "forward key state appears in the movement snapshot");

    InputController::KeyDown(input, 'A');
    movement = InputController::Movement(input);
    Check(movement.right == -1.0f, "left movement produces a negative right axis");
    InputController::KeyDown(input, 'D');
    movement = InputController::Movement(input);
    Check(movement.right == 0.0f, "opposing horizontal keys cancel each other");
    InputController::KeyUp(input, 'A');
    movement = InputController::Movement(input);
    Check(movement.right == 1.0f, "releasing left retains right movement");

    InputController::KeyDown(input, 0x10);
    InputController::KeyDown(input, 0x11);
    movement = InputController::Movement(input);
    Check(movement.boost && movement.slow, "modifier keys are retained in movement snapshots");
    Check(movement.running, "first Shift press toggles running on");
    InputController::KeyDown(input, 0x10);
    Check(InputController::Movement(input).running, "Shift auto-repeat does not toggle again");
    InputController::KeyUp(input, 0x10);
    Check(InputController::Movement(input).running && !InputController::Movement(input).boost,
        "releasing Shift retains running but releases fly-camera boost");
    InputController::KeyDown(input, 0x10);
    Check(!InputController::Movement(input).running, "second Shift press toggles walking");
    InputController::KeyUp(input, 0x10);
    InputController::KeyDown(input, 0x10);
    Check(InputController::KeyDown(input, 'O') == InputController::Action::OpenDat,
        "control plus O emits the open-DAT action");
    InputController::KeyUp(input, 0x11);
    Check(InputController::KeyDown(input, 'O') == InputController::Action::None,
        "O without control does not emit an action");
    Check(InputController::KeyDown(input, 'F') == InputController::Action::ToggleGameMode,
        "F emits the game-mode action");
    Check(InputController::KeyDown(input, 'G') == InputController::Action::UnstickPlayer,
        "G emits the unstick action");
    Check(InputController::KeyDown(input, 'V') == InputController::Action::CycleWeather,
        "V emits the weather-cycle action");
    Check(InputController::KeyDown(input, 'T') ==
            InputController::Action::ToggleCameraDebugOverlay,
        "T emits the camera-debug toggle action");
    Check(InputController::KeyDown(input, 'T') == InputController::Action::None,
        "held T does not repeatedly toggle camera debug telemetry");
    InputController::KeyUp(input, 'T');
    Check(InputController::KeyDown(input, 'T') ==
            InputController::Action::ToggleCameraDebugOverlay,
        "T toggles camera debug telemetry again after release");
    InputController::KeyUp(input, 'T');
    Check(InputController::KeyDown(input, 0x1B) == InputController::Action::None,
        "Escape does not emit a program exit action");
    Check(InputController::KeyDown(input, 0x08) == InputController::Action::Back,
        "Backspace emits the back action");
    Check(InputController::KeyDown(input, 0x0D) == InputController::Action::Confirm,
        "Enter emits the confirm action");

    InputController::BeginOrbit(input, 10, 20);
    Check(input.dragMode == InputController::DragMode::Orbit && input.cursorHidden,
        "orbit dragging requests a hidden software cursor");
    const InputController::DragDelta orbitDelta = InputController::MouseMoved(input, 13, 18);
    Check(orbitDelta.mode == InputController::DragMode::Orbit &&
          orbitDelta.x == 3 && orbitDelta.y == -2,
        "orbit dragging reports relative mouse movement");
    Check(input.clientMouse.x == 13 && input.clientMouse.y == 18,
        "mouse movement updates the tracked client position");
    InputController::MouseLeft(input);
    Check(input.clientMouse.x == -1 && input.clientMouse.y == -1,
        "mouse leave clears the tracked client position");

    InputController::SetHardwareCursorEnabled(input, true);
    Check(input.cursorHidden,
        "enabling the hardware cursor does not reveal an active orbit cursor");
    InputController::EndDrag(input);
    Check(input.dragMode == InputController::DragMode::None && !input.wantsCursorHidden,
        "ending a drag clears its capture and cursor intent");

    InputController::BeginPan(input, 4, 5);
    Check(input.dragMode == InputController::DragMode::Pan && !input.cursorHidden,
        "pan dragging keeps the cursor visible");
    InputController::FocusLost(input);
    movement = InputController::Movement(input);
    Check(input.dragMode == InputController::DragMode::None,
        "focus loss cancels an active drag");
    Check(movement.forward == 0.0f && movement.right == 0.0f &&
          !movement.boost && !movement.slow,
        "focus loss clears held movement and modifier keys");
    Check(movement.running, "focus loss preserves the selected run mode");
    InputController::KeyDown(input, 0x10);
    Check(!InputController::Movement(input).running, "Shift toggles normally after focus returns");

    InputController::Shutdown(input);
    Check(input.window == nullptr, "input-controller shutdown releases its window association");
}

void TestInteractionController()
{
    InteractionController::State interaction;
    Check(InteractionController::IsEditMode(interaction) &&
          !InteractionController::IsGameMode(interaction),
        "interaction controller starts in edit mode");
    Check(interaction.gameMusicId == 0,
        "interaction controller starts without selected game music");

    InteractionController::ModeTransition transition = InteractionController::SetMode(
        interaction, InteractionController::Mode::Game);
    Check(transition.changed && transition.playerCameraActive &&
          InteractionController::IsGameMode(interaction),
        "entering game mode reports a changed transition and active player camera");
    transition = InteractionController::SetMode(
        interaction, InteractionController::Mode::Game);
    Check(!transition.changed && transition.playerCameraActive,
        "reapplying game mode retains the required camera state");
    transition = InteractionController::ToggleMode(interaction);
    Check(transition.changed && !transition.playerCameraActive &&
          InteractionController::IsEditMode(interaction),
        "toggling game mode returns to edit mode and disables the player camera");

    InteractionController::SetGameMusicId(interaction, -12);
    Check(interaction.gameMusicId == 0, "negative game music IDs clamp to no selection");
    InteractionController::SetGameMusicId(interaction, 202);
    Check(interaction.gameMusicId == 202, "valid game music selection is retained");

    InteractionController::PlaybackContext context;
    InteractionController::PlaybackDecision decision =
        InteractionController::DecidePlayback(interaction, context);
    Check(decision.action == InteractionController::PlaybackAction::Stop &&
          decision.playbackAllowed,
        "edit mode does not play the selected game music");

    InteractionController::SetMode(interaction, InteractionController::Mode::Game);
    decision = InteractionController::DecidePlayback(interaction, context);
    Check(decision.action == InteractionController::PlaybackAction::PlayGameMusic &&
          decision.musicId == 202,
        "game mode plays the selected game music");

    context.titleScreenActive = true;
    context.titleMusicId = 108;
    decision = InteractionController::DecidePlayback(interaction, context);
    Check(decision.action == InteractionController::PlaybackAction::PlayTitleMusic &&
          decision.musicId == 108,
        "title music takes priority over selected game music");

    context.externalPlayerOpen = true;
    decision = InteractionController::DecidePlayback(interaction, context);
    Check(decision.action == InteractionController::PlaybackAction::ControlExternalPlayer &&
          decision.playbackAllowed,
        "an open external player receives the playback permission decision");

    context.soundsEnabled = false;
    decision = InteractionController::DecidePlayback(interaction, context);
    Check(decision.action == InteractionController::PlaybackAction::ControlExternalPlayer &&
          !decision.playbackAllowed,
        "disabling sounds pauses an open external player");

    context.externalPlayerOpen = false;
    decision = InteractionController::DecidePlayback(interaction, context);
    Check(decision.action == InteractionController::PlaybackAction::Stop &&
          !decision.playbackAllowed,
        "disabling sounds stops application music");

    context.soundsEnabled = true;
    context.titleScreenActive = false;
    context.playSoundsInBackground = false;
    context.applicationOwnsForeground = false;
    decision = InteractionController::DecidePlayback(interaction, context);
    Check(decision.action == InteractionController::PlaybackAction::Stop &&
          !decision.playbackAllowed,
        "foreground-only playback stops when no application window owns focus");

    context.applicationOwnsForeground = true;
    decision = InteractionController::DecidePlayback(interaction, context);
    Check(decision.action == InteractionController::PlaybackAction::PlayGameMusic &&
          decision.playbackAllowed,
        "foreground-only playback resumes for an application-owned window");

    context.applicationOwnsForeground = false;
    context.playSoundsInBackground = true;
    decision = InteractionController::DecidePlayback(interaction, context);
    Check(decision.action == InteractionController::PlaybackAction::PlayGameMusic &&
          decision.playbackAllowed,
        "background playback permits game music without application focus");

    InteractionController::SetGameMusicId(interaction, 0);
    decision = InteractionController::DecidePlayback(interaction, context);
    Check(decision.action == InteractionController::PlaybackAction::Stop,
        "game mode without a selected track stops application music");
}

void TestModelOwnershipContract()
{
    using OwnedModel = FFXIModelLifetime::OwnedModel;
    Check(std::is_default_constructible_v<OwnedModel>,
        "owned model resources can start empty");
    Check(std::is_destructible_v<OwnedModel>,
        "owned model resources release through deterministic destruction");
    Check(!std::is_copy_constructible_v<OwnedModel> &&
          !std::is_copy_assignable_v<OwnedModel>,
        "owned model resources cannot duplicate ownership");
    Check(std::is_move_constructible_v<OwnedModel> &&
          std::is_move_assignable_v<OwnedModel>,
        "owned model resources can transfer ownership");
    Check(std::is_nothrow_move_constructible_v<OwnedModel> &&
          std::is_nothrow_move_assignable_v<OwnedModel>,
        "owned model ownership transfers are noexcept for container safety");
}

void TestSceneModelLoaderContract()
{
    const SceneModelLoader::DatOptions options;
    Check(!options.enableTextureCompression && options.userContentLoad &&
          !options.renderEnvironment && !options.renderUnreferenced,
        "scene DAT loader defaults match an ordinary user-content load");
    Check(!std::is_copy_constructible_v<SceneModelLoader::Result> &&
          !std::is_copy_assignable_v<SceneModelLoader::Result>,
        "scene load results cannot duplicate model ownership");
    Check(std::is_nothrow_move_constructible_v<SceneModelLoader::Result> &&
          std::is_nothrow_move_assignable_v<SceneModelLoader::Result>,
        "scene load results transfer ownership safely across workflow boundaries");
}

void TestSceneLoadContext()
{
    SceneLoadContext::State state;
    Check(!state.HasRememberedRequest() && state.Path().empty(),
        "scene load context starts without a remembered request");

    SceneLoadContext::Options options;
    options.userContentLoad = false;
    options.renderEnvironment = true;
    options.renderUnreferenced = true;
    options.preserveGameScreen = true;
    state.Remember("C:\\FFXI\\ROM\\0\\1.DAT", "Remembered Zone", options);

    Check(state.HasRememberedRequest(),
        "scene load context recognizes a remembered path");
    Check(state.MatchesPath("c:\\ffxi\\rom\\0\\1.dat"),
        "scene load context compares remembered paths without case sensitivity");
    Check(!state.MatchesPath("C:\\FFXI\\ROM\\0\\2.DAT"),
        "scene load context rejects a different path");

    const SceneLoadContext::Request snapshot = state.Snapshot();
    Check(snapshot.path == "C:\\FFXI\\ROM\\0\\1.DAT" &&
          snapshot.name == "Remembered Zone",
        "scene load snapshots preserve path and preferred name");
    Check(!snapshot.options.userContentLoad && snapshot.options.renderEnvironment &&
          snapshot.options.renderUnreferenced && snapshot.options.preserveGameScreen,
        "scene load snapshots preserve every load option");

    state.Remember("C:\\FFXI\\ROM\\0\\2.DAT", "Replacement", {});
    Check(snapshot.path == "C:\\FFXI\\ROM\\0\\1.DAT",
        "scene load snapshots remain stable while the active request changes");

    Check(SceneLoadContext::FormatLoadedLabel(nullptr, nullptr, nullptr, nullptr) ==
              "No zone loaded",
        "scene label formatting handles an empty scene");
    Check(SceneLoadContext::FormatLoadedLabel(
              "Preferred", "Discovered", "C:\\FFXI\\ROM\\0\\1.DAT", "ROM/0/1.DAT") ==
              "Loaded zone: Preferred (ROM/0/1.DAT)",
        "scene label formatting gives the preferred name priority");
    Check(SceneLoadContext::FormatLoadedLabel(
              nullptr, "Discovered", "C:\\FFXI\\ROM\\0\\1.DAT", "ROM/0/1.DAT") ==
              "Loaded zone: Discovered (ROM/0/1.DAT)",
        "scene label formatting falls back to the discovered zone name");
    Check(SceneLoadContext::FormatLoadedLabel(
              nullptr, nullptr, "C:\\FFXI\\ROM\\0\\1.DAT", "ROM/0/1.DAT") ==
              "Loaded zone: 1.DAT (ROM/0/1.DAT)",
        "scene label formatting falls back to the source filename");

    state.Clear();
    Check(!state.HasRememberedRequest() && state.Path().empty(),
        "scene load context can explicitly discard remembered state");
}

void TestSceneRequestResolverContract()
{
    SceneRequestResolver::Result result;
    Check(!result.Succeeded() && result.zoneId == -1,
        "unresolved scene requests do not report success");

    result.request.path = "C:\\FFXI\\ROM\\0\\1.DAT";
    Check(result.Succeeded(),
        "a resolved scene request requires a path and no error");

    result.error = SceneRequestResolver::Error::ModelFileMissing;
    Check(!result.Succeeded(),
        "a scene request with a resolution error does not report success");

    const SceneLoadContext::Options options = result.request.options;
    Check(options.userContentLoad && !options.renderEnvironment &&
          !options.renderUnreferenced && !options.preserveGameScreen,
        "resolved model requests default to ordinary user-content loading");
}

void TestTitleSceneAssetsContract()
{
    const TitleSceneAssets::Paths paths;
    Check(!paths.logoDat && !paths.atlasDat && !paths.uiDat,
        "title scene asset paths start empty");
    Check(!std::is_copy_constructible_v<TitleSceneAssets::State> &&
          !std::is_copy_assignable_v<TitleSceneAssets::State>,
        "title scene assets cannot duplicate model ownership");
    Check(std::is_nothrow_move_constructible_v<TitleSceneAssets::State> &&
          std::is_nothrow_move_assignable_v<TitleSceneAssets::State>,
        "title scene asset ownership transfers are noexcept");
}

void TestCreationModelLoaderContract()
{
    const CreationModelLoader::Request request;
    Check(!request.ffxiRoot && !request.bodyMeshDat && !request.headMeshDat &&
          !request.bodyAnimationDat && !request.headAnimationDat &&
          !request.enableTextureCompression,
        "creation model loader requests start without borrowed inputs");
    Check(!std::is_copy_constructible_v<CreationModelLoader::Result> &&
          !std::is_copy_assignable_v<CreationModelLoader::Result>,
        "creation model load results cannot duplicate model ownership");
    Check(std::is_nothrow_move_constructible_v<CreationModelLoader::Result> &&
          std::is_nothrow_move_assignable_v<CreationModelLoader::Result>,
        "creation model load results transfer ownership without throwing");
}

void TestGeneratedPlayerDatSet()
{
    FFXIDatSet::PlayerOptions options;
    char datSet[4096] = {};
    Check(FFXIDatSet::BuildPlayer("C:\\FFXI\\", options, datSet, sizeof(datSet)),
        "default generated-player options build a DAT set");

    std::string_view text(datSet);
    Check(text.starts_with("NOESIS_FF11_DAT_SET\n") &&
          text.find("setPathAbs \"C:\\FFXI\\\"") != std::string_view::npos,
        "generated-player DAT sets declare their format and installation root");
    Check(text.find("dat \"__skeleton\" \"ROM/27/82.dat\"") != std::string_view::npos &&
          text.find("dat \"__animation\" \"ROM/32/13.dat\"") != std::string_view::npos,
        "generated-player DAT sets include the race skeleton and animation bank");
    Check(text.find("dat \"face\" \"ROM/27/87.dat\"") != std::string_view::npos &&
          text.find("dat \"head\" \"ROM/27/103.dat\"") != std::string_view::npos,
        "default generated-player DAT sets include base face and head paths");
    Check(text.find("dat \"main\"") == std::string_view::npos,
        "default generated-player DAT sets omit unequipped weapons");

    options.faceVariant = 1;
    options.headItem = 2;
    options.mainItem = 1;
    options.animationBank = 1;
    Check(FFXIDatSet::BuildPlayer("C:\\FFXI\\", options, datSet, sizeof(datSet)),
        "customized generated-player options build a DAT set");
    text = datSet;
    Check(text.find("dat \"face\" \"ROM/27/88.dat\"") != std::string_view::npos &&
          text.find("dat \"head\" \"ROM/27/104.dat\"") != std::string_view::npos &&
          text.find("dat \"main\" \"ROM/29/20.dat\"") != std::string_view::npos &&
          text.find("dat \"__animation\" \"ROM/32/14.dat\"") != std::string_view::npos,
        "customized generated-player DAT sets apply variants and equipped weapons");
}

void TestPlayerModelLoaderContract()
{
    const PlayerModelLoader::Request request;
    Check(!request.ffxiRoot && request.customization.raceIndex == 0 &&
          !request.enableTextureCompression &&
          NearlyEqual(request.footContactAdjustment, 0.212f),
        "player model loader requests have safe generated-player defaults");
    Check(!std::is_copy_constructible_v<PlayerModelLoader::Result> &&
          !std::is_copy_assignable_v<PlayerModelLoader::Result>,
        "player model load results cannot duplicate model ownership");
    Check(std::is_nothrow_move_constructible_v<PlayerModelLoader::Result> &&
          std::is_nothrow_move_assignable_v<PlayerModelLoader::Result>,
        "player model load results transfer ownership without throwing");
}

void TestLowPolyCharacterPanelContract()
{
    const LowPolyCharacterPanel::State state;
    Check(!state.owner && !state.window && !state.equipment &&
          !state.faceVariant && !state.eventHandler && !state.eventContext,
        "low-poly character panels start detached from application state");
    Check(!std::is_copy_constructible_v<LowPolyCharacterPanel::State> &&
          !std::is_copy_assignable_v<LowPolyCharacterPanel::State> &&
          !std::is_move_constructible_v<LowPolyCharacterPanel::State> &&
          !std::is_move_assignable_v<LowPolyCharacterPanel::State>,
        "low-poly character panel callback state has a stable address");
    Check(LowPolyCharacterPanel::Command::LoadPreset !=
              LowPolyCharacterPanel::Command::SelectionChanged,
        "low-poly panel actions use distinct typed commands");
}

void TestHighPolyCreationPanelContract()
{
    const HighPolyCreationPanel::State state;
    Check(!state.owner && !state.window && !state.selection &&
          !state.animationIndex && !state.animatedCamera &&
          !state.characterName && state.characterNameCapacity == 0 &&
          !state.eventHandler && !state.eventContext,
        "high-poly creation panels start detached from application state");
    Check(!std::is_copy_constructible_v<HighPolyCreationPanel::State> &&
          !std::is_copy_assignable_v<HighPolyCreationPanel::State> &&
          !std::is_move_constructible_v<HighPolyCreationPanel::State> &&
          !std::is_move_assignable_v<HighPolyCreationPanel::State>,
        "high-poly creation panel callback state has a stable address");
    Check(HighPolyCreationPanel::Command::ReturnToTitle !=
              HighPolyCreationPanel::Command::SelectionChanged,
        "high-poly panel actions use distinct typed commands");
}

void TestZoneObjectPanelStateContract()
{
    const ZoneObjectPanel::State state;
    const ZoneObjectPanel::RefreshData refresh;
    Check(!state.owner && !state.window && !state.zoneLabel &&
          !state.placedObjectList && !state.unreferencedObjectList &&
          !state.collisionObjectList && !state.drawBatchList &&
          !state.dataTree && !state.rawDataTree && !state.collisionDataTree,
        "zone-object panels start without borrowed window handles");
    Check(!state.populatingObjectList && state.placedColumnMode == -1 &&
          state.unreferencedColumnMode == -1 && state.collisionColumnMode == -1 &&
          state.drawBatchColumnMode == -1 &&
          state.treeSelectedMapObjectIndex == -1 && !state.combinedObjectTree &&
          state.activeSplitter == 0 && !state.brushes.panel &&
          !state.brushes.control && !state.brushes.edit,
        "zone-object panel bookkeeping has safe detached defaults");
    Check(!std::is_copy_constructible_v<ZoneObjectPanel::State> &&
          !std::is_copy_assignable_v<ZoneObjectPanel::State> &&
          !std::is_move_constructible_v<ZoneObjectPanel::State> &&
          !std::is_move_assignable_v<ZoneObjectPanel::State> &&
          ZoneObjectPanel::kToolButtonCount == 5 &&
          ZoneObjectPanel::kUnreferencedFieldCount == 9 &&
          ZoneObjectPanel::kPaneCount == 3,
        "zone-object panel state has stable storage and explicit collection sizes");
    Check(!state.eventHandler && !state.eventContext &&
          std::string_view(state.creationZoneLabel).empty() &&
          !state.creationCollisionVisible && !state.creationEditingEnabled &&
          ZoneObjectPanel::IDC_ZONE_OBJECT_LIST == 7201 &&
          ZoneObjectPanel::IDC_ZONE_COMBINE_TREE_TOGGLE == 7236,
        "zone-object panel presentation and callback defaults are stable");
    Check(ZoneObjectPanel::EventType::Command !=
              ZoneObjectPanel::EventType::MapObjectSelectionChanged &&
          ZoneObjectPanel::Command::ShowPlaced !=
              ZoneObjectPanel::Command::SetCollisionVisibility &&
          ZoneObjectPanel::Command::ApplyTransform !=
              ZoneObjectPanel::Command::CenterSelected,
        "zone-object panel actions use distinct typed commands");
    Check(std::string_view(refresh.zoneLabel).empty() && !refresh.overrides &&
          !refresh.hiddenObjectNames && refresh.collisionTriangleCount == 0 &&
          refresh.modelMeshCount == 0 && !refresh.editingEnabled,
        "zone-object panel refresh inputs have safe detached defaults");
}

void TestOrbitCameraInputOperations()
{
    OrbitCamera::State camera;
    OrbitCamera::Rotate(camera, 10, -10);
    Check(NearlyEqual(camera.yaw, -0.05f) && NearlyEqual(camera.pitch, -0.20f),
        "orbit rotation applies mouse sensitivity to yaw and pitch");

    OrbitCamera::Rotate(camera, 0, -1000);
    Check(NearlyEqual(camera.pitch, 1.55f), "orbit rotation clamps maximum pitch");
    OrbitCamera::Rotate(camera, 0, 1000);
    Check(NearlyEqual(camera.pitch, -1.55f), "orbit rotation clamps minimum pitch");

    camera.distance = 5.0f;
    OrbitCamera::Zoom(camera, 1.0f);
    Check(NearlyEqual(camera.distance, 4.4f), "orbit zoom applies one wheel step");
    OrbitCamera::Zoom(camera, 1000.0f);
    Check(NearlyEqual(camera.distance, 0.05f), "orbit zoom clamps its minimum distance");
    OrbitCamera::Zoom(camera, -1000000.0f);
    Check(NearlyEqual(camera.distance, 2000.0f), "orbit zoom clamps its maximum distance");

    const float oldTargetX = camera.target[0];
    const float oldTargetY = camera.target[1];
    const float oldTargetZ = camera.target[2];
    OrbitCamera::Pan(camera, 5, -4);
    Check(!NearlyEqual(camera.target[0], oldTargetX) ||
          !NearlyEqual(camera.target[1], oldTargetY) ||
          !NearlyEqual(camera.target[2], oldTargetZ),
        "orbit panning changes the camera target");
}
}

void TestCollisionRegressions()
{
    auto add = [](ZoneCollision::Mesh& mesh, const float* points) {
        ZoneCollision::Triangle triangle;
        Check(ZoneCollision::BuildTriangle(points, false, triangle), "valid regression triangle");
        mesh.AddTriangle(triangle, 0.0f);
    };
    for (bool reverse : { false, true })
    {
        ZoneCollision::Mesh mesh;
        float wall[] = { 0,-10,-10, 0,10,-10, 0,0,10 };
        if (reverse) for (int i = 0; i < 3; ++i) std::swap(wall[i], wall[3+i]);
        add(mesh, wall);
        for (float side : { -1.0f, 1.0f })
        {
            float position[] = { side * 1.0f, 0, 0 };
            float velocity = 0;
            bool grounded = false;
            ZoneCollision::MoveHorizontal(mesh, 0.72f, 3.2f, 0.85f, 1.25f,
                position, velocity, grounded, -side * 20.0f, 0);
            Check(position[0] * side >= 0.719f, "thin wall blocks both windings and approach sides over long moves");
            ZoneCollision::MoveHorizontal(mesh, 0.72f, 3.2f, 0.85f, 1.25f,
                position, velocity, grounded, side * 0.2f, 0.2f);
            Check(position[0] * side > 0.9f && position[2] > 0.19f, "player can move away from a wall");
        }
    }
    ZoneCollision::Mesh ledge;
    const float riser[] = { 0,-0.5f,-10, 0,10,-10, 0,-0.5f,10 };
    const float top[] = { 0,-0.5f,-10, 10,-0.5f,-10, 0,-0.5f,10 };
    add(ledge, riser); add(ledge, top);
    float position[] = { -0.1f,0,0 };
    float velocity = 0;
    bool grounded = true;
    ZoneCollision::MoveHorizontal(ledge, 0.72f, 3.2f, 0.85f, 1.25f,
        position, velocity, grounded, 0.2f, 0);
    Check(position[0] > 0.09f && NearlyEqual(position[1], -0.5f), "low ledge with deep riser is walkable");
    Check(!ZoneCollision::OverlapsWallAt(ledge.Triangles(), ledge.Index(),
        position[0], position[1], position[2], 0.72f, 3.2f, 0.85f), "ledge floor is safe at its edge");

    ZoneCollision::Mesh gap;
    const float farFloor[] = { 0.05f,0,-10, 10,0,-10, 0.05f,0,10 };
    add(gap, farFloor);
    position[0] = -0.1f; position[1] = 0; position[2] = 0; grounded = true;
    ZoneCollision::MoveHorizontal(gap, 0.72f, 3.2f, 0.85f, 1.25f,
        position, velocity, grounded, 0.1f, 0);
    Check(position[0] > -0.01f && !grounded, "missing floor sample does not become an invisible wall");
    ZoneCollision::MoveHorizontal(gap, 0.72f, 3.2f, 0.85f, 1.25f,
        position, velocity, grounded, 0.1f, 0);
    ZoneCollision::UpdateVerticalMotion(gap, 0.72f, 0.85f, 0.35f, 80, 28, 80,
        0.016f, position, velocity, grounded, 3.2f);
    Check(position[0] > 0.09f && grounded, "player crosses a narrow floor seam and regains support");
}

int main()
{
    NpcInteraction::State npcSelection;
    npcSelection.targets = { { 10, 0, 0, 100, 100, 0.8f }, { 20, 25, 25, 75, 75, 0.2f } };
    Check(!npcSelection.Click(50, 50) && npcSelection.selected == 20, "Nearest overlapping NPC is selected without talking");
    Check(npcSelection.Click(50, 50), "Click selected NPC opens dialogue");
    Check(!npcSelection.Click(10, 10) && npcSelection.selected == 10, "Click different NPC changes selection without dialogue");
    Check(!npcSelection.Click(150, 150) && npcSelection.selected == 0, "Empty space clears NPC selection");
    npcSelection.targets.clear();
    Check(!npcSelection.Click(50, 50), "Unrendered NPC cannot be selected");

    {
        PlayerController::State player;
        PlayerController::InputSnapshot input;
        input.forward = 1;
        PlayerController::SimulationContext context;
        context.beforeHorizontalMove = [](const PlayerController::State& before, float dx, float dz, float dt) {
            Check(NearlyEqual(before.position[2],0), "dynamic contact runs before player displacement");
            Check(NearlyEqual(dx,0) && dz<0 && NearlyEqual(dt,0.1f), "dynamic contact receives clamped intended motion");
        };
        PlayerController::UpdateMovement(player,input,1.0f,context);
    }
    TestCollisionRegressions();
    TestDefaultState();
    TestResolutionOptions();
    TestWindowModes();
    TestSoundOptions();
    TestDrawDistanceOptions();
    TestQualityModes();
    TestPlayerControllerState();
    TestPlayerControllerMovement();
    TestInputController();
    TestFastRunning();
    TestMouseForward();
    TestInteractionController();
    TestModelOwnershipContract();
    TestSceneModelLoaderContract();
    TestSceneLoadContext();
    TestSceneRequestResolverContract();
    TestTitleSceneAssetsContract();
    TestCreationModelLoaderContract();
    TestGeneratedPlayerDatSet();
    TestPlayerModelLoaderContract();
    TestLowPolyCharacterPanelContract();
    TestHighPolyCreationPanelContract();
    TestZoneObjectPanelStateContract();
    TestOrbitCameraInputOperations();

    if (gFailures == 0)
    {
        std::cout << "PASS: " << gChecks << " logic checks\n";
        return 0;
    }

    std::cerr << "FAILED: " << gFailures << " of " << gChecks << " checks\n";
    return 1;
}
