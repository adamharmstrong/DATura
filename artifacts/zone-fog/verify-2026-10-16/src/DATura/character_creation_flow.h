#pragma once

#include "character_save_data.h"
#include "ffxi_stat_system.h"

namespace CharacterCreationFlow
{
enum class Stage
{
    Inactive,
    Customization,
    StartingJob,
    NationSelection,
};

struct State
{
    Stage stage = Stage::Inactive;
    int selectedStartingJob = 0;
    FFXIStats::Job startingJob = FFXIStats::kJob_WAR;
    int selectedNation = 0;
};

int StartingJobCount();
const char* StartingJobName(int index);
int SelectedStartingJob(const State& state);
void ResetStartingJobSelection(State& state);
void SetStage(State& state, Stage stage);
bool IsStartingJobSelectionActive(const State& state);
bool IsNationSelectionActive(const State& state);
Stage Back(State& state);
void SetStartingJobSelection(State& state, int index);
void MoveStartingJobSelection(State& state, int delta);
FFXIStats::Job ConfirmStartingJobSelection(State& state);
FFXIStats::Job StartingJob(const State& state);
int SelectedNation(const State& state, int nationCount);
void SelectNation(State& state, int index, int nationCount);
CharacterSaveData::Data PrepareCharacterData(
    int playerRaceIndex, int homeNationIndex, FFXIStats::Job startingJob);
}
