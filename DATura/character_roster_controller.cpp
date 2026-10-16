#include "stdafx.h"
#include "character_roster_controller.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <system_error>

namespace CharacterRosterController
{
namespace
{
bool IsCharacterDataSet(const std::filesystem::path& path)
{
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
        [](const unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return extension == ".ff11datset";
}
}

void Reset(State& state)
{
    state.mode = Mode::None;
    state.selectedIndex = 0;
}

void SetMode(State& state, const Mode mode)
{
    state.mode = mode;
}

Mode CurrentMode(const State& state)
{
    return state.mode;
}

bool IsActive(const State& state)
{
    return state.mode != Mode::None;
}

Intent Back(const State& state)
{
    return IsActive(state)
        ? Intent{ IntentKind::ReturnToTitleScreen, {} }
        : Intent{};
}

void Select(State& state, const int index, const std::size_t rosterSize)
{
    if (rosterSize == 0)
    {
        state.selectedIndex = 0;
        return;
    }
    state.selectedIndex = std::clamp(index, 0, static_cast<int>(rosterSize - 1));
}

int SelectedIndex(const State& state, const std::size_t rosterSize)
{
    if (rosterSize == 0)
        return 0;
    return std::clamp(state.selectedIndex, 0, static_cast<int>(rosterSize - 1));
}

Intent Confirm(const State& state, const Candidate& candidate)
{
    if (!IsActive(state))
        return {};
    return {
        state.mode == Mode::SelectCharacter
            ? IntentKind::LoadSelectedCharacter
            : IntentKind::ConfirmDeleteSelectedCharacter,
        candidate
    };
}

std::vector<Candidate> EnumerateCandidates(const std::string& characterDirectory)
{
    std::vector<Candidate> candidates;
    std::error_code error;
    const std::filesystem::directory_iterator end;
    for (std::filesystem::directory_iterator entry(characterDirectory, error);
         !error && entry != end; entry.increment(error))
    {
        std::error_code typeError;
        if (!entry->is_regular_file(typeError) || typeError ||
            !IsCharacterDataSet(entry->path()))
            continue;

        Candidate candidate;
        candidate.name = entry->path().stem().string();
        candidate.dataSetPath = entry->path().string();
        std::filesystem::path dataPath = entry->path();
        dataPath.replace_extension(".ini");
        candidate.dataPath = dataPath.string();
        candidates.push_back(std::move(candidate));
    }
    return candidates;
}

DeleteRequest PrepareDelete(const Candidate& candidate)
{
    DeleteRequest request;
    request.dataSetPath = candidate.dataSetPath;
    std::filesystem::path noesisPath(candidate.dataSetPath);
    noesisPath.replace_extension(".noesis");
    request.noesisPath = noesisPath.string();
    request.dataPath = candidate.dataPath;
    return request;
}
}
