#pragma once

namespace InteractionController
{
enum class Mode
{
    Edit,
    Game,
};

enum class PlaybackAction
{
    Stop,
    PlayTitleMusic,
    PlayGameMusic,
    ControlExternalPlayer,
};

struct State
{
    Mode mode = Mode::Edit;
    int gameMusicId = 0;
};

struct ModeTransition
{
    bool changed = false;
    bool playerCameraActive = false;
};

struct PlaybackContext
{
    bool soundsEnabled = true;
    bool playSoundsInBackground = true;
    bool applicationOwnsForeground = true;
    bool externalPlayerOpen = false;
    bool titleScreenActive = false;
    int titleMusicId = 0;
};

struct PlaybackDecision
{
    PlaybackAction action = PlaybackAction::Stop;
    int musicId = 0;
    bool playbackAllowed = false;
};

bool IsEditMode(const State& state);
bool IsGameMode(const State& state);
ModeTransition SetMode(State& state, Mode mode);
ModeTransition ToggleMode(State& state);
void SetGameMusicId(State& state, int musicId);
PlaybackDecision DecidePlayback(const State& state, const PlaybackContext& context);
}
