#pragma once

#include "application_settings.h"

namespace ApplicationSettingsStore
{
const char* RegistryKey();

// Load and save persisted settings only. Runtime effects such as device resets,
// audio updates, and input refreshes stay with the application shell.
void Load(ApplicationSettings::State& settings);
void SaveCustomTextures(const ApplicationSettings::State& settings);
void SaveExtendedGraphics(const ApplicationSettings::State& settings);
void SaveKeyBindings(const ApplicationSettings::State& settings);
void SaveTitleBackground(const ApplicationSettings::State& settings);
void SaveAll(const ApplicationSettings::State& settings);
}
