#include "interaction_controller.h"

#include <algorithm>

namespace InteractionController
{
bool IsEditMode(const State& state)
{
    return state.mode == Mode::Edit;
}

bool IsGameMode(const State& state)
{
    return state.mode == Mode::Game;
}

ModeTransition SetMode(State& state, const Mode mode)
{
    const bool changed = state.mode != mode;
    state.mode = mode;
    return { changed, mode == Mode::Game };
}

ModeTransition ToggleMode(State& state)
{
    return SetMode(state, IsEditMode(state) ? Mode::Game : Mode::Edit);
}

void SetGameMusicId(State& state, const int musicId)
{
    state.gameMusicId = std::max(musicId, 0);
}

PlaybackDecision DecidePlayback(const State& state, const PlaybackContext& context)
{
    const bool playbackAllowed = context.soundsEnabled &&
        (context.playSoundsInBackground || context.applicationOwnsForeground);

    if (context.externalPlayerOpen)
    {
        return {
            PlaybackAction::ControlExternalPlayer,
            0,
            playbackAllowed,
        };
    }

    if (!playbackAllowed)
        return {};

    if (context.titleScreenActive)
    {
        return {
            PlaybackAction::PlayTitleMusic,
            std::max(context.titleMusicId, 0),
            true,
        };
    }

    if (IsGameMode(state) && state.gameMusicId > 0)
    {
        return {
            PlaybackAction::PlayGameMusic,
            state.gameMusicId,
            true,
        };
    }

    PlaybackDecision decision;
    decision.playbackAllowed = true;
    return decision;
}
}
