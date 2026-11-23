#pragma once

#include <windows.h>
#include <d3d9.h>
#include <string>
#include <vector>

// Shared query UI. Keep the existing conversation API while moving rendering
// out of the chatlog header. Event selection/values remain owned by the caller.
namespace NpcChatWindow
{
extern HWND choiceWindow;
extern std::wstring choicePrompt;
extern std::vector<std::wstring> choiceOptions;
extern int choiceSelected;
extern int choiceFirstVisible;
inline constexpr int queryVisibleRows = 3;

void HideChoices();
void LayoutChoices(HWND owner);
void ShowChoices(HWND owner, const std::string& prompt,
    const std::vector<std::string>& options, int selected = 0);
void SetChoiceSelection(HWND owner, int selected);
// Call after the chatlog, before Present; independent of chatlog visibility.
void DrawChoices(IDirect3DDevice9* device);
}
