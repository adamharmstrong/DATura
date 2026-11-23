#include "stdafx.h"
#include "character_creation_flow.h"

#include <algorithm>
#include <array>

namespace CharacterCreationFlow
{
namespace
{
constexpr std::array<FFXIStats::Job, 6> kStartingJobs =
{
    FFXIStats::kJob_WAR, FFXIStats::kJob_MNK, FFXIStats::kJob_WHM,
    FFXIStats::kJob_BLM, FFXIStats::kJob_RDM, FFXIStats::kJob_THF
};

constexpr std::array<const char*, 6> kStartingJobNames =
{
    "Warrior", "Monk", "White Mage", "Black Mage", "Red Mage", "Thief"
};

int ClampIndex(const int index)
{
    return index >= 0 && index < static_cast<int>(kStartingJobs.size()) ? index : 0;
}
}

int StartingJobCount()
{
    return static_cast<int>(kStartingJobs.size());
}

const char* StartingJobName(const int index)
{
    return kStartingJobNames[ClampIndex(index)];
}

int SelectedStartingJob(const State& state)
{
    return ClampIndex(state.selectedStartingJob);
}

void ResetStartingJobSelection(State& state)
{
    state.selectedStartingJob = 0;
}

void SetStage(State& state, const Stage stage)
{
    state.stage = stage;
}

bool IsStartingJobSelectionActive(const State& state)
{
    return state.stage == Stage::StartingJob;
}

bool IsNationSelectionActive(const State& state)
{
    return state.stage == Stage::NationSelection;
}

Stage Back(State& state)
{
    if (state.stage == Stage::StartingJob)
        state.stage = Stage::Customization;
    else if (state.stage == Stage::NationSelection)
        state.stage = Stage::StartingJob;
    return state.stage;
}

void SetStartingJobSelection(State& state, const int index)
{
    state.selectedStartingJob = ClampIndex(index);
}

void MoveStartingJobSelection(State& state, const int delta)
{
    const int count = StartingJobCount();
    const int current = SelectedStartingJob(state);
    state.selectedStartingJob = (current + delta % count + count) % count;
}

FFXIStats::Job ConfirmStartingJobSelection(State& state)
{
    state.selectedStartingJob = ClampIndex(state.selectedStartingJob);
    state.startingJob = kStartingJobs[state.selectedStartingJob];
    return state.startingJob;
}

FFXIStats::Job StartingJob(const State& state)
{
    return state.startingJob;
}

int SelectedNation(const State& state, const int nationCount)
{
    return nationCount > 0
        ? std::clamp(state.selectedNation, 0, nationCount - 1) : 0;
}

void SelectNation(State& state, const int index, const int nationCount)
{
    state.selectedNation = nationCount > 0
        ? std::clamp(index, 0, nationCount - 1) : 0;
}

CharacterSaveData::Data PrepareCharacterData(
    const int playerRaceIndex, const int homeNationIndex,
    const FFXIStats::Job startingJob)
{
    CharacterSaveData::Data data;
    if (playerRaceIndex <= 1) data.stats.race = FFXIStats::kRace_Hume;
    else if (playerRaceIndex <= 3) data.stats.race = FFXIStats::kRace_Elvaan;
    else if (playerRaceIndex <= 5) data.stats.race = FFXIStats::kRace_Tarutaru;
    else if (playerRaceIndex == 6) data.stats.race = FFXIStats::kRace_Mithra;
    else if (playerRaceIndex == 7) data.stats.race = FFXIStats::kRace_Galka;
    else data.stats.race = FFXIStats::kRace_Hume;
    data.homeNationIndex = std::clamp(homeNationIndex, 0, 2);
    data.stats.mainJob = startingJob;
    data.stats.mainLevel = 1;
    data.stats.supportJob = FFXIStats::kJob_None;
    data.stats.supportLevel = 0;
    return data;
}
}
