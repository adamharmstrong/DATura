#include "stdafx.h"
#include "zone_object_panel.h"

#include "noesis_rapi.h"
#include "model_ff11.h"
#include "zone_object_panel_arrangement.h"
#include "zone_object_panel_edit_controls.h"
#include "zone_object_panel_layout.h"
#include "zone_object_panel_splitter.h"
#include "zone_object_transform_editor.h"
#include "zone_object_list_selection.h"
#include "zone_object_list_columns.h"
#include "zone_object_list_rows.h"
#include "zone_object_tree_builder.h"
#include "zone_object_tree_view.h"
#include "win32_panel_controls.h"
#include "win32_tool_window.h"

#include <commctrl.h>
#include <windowsx.h>

namespace ZoneObjectPanel
{
namespace
{
constexpr char kWindowClassName[] = "DATuraZoneObjectPanelClass";
constexpr char kWindowTitle[] = "Zone Objects";
constexpr int kWindowWidth = 1500;
constexpr int kWindowHeight = 850;

void ReleaseBrush(HBRUSH& brush)
{
    if (brush)
        DeleteObject(brush);
    brush = NULL;
}

State* StateForWindow(const HWND window)
{
    return reinterpret_cast<State*>(
        GetWindowLongPtrA(window, GWLP_USERDATA));
}

void CreateControls(State& state, const HWND window)
{
    ZoneObjectPanelStyle::EnsureBrushes(state.brushes);
    state.zoneLabel = Win32PanelControls::AddPanelControl(
        window, "STATIC", state.creationZoneLabel ? state.creationZoneLabel : "",
        0, IDC_ZONE_LABEL, 8, 8, 540, 20);
    state.placedListLabel = Win32PanelControls::AddPanelControl(
        window, "STATIC", "Placed Geometry", 0, 0, 8, 36, 320, 18);
    state.unreferencedListLabel = Win32PanelControls::AddPanelControl(
        window, "STATIC", "Unreferenced Geometry / Draw Batches", 0, 0,
        8, 260, 360, 18);
    state.collisionListLabel = Win32PanelControls::AddPanelControl(
        window, "STATIC", "Collision Meshes", 0, 0, 8, 390, 220, 18);
    state.drawBatchListLabel = Win32PanelControls::AddPanelControl(
        window, "STATIC",
        "MapGeo Draw Batches (materials, flags, vertex/index data)",
        0, 0, 8, 500, 440, 18);

    state.placedShowAllButton = Win32PanelControls::AddPanelControl(
        window, "BUTTON", "Show Placed", BS_PUSHBUTTON,
        IDC_ZONE_PLACED_SHOW_ALL, 8, 626, 90, 24);
    state.placedHideAllButton = Win32PanelControls::AddPanelControl(
        window, "BUTTON", "Hide Placed", BS_PUSHBUTTON,
        IDC_ZONE_PLACED_HIDE_ALL, 104, 626, 90, 24);
    state.unreferencedShowAllButton = Win32PanelControls::AddPanelControl(
        window, "BUTTON", "Show Raw", BS_PUSHBUTTON,
        IDC_ZONE_UNREF_SHOW_ALL, 608, 626, 90, 24);
    state.unreferencedHideAllButton = Win32PanelControls::AddPanelControl(
        window, "BUTTON", "Hide Raw", BS_PUSHBUTTON,
        IDC_ZONE_UNREF_HIDE_ALL, 704, 626, 90, 24);
    state.showSelectedButton = Win32PanelControls::AddPanelControl(
        window, "BUTTON", "Show Only Selected", BS_PUSHBUTTON,
        IDC_ZONE_SHOW_SELECTED, 8, 466, 160, 26);
    state.hideSelectedButton = Win32PanelControls::AddPanelControl(
        window, "BUTTON", "Hide Selected", BS_PUSHBUTTON,
        IDC_ZONE_HIDE_SELECTED, 174, 466, 110, 26);
    state.highlightSelectedButton = Win32PanelControls::AddPanelControl(
        window, "BUTTON", "Highlight Selected", BS_PUSHBUTTON,
        IDC_ZONE_HIGHLIGHT_SELECTED, 290, 466, 128, 26);
    state.centerSelectedButton = Win32PanelControls::AddPanelControl(
        window, "BUTTON", "Center Camera", BS_PUSHBUTTON,
        IDC_ZONE_CENTER_SELECTED, 424, 466, 112, 26);
    state.combineTreeButton = Win32PanelControls::AddPanelControl(
        window, "BUTTON", "Combine Lists", BS_PUSHBUTTON,
        IDC_ZONE_COMBINE_TREE_TOGGLE, 1200, 34, 120, 24);

    state.placedObjectList = Win32PanelControls::AddPanelControl(
        window, WC_LISTVIEWA, "", LVS_REPORT | LVS_SHOWSELALWAYS,
        IDC_ZONE_OBJECT_LIST, 8, 56, 820, 190, WS_EX_CLIENTEDGE);
    ZoneObjectPanelStyle::ApplyListColors(state.placedObjectList, true);
    state.unreferencedObjectList = Win32PanelControls::AddPanelControl(
        window, WC_LISTVIEWA, "", LVS_REPORT | LVS_SHOWSELALWAYS,
        IDC_ZONE_UNREF_OBJECT_LIST, 8, 280, 820, 178, WS_EX_CLIENTEDGE);
    ZoneObjectPanelStyle::ApplyListColors(state.unreferencedObjectList, true);
    state.collisionObjectList = Win32PanelControls::AddPanelControl(
        window, WC_LISTVIEWA, "", LVS_REPORT | LVS_SHOWSELALWAYS,
        IDC_ZONE_COLLISION_OBJECT_LIST, 8, 410, 820, 80, WS_EX_CLIENTEDGE);
    ZoneObjectPanelStyle::ApplyListColors(state.collisionObjectList, false);
    state.drawBatchList = Win32PanelControls::AddPanelControl(
        window, WC_LISTVIEWA, "", LVS_REPORT | LVS_SHOWSELALWAYS,
        IDC_ZONE_DRAW_BATCH_LIST, 8, 500, 820, 90, WS_EX_CLIENTEDGE);
    ZoneObjectPanelStyle::ApplyListColors(state.drawBatchList, false);

    const DWORD treeStyle =
        TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS | TVS_SHOWSELALWAYS;
    state.dataTree = Win32PanelControls::AddPanelControl(
        window, WC_TREEVIEWA, "", treeStyle, IDC_ZONE_DATA_TREE,
        8, 56, 820, 360, WS_EX_CLIENTEDGE);
    state.rawDataTree = Win32PanelControls::AddPanelControl(
        window, WC_TREEVIEWA, "", treeStyle, IDC_ZONE_RAW_DATA_TREE,
        8, 56, 820, 360, WS_EX_CLIENTEDGE);
    state.collisionDataTree = Win32PanelControls::AddPanelControl(
        window, WC_TREEVIEWA, "", treeStyle, IDC_ZONE_COLLISION_DATA_TREE,
        8, 56, 820, 360, WS_EX_CLIENTEDGE);
    const HWND trees[] = { state.dataTree, state.rawDataTree, state.collisionDataTree };
    for (const HWND tree : trees)
    {
        if (tree)
            SendMessageA(tree, TVM_SETUNICODEFORMAT, FALSE, 0);
        ZoneObjectPanelStyle::ApplyTreeColors(tree);
    }

    state.loadingLabel = Win32PanelControls::AddPanelControl(
        window, "STATIC", "Loading Object List...", SS_CENTER | SS_CENTERIMAGE,
        IDC_ZONE_LOADING_LABEL, 18, 58, 220, 32, WS_EX_CLIENTEDGE, false);

    const char* const fieldLabels[6] =
        { "X", "Y", "Z", "Position", "Rotation", "Scale" };
    for (int index = 0; index < 6; ++index)
    {
        state.unreferencedLabels[index] = Win32PanelControls::AddPanelControl(
            window, "STATIC", fieldLabels[index], 0, 0,
            10, 492 + index * 20, 70, 20, 0, false);
    }
    for (int index = 0; index < kUnreferencedFieldCount; ++index)
    {
        state.unreferencedEdits[index] = Win32PanelControls::AddPanelControl(
            window, "EDIT", "", ES_AUTOHSCROLL, IDC_ZONE_UNREF_X + index,
            72 + (index % 3) * 80, 516 + (index / 3) * 24, 76, 22,
            WS_EX_CLIENTEDGE, false);
    }
    state.unreferencedApplyButton = Win32PanelControls::AddPanelControl(
        window, "BUTTON", "Apply Transform", BS_PUSHBUTTON,
        IDC_ZONE_UNREF_APPLY, 510, 516, 130, 26, 0, false);

    const char* const toolLabels[kToolButtonCount] =
        { "Select Box", "Move", "Rotate", "Scale", "Transform" };
    const int toolIds[kToolButtonCount] =
    {
        IDC_ZONE_TOOL_SELECT, IDC_ZONE_TOOL_MOVE, IDC_ZONE_TOOL_ROTATE,
        IDC_ZONE_TOOL_SCALE, IDC_ZONE_TOOL_TRANSFORM
    };
    for (int index = 0; index < kToolButtonCount; ++index)
    {
        state.toolButtons[index] = Win32PanelControls::AddPanelControl(
            window, "BUTTON", toolLabels[index], BS_PUSHBUTTON, toolIds[index],
            8, 670 + index * 28, 142, 26);
    }

    state.statusLabel = Win32PanelControls::AddPanelControl(
        window, "STATIC", "", 0, IDC_ZONE_STATUS_LABEL, 168, 830, 650, 20);
    state.collisionVisibleCheck = Win32PanelControls::AddPanelControl(
        window, "BUTTON", "Show collision mesh", BS_AUTOCHECKBOX,
        IDC_ZONE_COLLISION_VISIBLE, 10, 540, 420, 24, 0, false);
    SendMessageA(state.collisionVisibleCheck, BM_SETCHECK,
        state.creationCollisionVisible ? BST_CHECKED : BST_UNCHECKED, 0);
}

void Arrange(State& state, const int clientWidth, const int clientHeight)
{
    ZoneObjectPanelArrangement::Context context = {};
    ZoneObjectPanelArrangement::Controls& controls = context.controls;
    controls.zoneLabel = state.zoneLabel;
    controls.combineTreeButton = state.combineTreeButton;
    controls.placedListLabel = state.placedListLabel;
    controls.unreferencedListLabel = state.unreferencedListLabel;
    controls.collisionListLabel = state.collisionListLabel;
    controls.drawBatchListLabel = state.drawBatchListLabel;
    controls.placedObjectList = state.placedObjectList;
    controls.unreferencedObjectList = state.unreferencedObjectList;
    controls.collisionObjectList = state.collisionObjectList;
    controls.drawBatchList = state.drawBatchList;
    controls.dataTree = state.dataTree;
    controls.rawDataTree = state.rawDataTree;
    controls.collisionDataTree = state.collisionDataTree;
    controls.loadingLabel = state.loadingLabel;
    controls.placedShowAllButton = state.placedShowAllButton;
    controls.placedHideAllButton = state.placedHideAllButton;
    controls.unreferencedShowAllButton = state.unreferencedShowAllButton;
    controls.unreferencedHideAllButton = state.unreferencedHideAllButton;
    controls.showSelectedButton = state.showSelectedButton;
    controls.hideSelectedButton = state.hideSelectedButton;
    controls.highlightSelectedButton = state.highlightSelectedButton;
    controls.centerSelectedButton = state.centerSelectedButton;
    controls.toolButtons = state.toolButtons;
    controls.toolButtonCount = kToolButtonCount;
    controls.unreferencedLabels = state.unreferencedLabels;
    controls.unreferencedLabelCount = kUnreferencedFieldCount;
    controls.unreferencedEdits = state.unreferencedEdits;
    controls.unreferencedEditCount = kUnreferencedFieldCount;
    controls.unreferencedApplyButton = state.unreferencedApplyButton;
    controls.collisionVisibleCheck = state.collisionVisibleCheck;
    controls.statusLabel = state.statusLabel;
    context.paneWidths = state.paneWidths;
    context.combinedObjectTree = state.combinedObjectTree;
    ZoneObjectPanelArrangement::Arrange(context, clientWidth, clientHeight);
}

void Emit(State& state, const Event& event)
{
    if (state.eventHandler)
        state.eventHandler(state.eventContext, event);
}

bool TryGetTree(const UINT controlId, Tree* const tree)
{
    if (!tree)
        return false;
    switch (controlId)
    {
    case IDC_ZONE_DATA_TREE: *tree = Tree::Objects; return true;
    case IDC_ZONE_RAW_DATA_TREE: *tree = Tree::RawData; return true;
    case IDC_ZONE_COLLISION_DATA_TREE: *tree = Tree::CollisionData; return true;
    }
    return false;
}

void EmitCommand(State& state, const Command command)
{
    Event event = {};
    event.type = EventType::Command;
    event.command = command;
    if (command == Command::ApplyTransform)
    {
        event.transform = ZoneObjectTransformEditor::ReadFields(
            state.unreferencedEdits, kUnreferencedFieldCount);
    }
    else if (command == Command::SetCollisionVisibility)
    {
        event.collisionVisible = state.collisionVisibleCheck &&
            SendMessageA(state.collisionVisibleCheck, BM_GETCHECK, 0, 0) == BST_CHECKED;
    }
    Emit(state, event);
}

bool HandleNotification(State& state, const LPARAM lParam)
{
    if (state.populatingObjectList)
        return false;

    const LPNMHDR header = reinterpret_cast<LPNMHDR>(lParam);
    if (!header)
        return false;

    Tree treeKind = Tree::Objects;
    if (TryGetTree(static_cast<UINT>(header->idFrom), &treeKind))
    {
        const LPNMTREEVIEWA tree = reinterpret_cast<LPNMTREEVIEWA>(lParam);
        if (header->code == TVN_ITEMEXPANDINGA && tree->action == TVE_EXPAND)
        {
            Event event = {};
            event.type = EventType::TreeExpansionRequested;
            event.tree = treeKind;
            event.treeItem = tree->itemNew.hItem;
            event.treeItemData = tree->itemNew.lParam;
            Emit(state, event);
            return true;
        }
        if (header->code == TVN_SELCHANGEDA)
        {
            Event event = {};
            event.type = EventType::TreeSelectionChanged;
            event.tree = treeKind;
            event.mapObjectIndex =
                ZoneObjectTreeView::GetZoneTreeParamType(tree->itemNew.lParam) ==
                        ZoneObjectTreeView::kZoneTreeNode_MapObject
                    ? ZoneObjectTreeView::GetZoneTreeParamIndex(tree->itemNew.lParam)
                    : -1;
            Emit(state, event);
            return true;
        }
        return false;
    }

    if ((header->idFrom != IDC_ZONE_OBJECT_LIST &&
         header->idFrom != IDC_ZONE_UNREF_OBJECT_LIST) ||
        header->code != LVN_ITEMCHANGED)
    {
        return false;
    }

    const LPNMLISTVIEW listView = reinterpret_cast<LPNMLISTVIEW>(lParam);
    if ((listView->uChanged & LVIF_STATE) == 0)
        return false;

    const HWND list = header->idFrom == IDC_ZONE_OBJECT_LIST
        ? state.placedObjectList : state.unreferencedObjectList;
    const UINT oldCheckState = listView->uOldState & LVIS_STATEIMAGEMASK;
    const UINT newCheckState = listView->uNewState & LVIS_STATEIMAGEMASK;
    bool handled = false;
    if (oldCheckState != newCheckState)
    {
        Event event = {};
        event.type = EventType::MapObjectVisibilityChanged;
        event.mapObjectIndex = ZoneObjectListSelection::GetMapObjectIndex(
            list, listView->iItem);
        event.mapObjectVisible = ListView_GetCheckState(list, listView->iItem) != FALSE;
        Emit(state, event);
        handled = true;
    }
    if ((listView->uNewState & LVIS_SELECTED) != 0)
    {
        ZoneObjectListSelection::ClearOtherMapObjectListSelection(
            list, state.placedObjectList, state.unreferencedObjectList);
        Event event = {};
        event.type = EventType::MapObjectSelectionChanged;
        Emit(state, event);
        handled = true;
    }
    return handled;
}

LRESULT CALLBACK WindowProcedure(const HWND window, const UINT message,
                                 const WPARAM wParam, const LPARAM lParam)
{
    State* state = StateForWindow(window);
    if (message == WM_NCCREATE)
    {
        const CREATESTRUCTA* create =
            reinterpret_cast<const CREATESTRUCTA*>(lParam);
        state = static_cast<State*>(create->lpCreateParams);
        SetWindowLongPtrA(window, GWLP_USERDATA,
                          reinterpret_cast<LONG_PTR>(state));
        if (state)
            state->window = window;
    }

    switch (message)
    {
    case WM_CREATE:
        if (!state)
            return -1;
        CreateControls(*state, window);
        SetEditingEnabled(*state, state->creationEditingEnabled);
        return 0;

    case WM_SIZE:
        if (state)
            Arrange(*state, LOWORD(lParam), HIWORD(lParam));
        return 0;

    case WM_SETCURSOR:
        if (state && LOWORD(lParam) == HTCLIENT && !state->combinedObjectTree &&
            ZoneObjectPanelSplitter::SetResizeCursorIfOverSplitter(
                window, state->paneWidths))
        {
            return TRUE;
        }
        break;

    case WM_LBUTTONDOWN:
        if (state && !state->combinedObjectTree &&
            ZoneObjectPanelSplitter::BeginResizeDrag(
                window, state->paneWidths, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam),
                state->activeSplitter))
        {
            return 0;
        }
        break;

    case WM_MOUSEMOVE:
        if (state && state->activeSplitter)
        {
            int clientWidth = 0;
            int clientHeight = 0;
            if (ZoneObjectPanelSplitter::ResizeActiveDrag(
                    window, state->paneWidths, state->activeSplitter,
                    GET_X_LPARAM(lParam), &clientWidth, &clientHeight))
            {
                SendMessageA(window, WM_SIZE, 0,
                             MAKELPARAM(clientWidth, clientHeight));
                InvalidateRect(window, NULL, TRUE);
                return 0;
            }
        }
        break;

    case WM_LBUTTONUP:
        if (state && ZoneObjectPanelSplitter::EndResizeDrag(state->activeSplitter))
            return 0;
        break;

    case WM_CTLCOLORSTATIC:
        if (state)
        {
            ZoneObjectPanelStyle::EnsureBrushes(state->brushes);
            HDC hdc = reinterpret_cast<HDC>(wParam);
            SetTextColor(hdc, ZoneObjectPanelStyle::TextColor());
            SetBkColor(hdc, ZoneObjectPanelStyle::PanelColor());
            return reinterpret_cast<LRESULT>(state->brushes.panel);
        }
        break;

    case WM_CTLCOLOREDIT:
        if (state)
        {
            ZoneObjectPanelStyle::EnsureBrushes(state->brushes);
            HDC hdc = reinterpret_cast<HDC>(wParam);
            SetTextColor(hdc, ZoneObjectPanelStyle::TextColor());
            SetBkColor(hdc, ZoneObjectPanelStyle::EditColor());
            return reinterpret_cast<LRESULT>(state->brushes.edit);
        }
        break;

    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLORBTN:
        if (state)
        {
            ZoneObjectPanelStyle::EnsureBrushes(state->brushes);
            HDC hdc = reinterpret_cast<HDC>(wParam);
            SetTextColor(hdc, ZoneObjectPanelStyle::TextColor());
            SetBkColor(hdc, ZoneObjectPanelStyle::ControlColor());
            return reinterpret_cast<LRESULT>(state->brushes.control);
        }
        break;

    case WM_ERASEBKGND:
        if (state)
        {
            ZoneObjectPanelStyle::EnsureBrushes(state->brushes);
            RECT client = {};
            GetClientRect(window, &client);
            HDC hdc = reinterpret_cast<HDC>(wParam);
            FillRect(hdc, &client, state->brushes.panel);
            if (!state->combinedObjectTree)
            {
                int placedX = 0;
                int placedWidth = 0;
                int rawX = 0;
                int rawWidth = 0;
                int collisionX = 0;
                int collisionWidth = 0;
                ZoneObjectPanelLayout::ComputeZonePaneLayout(
                    client.right - client.left, state->paneWidths,
                    &placedX, &placedWidth, &rawX, &rawWidth,
                    &collisionX, &collisionWidth);
                HBRUSH splitterBrush = CreateSolidBrush(RGB(88, 94, 98));
                RECT first = { placedX + placedWidth + 4, 56,
                               placedX + placedWidth + 6, client.bottom - 230 };
                RECT second = { rawX + rawWidth + 4, 56,
                                rawX + rawWidth + 6, client.bottom - 230 };
                FillRect(hdc, &first, splitterBrush);
                FillRect(hdc, &second, splitterBrush);
                DeleteObject(splitterBrush);
            }
            return 1;
        }
        break;

    case WM_CLOSE:
        ShowWindow(window, SW_HIDE);
        return 0;

    case WM_COMMAND:
        if (state)
        {
            switch (LOWORD(wParam))
            {
            case IDC_ZONE_PLACED_SHOW_ALL: EmitCommand(*state, Command::ShowPlaced); return 0;
            case IDC_ZONE_PLACED_HIDE_ALL: EmitCommand(*state, Command::HidePlaced); return 0;
            case IDC_ZONE_UNREF_SHOW_ALL: EmitCommand(*state, Command::ShowRaw); return 0;
            case IDC_ZONE_UNREF_HIDE_ALL: EmitCommand(*state, Command::HideRaw); return 0;
            case IDC_ZONE_COMBINE_TREE_TOGGLE: EmitCommand(*state, Command::ToggleCombinedTree); return 0;
            case IDC_ZONE_SHOW_SELECTED: EmitCommand(*state, Command::ShowOnlySelected); return 0;
            case IDC_ZONE_HIDE_SELECTED: EmitCommand(*state, Command::HideSelected); return 0;
            case IDC_ZONE_HIGHLIGHT_SELECTED: EmitCommand(*state, Command::ToggleHighlight); return 0;
            case IDC_ZONE_CENTER_SELECTED: EmitCommand(*state, Command::CenterSelected); return 0;
            case IDC_ZONE_UNREF_APPLY: EmitCommand(*state, Command::ApplyTransform); return 0;
            case IDC_ZONE_COLLISION_VISIBLE: EmitCommand(*state, Command::SetCollisionVisibility); return 0;
            }
        }
        break;

    case kPopulateMessage:
        if (state)
        {
            Event event = {};
            event.type = EventType::RefreshRequested;
            Emit(*state, event);
            return 0;
        }
        break;

    case WM_NOTIFY:
        if (state && HandleNotification(*state, lParam))
            return 0;
        break;

    case WM_DESTROY:
        if (state && state->window == window)
            ResetControls(*state);
        SetWindowLongPtrA(window, GWLP_USERDATA, 0);
        return 0;
    }

    return DefWindowProcA(window, message, wParam, lParam);
}
}

void Initialize(State& state, const HWND owner,
                const EventHandler eventHandler, void* const eventContext)
{
    state.owner = owner;
    state.eventHandler = eventHandler;
    state.eventContext = eventContext;
}

HWND Window(const State& state) noexcept
{
    return state.window;
}

HWND TreeWindow(const State& state, const Tree tree) noexcept
{
    switch (tree)
    {
    case Tree::Objects: return state.dataTree;
    case Tree::RawData: return state.rawDataTree;
    case Tree::CollisionData: return state.collisionDataTree;
    }
    return NULL;
}

int SelectedMapObjectIndex(const State& state) noexcept
{
    return ZoneObjectListSelection::GetSelectedMapObjectIndex(
        state.placedObjectList, state.unreferencedObjectList,
        state.treeSelectedMapObjectIndex);
}

void SetTreeSelectedMapObjectIndex(
    State& state, const int mapObjectIndex) noexcept
{
    state.treeSelectedMapObjectIndex = mapObjectIndex;
}

void ToggleCombinedTree(State& state) noexcept
{
    state.combinedObjectTree = !state.combinedObjectTree;
}

void Show(State& state, const char* const zoneLabel,
          const bool collisionVisible, const bool editingEnabled,
          const bool activate)
{
    if (state.window)
    {
        Win32ToolWindow::Show(state.window, activate);
        return;
    }

    state.creationZoneLabel = zoneLabel ? zoneLabel : "";
    state.creationCollisionVisible = collisionVisible;
    state.creationEditingEnabled = editingEnabled;
    ZoneObjectPanelStyle::EnsureBrushes(state.brushes);
    Win32ToolWindow::Spec spec =
    {
        WindowProcedure, kWindowClassName, kWindowTitle, kWindowWidth, kWindowHeight,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME,
        state.brushes.panel
    };
    spec.createParameter = &state;
    state.window = Win32ToolWindow::Create(state.owner, spec);
    state.creationZoneLabel = "";
    if (state.window)
    {
        Win32ToolWindow::Show(state.window, false);
        UpdateWindow(state.window);
    }
}

void Hide(const State& state)
{
    if (state.window)
        ShowWindow(state.window, SW_HIDE);
}

void SetEditingEnabled(State& state, const bool enabled)
{
    HWND editControls[] =
    {
        state.placedShowAllButton,
        state.placedHideAllButton,
        state.unreferencedShowAllButton,
        state.unreferencedHideAllButton,
        state.showSelectedButton,
        state.hideSelectedButton,
        state.highlightSelectedButton,
        state.centerSelectedButton,
        state.combineTreeButton,
        state.unreferencedApplyButton,
        state.collisionVisibleCheck
    };
    ZoneObjectPanelEditControls::SetControlsEnabled(
        editControls, static_cast<int>(sizeof(editControls) / sizeof(editControls[0])),
        enabled);
    ZoneObjectPanelEditControls::SetControlsEnabled(
        state.toolButtons, kToolButtonCount, enabled);
    ZoneObjectPanelEditControls::SetControlsEnabled(
        state.unreferencedEdits, kUnreferencedFieldCount, enabled);
}

void SetHighlightActive(const State& state, const bool active)
{
    if (state.highlightSelectedButton)
    {
        SetWindowTextA(state.highlightSelectedButton,
            active ? "Clear Highlight" : "Highlight Selected");
    }
}

void SetCollisionVisible(const State& state, const bool visible)
{
    if (state.collisionVisibleCheck)
    {
        SendMessageA(state.collisionVisibleCheck, BM_SETCHECK,
            visible ? BST_CHECKED : BST_UNCHECKED, 0);
    }
}

void SetZoneLabel(const State& state, const char* const zoneLabel)
{
    if (state.zoneLabel)
        SetWindowTextA(state.zoneLabel, zoneLabel ? zoneLabel : "");
}

void PopulateTransformFields(
    State& state, const int mapObjectIndex,
    const std::map<std::string, ZoneObjectTransform::DebugTransform>& overrides)
{
    ZoneObjectTransformEditor::PopulateMapObjectFields(
        state.unreferencedEdits, kUnreferencedFieldCount,
        mapObjectIndex, overrides);
}

void RebuildTree(State& state, const char* const zoneLabel)
{
    ZoneObjectTreeBuilder::Context context = {};
    context.placedObjectList = state.placedObjectList;
    context.unreferencedObjectList = state.unreferencedObjectList;
    context.dataTree = state.dataTree;
    context.rawDataTree = state.rawDataTree;
    context.collisionDataTree = state.collisionDataTree;
    context.combinedObjectTree = state.combinedObjectTree;
    context.loadedZoneLabel = zoneLabel ? zoneLabel : "";
    ZoneObjectTreeBuilder::Build(context);
}

void BeginRefresh(State& state, const char* const zoneLabel)
{
    if (!state.window || !state.placedObjectList || !state.unreferencedObjectList ||
        !state.collisionObjectList || !state.drawBatchList || !state.dataTree ||
        !state.rawDataTree || !state.collisionDataTree)
    {
        return;
    }

    if (state.zoneLabel)
        SetWindowTextA(state.zoneLabel, zoneLabel ? zoneLabel : "");
    ZoneObjectListColumns::SetupMapObjectColumns(
        state.placedObjectList, state.placedColumnMode, 0);
    ZoneObjectListColumns::SetupMapObjectColumns(
        state.unreferencedObjectList, state.unreferencedColumnMode, 1);
    ZoneObjectListColumns::SetupCollisionColumns(
        state.collisionObjectList, state.collisionColumnMode, 0);
    ZoneObjectListColumns::SetupDrawBatchColumns(
        state.drawBatchList, state.drawBatchColumnMode, 0);
    ListView_DeleteAllItems(state.placedObjectList);
    ListView_DeleteAllItems(state.unreferencedObjectList);
    ListView_DeleteAllItems(state.collisionObjectList);
    ListView_DeleteAllItems(state.drawBatchList);
    TreeView_DeleteAllItems(state.dataTree);
    TreeView_DeleteAllItems(state.rawDataTree);
    TreeView_DeleteAllItems(state.collisionDataTree);
    if (state.loadingLabel)
    {
        SetWindowTextA(state.loadingLabel, "Loading Object List...");
        ShowWindow(state.loadingLabel, SW_SHOW);
        BringWindowToTop(state.loadingLabel);
    }
    UpdateWindow(state.window);
    PostMessageA(state.window, kPopulateMessage, 0, 0);
}

void Refresh(State& state, const RefreshData& data)
{
    if (!state.placedObjectList || !state.unreferencedObjectList ||
        !state.collisionObjectList || !state.drawBatchList || !state.dataTree ||
        !state.rawDataTree || !state.collisionDataTree || !data.overrides ||
        !data.hiddenObjectNames)
    {
        return;
    }

    const HWND redrawControls[] =
    {
        state.placedObjectList, state.unreferencedObjectList,
        state.collisionObjectList, state.drawBatchList, state.dataTree,
        state.rawDataTree, state.collisionDataTree
    };
    state.populatingObjectList = true;
    state.treeSelectedMapObjectIndex = -1;
    for (const HWND control : redrawControls)
        SendMessageA(control, WM_SETREDRAW, FALSE, 0);

    ListView_DeleteAllItems(state.placedObjectList);
    ListView_DeleteAllItems(state.unreferencedObjectList);
    ListView_DeleteAllItems(state.collisionObjectList);
    ListView_DeleteAllItems(state.drawBatchList);
    ZoneObjectListColumns::SetupMapObjectColumns(
        state.placedObjectList, state.placedColumnMode, 0);
    ZoneObjectListColumns::SetupMapObjectColumns(
        state.unreferencedObjectList, state.unreferencedColumnMode, 1);
    ZoneObjectListColumns::SetupCollisionColumns(
        state.collisionObjectList, state.collisionColumnMode, 0);
    ZoneObjectListColumns::SetupDrawBatchColumns(
        state.drawBatchList, state.drawBatchColumnMode, 0);
    ZoneObjectPanelStyle::ApplyListColors(state.placedObjectList, true);
    ZoneObjectPanelStyle::ApplyListColors(state.unreferencedObjectList, true);
    ZoneObjectPanelStyle::ApplyListColors(state.collisionObjectList, false);
    ZoneObjectPanelStyle::ApplyListColors(state.drawBatchList, false);
    ZoneObjectPanelStyle::ApplyTreeColors(state.dataTree);
    ZoneObjectPanelStyle::ApplyTreeColors(state.rawDataTree);
    ZoneObjectPanelStyle::ApplyTreeColors(state.collisionDataTree);
    SetZoneLabel(state, data.zoneLabel);

    const int mapObjectCount = Model_FF11_GetLastMapObjectCount();
    for (int index = 0; index < mapObjectCount; ++index)
    {
        const char* displayName = Model_FF11_GetLastMapObjectDisplayName(index);
        const bool unreferenced = displayName && strncmp(displayName, "env:", 4) == 0;
        ZoneObjectListRows::InsertMapObjectRow(
            unreferenced ? state.unreferencedObjectList : state.placedObjectList,
            index, *data.overrides, *data.hiddenObjectNames);
    }
    const int collisionMeshCount = Model_FF11_GetLastCollisionMeshCount();
    for (int index = 0; index < collisionMeshCount; ++index)
        ZoneObjectListRows::InsertCollisionRow(state.collisionObjectList, index);
    const int drawBatchCount = Model_FF11_GetLastMapGeoDrawBatchCount();
    for (int index = 0; index < drawBatchCount; ++index)
        ZoneObjectListRows::InsertDrawBatchRow(state.drawBatchList, index);
    RebuildTree(state, data.zoneLabel);

    state.populatingObjectList = false;
    ZoneObjectPanelEditControls::SetUnreferencedEditorVisible(
        state.unreferencedLabels, state.unreferencedEdits,
        kUnreferencedFieldCount, state.unreferencedApplyButton, true);
    if (state.collisionVisibleCheck)
    {
        char collisionText[128] = {};
        sprintf_s(collisionText, "Show collision mesh (%d triangles)",
                  data.collisionTriangleCount);
        SetWindowTextA(state.collisionVisibleCheck, collisionText);
        ShowWindow(state.collisionVisibleCheck, SW_SHOW);
    }
    const HWND visibilityButtons[] =
    {
        state.placedShowAllButton, state.placedHideAllButton,
        state.unreferencedShowAllButton, state.unreferencedHideAllButton
    };
    for (const HWND button : visibilityButtons)
    {
        if (button)
            ShowWindow(button, SW_SHOW);
    }
    SetEditingEnabled(state, data.editingEnabled);

    if (state.statusLabel)
    {
        char statusText[512] = {};
        sprintf_s(statusText,
            "Map records: %d     Unreferenced MapGeo: %d     Draw batches: %d     Collision grid entries: %d     Meshes: %d     Textures: %d     Materials: %d     Bones: %d",
            ListView_GetItemCount(state.placedObjectList),
            ListView_GetItemCount(state.unreferencedObjectList), drawBatchCount,
            ListView_GetItemCount(state.collisionObjectList), data.modelMeshCount,
            data.modelTextureCount, data.modelMaterialCount, data.modelBoneCount);
        SetWindowTextA(state.statusLabel, statusText);
    }
    const int selectedMapObjectIndex = ZoneObjectListSelection::GetSelectedMapObjectIndex(
        state.placedObjectList, state.unreferencedObjectList,
        state.treeSelectedMapObjectIndex);
    PopulateTransformFields(state, selectedMapObjectIndex, *data.overrides);
    for (const HWND control : redrawControls)
    {
        SendMessageA(control, WM_SETREDRAW, TRUE, 0);
        InvalidateRect(control, NULL, TRUE);
    }
    if (state.loadingLabel)
        ShowWindow(state.loadingLabel, SW_HIDE);
}

void ResetControls(State& state)
{
    state.window = NULL;
    state.zoneLabel = NULL;
    state.placedListLabel = NULL;
    state.unreferencedListLabel = NULL;
    state.collisionListLabel = NULL;
    state.drawBatchListLabel = NULL;
    state.placedObjectList = NULL;
    state.unreferencedObjectList = NULL;
    state.collisionObjectList = NULL;
    state.drawBatchList = NULL;
    state.dataTree = NULL;
    state.rawDataTree = NULL;
    state.collisionDataTree = NULL;
    state.loadingLabel = NULL;
    state.placedShowAllButton = NULL;
    state.placedHideAllButton = NULL;
    state.unreferencedShowAllButton = NULL;
    state.unreferencedHideAllButton = NULL;
    state.showSelectedButton = NULL;
    state.hideSelectedButton = NULL;
    state.highlightSelectedButton = NULL;
    state.centerSelectedButton = NULL;
    state.combineTreeButton = NULL;
    for (HWND& button : state.toolButtons)
        button = NULL;
    state.statusLabel = NULL;
    for (HWND& edit : state.unreferencedEdits)
        edit = NULL;
    for (HWND& label : state.unreferencedLabels)
        label = NULL;
    state.unreferencedApplyButton = NULL;
    state.collisionVisibleCheck = NULL;

    state.populatingObjectList = false;
    state.placedColumnMode = -1;
    state.unreferencedColumnMode = -1;
    state.collisionColumnMode = -1;
    state.drawBatchColumnMode = -1;
    state.treeSelectedMapObjectIndex = -1;
    state.combinedObjectTree = false;
    for (int& paneWidth : state.paneWidths)
        paneWidth = 0;
    state.activeSplitter = 0;
}

void Destroy(State& state)
{
    if (state.window && IsWindow(state.window))
        DestroyWindow(state.window);
    ResetControls(state);
    state.owner = NULL;
    state.eventHandler = nullptr;
    state.eventContext = nullptr;
    state.creationZoneLabel = "";
    state.creationCollisionVisible = false;
    state.creationEditingEnabled = false;
    ReleaseBrush(state.brushes.panel);
    ReleaseBrush(state.brushes.control);
    ReleaseBrush(state.brushes.edit);
}
}
