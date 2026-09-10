#pragma once

#include "zone_object_panel_style.h"
#include "zone_object_transform.h"

#include <commctrl.h>
#include <windows.h>

#include <map>
#include <string>
#include <vector>

namespace ZoneObjectPanel
{
constexpr int kToolButtonCount = 5;
constexpr int kUnreferencedFieldCount = 9;
constexpr int kPaneCount = 3;
constexpr UINT kPopulateMessage = WM_APP + 31;

constexpr int IDC_ZONE_OBJECT_LIST = 7201;
constexpr int IDC_ZONE_LABEL = 7202;
constexpr int IDC_ZONE_PLACED_SHOW_ALL = 7203;
constexpr int IDC_ZONE_PLACED_HIDE_ALL = 7204;
constexpr int IDC_ZONE_UNREF_X = 7205;
constexpr int IDC_ZONE_UNREF_Y = 7206;
constexpr int IDC_ZONE_UNREF_Z = 7207;
constexpr int IDC_ZONE_UNREF_RX = 7208;
constexpr int IDC_ZONE_UNREF_RY = 7209;
constexpr int IDC_ZONE_UNREF_RZ = 7210;
constexpr int IDC_ZONE_UNREF_SX = 7211;
constexpr int IDC_ZONE_UNREF_SY = 7212;
constexpr int IDC_ZONE_UNREF_SZ = 7213;
constexpr int IDC_ZONE_UNREF_APPLY = 7214;
constexpr int IDC_ZONE_COLLISION_VISIBLE = 7215;
constexpr int IDC_ZONE_LOADING_LABEL = 7216;
constexpr int IDC_ZONE_SHOW_SELECTED = 7217;
constexpr int IDC_ZONE_HIDE_SELECTED = 7218;
constexpr int IDC_ZONE_HIGHLIGHT_SELECTED = 7219;
constexpr int IDC_ZONE_CENTER_SELECTED = 7220;
constexpr int IDC_ZONE_UNREF_OBJECT_LIST = 7221;
constexpr int IDC_ZONE_COLLISION_OBJECT_LIST = 7222;
constexpr int IDC_ZONE_UNREF_SHOW_ALL = 7223;
constexpr int IDC_ZONE_UNREF_HIDE_ALL = 7224;
constexpr int IDC_ZONE_TOOL_SELECT = 7225;
constexpr int IDC_ZONE_TOOL_MOVE = 7226;
constexpr int IDC_ZONE_TOOL_ROTATE = 7227;
constexpr int IDC_ZONE_TOOL_SCALE = 7228;
constexpr int IDC_ZONE_TOOL_TRANSFORM = 7229;
constexpr int IDC_ZONE_STATUS_LABEL = 7230;
constexpr int IDC_ZONE_DRAW_BATCH_LIST = 7231;
constexpr int IDC_ZONE_DATA_TREE = 7232;
constexpr int IDC_ZONE_RAW_DATA_TREE = 7233;
constexpr int IDC_ZONE_COLLISION_DATA_TREE = 7234;
constexpr int IDC_ZONE_COMBINE_TREE_TOGGLE = 7236;

enum class Command
{
    ShowPlaced,
    HidePlaced,
    ShowRaw,
    HideRaw,
    ToggleCombinedTree,
    ShowOnlySelected,
    HideSelected,
    ToggleHighlight,
    CenterSelected,
    ApplyTransform,
    SetCollisionVisibility
};

enum class EventType
{
    Command,
    RefreshRequested,
    TreeExpansionRequested,
    TreeSelectionChanged,
    MapObjectVisibilityChanged,
    MapObjectSelectionChanged
};

enum class Tree
{
    Objects,
    RawData,
    CollisionData
};

struct Event
{
    EventType type = EventType::Command;
    Command command = Command::ShowPlaced;
    ZoneObjectTransform::DebugTransform transform = {};
    bool collisionVisible = false;
    int mapObjectIndex = -1;
    bool mapObjectVisible = false;
    Tree tree = Tree::Objects;
    HTREEITEM treeItem = NULL;
    LPARAM treeItemData = 0;
};

using EventHandler = void (*)(void* context, const Event& event);

struct RefreshData
{
    const char* zoneLabel = "";
    const std::map<std::string, ZoneObjectTransform::DebugTransform>* overrides = nullptr;
    const std::vector<std::string>* hiddenObjectNames = nullptr;
    int collisionTriangleCount = 0;
    int modelMeshCount = 0;
    int modelMaterialCount = 0;
    int modelTextureCount = 0;
    int modelBoneCount = 0;
    bool editingEnabled = false;
};

struct State
{
    State() = default;
    State(const State&) = delete;
    State& operator=(const State&) = delete;
    State(State&&) = delete;
    State& operator=(State&&) = delete;

    HWND owner = NULL;
    HWND window = NULL;
    HWND zoneLabel = NULL;
    HWND placedListLabel = NULL;
    HWND unreferencedListLabel = NULL;
    HWND collisionListLabel = NULL;
    HWND drawBatchListLabel = NULL;
    HWND placedObjectList = NULL;
    HWND unreferencedObjectList = NULL;
    HWND collisionObjectList = NULL;
    HWND drawBatchList = NULL;
    HWND dataTree = NULL;
    HWND rawDataTree = NULL;
    HWND collisionDataTree = NULL;
    HWND loadingLabel = NULL;
    HWND placedShowAllButton = NULL;
    HWND placedHideAllButton = NULL;
    HWND unreferencedShowAllButton = NULL;
    HWND unreferencedHideAllButton = NULL;
    HWND showSelectedButton = NULL;
    HWND hideSelectedButton = NULL;
    HWND highlightSelectedButton = NULL;
    HWND centerSelectedButton = NULL;
    HWND combineTreeButton = NULL;
    HWND toolButtons[kToolButtonCount] = {};
    HWND statusLabel = NULL;
    HWND unreferencedEdits[kUnreferencedFieldCount] = {};
    HWND unreferencedLabels[kUnreferencedFieldCount] = {};
    HWND unreferencedApplyButton = NULL;
    HWND collisionVisibleCheck = NULL;

    bool populatingObjectList = false;
    int placedColumnMode = -1;
    int unreferencedColumnMode = -1;
    int collisionColumnMode = -1;
    int drawBatchColumnMode = -1;
    int treeSelectedMapObjectIndex = -1;
    bool combinedObjectTree = false;
    int paneWidths[kPaneCount] = {};
    int activeSplitter = 0;
    ZoneObjectPanelStyle::Brushes brushes;

    EventHandler eventHandler = nullptr;
    void* eventContext = nullptr;
    const char* creationZoneLabel = "";
    bool creationCollisionVisible = false;
    bool creationEditingEnabled = false;
};

void Initialize(State& state, HWND owner, EventHandler eventHandler,
                void* eventContext = nullptr);
HWND Window(const State& state) noexcept;
HWND TreeWindow(const State& state, Tree tree) noexcept;
int SelectedMapObjectIndex(const State& state) noexcept;
void SetTreeSelectedMapObjectIndex(State& state, int mapObjectIndex) noexcept;
void ToggleCombinedTree(State& state) noexcept;
void Show(State& state, const char* zoneLabel, bool collisionVisible,
          bool editingEnabled, bool activate = true);
void Hide(const State& state);
void SetEditingEnabled(State& state, bool enabled);
void SetHighlightActive(const State& state, bool active);
void SetCollisionVisible(const State& state, bool visible);
void SetZoneLabel(const State& state, const char* zoneLabel);
void PopulateTransformFields(
    State& state, int mapObjectIndex,
    const std::map<std::string, ZoneObjectTransform::DebugTransform>& overrides);
void BeginRefresh(State& state, const char* zoneLabel);
void Refresh(State& state, const RefreshData& data);
void RebuildTree(State& state, const char* zoneLabel);
void ResetControls(State& state);
void Destroy(State& state);
}
