#pragma once

#include <windows.h>

#define WM_DATURA_AUDIO_PLAYER_CLOSED (WM_APP + 42)

// Opens (or focuses) the installed FFXI music and sound-effect browser.
void AudioPlayer_Show(HWND owner, const char* ffxiRootPath);
void AudioPlayer_SetRootPath(const char* ffxiRootPath);

// The main window uses these to keep contextual game music from replacing a
// manual preview and to honor the application's background-audio settings.
bool AudioPlayer_IsOpen();
bool AudioPlayer_OwnsWindow(HWND window);
void AudioPlayer_SetPlaybackAllowed(bool allowed);
void AudioPlayer_StopPlayback();
