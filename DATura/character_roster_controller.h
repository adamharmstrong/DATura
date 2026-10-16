#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace CharacterRosterController
{
enum class Mode
{
    None,
    SelectCharacter,
    DeleteCharacter,
};

enum class IntentKind
{
    None,
    LoadSelectedCharacter,
    ConfirmDeleteSelectedCharacter,
    ReturnToTitleScreen,
};

struct State
{
    Mode mode = Mode::None;
    int selectedIndex = 0;
};

struct Candidate
{
    std::string name;
    std::string dataSetPath;
    std::string dataPath;
};

struct Intent
{
    IntentKind kind = IntentKind::None;
    Candidate candidate;
};

struct DeleteRequest
{
    std::string dataSetPath;
    std::string noesisPath;
    std::string dataPath;
};

void Reset(State& state);
void SetMode(State& state, Mode mode);
Mode CurrentMode(const State& state);
bool IsActive(const State& state);
Intent Back(const State& state);
void Select(State& state, int index, std::size_t rosterSize);
int SelectedIndex(const State& state, std::size_t rosterSize);
Intent Confirm(const State& state, const Candidate& candidate);
std::vector<Candidate> EnumerateCandidates(const std::string& characterDirectory);
DeleteRequest PrepareDelete(const Candidate& candidate);
}
