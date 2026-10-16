# `main.cpp` Refactoring Plan

**Date:** September 28, 2026  
**Scope:** Reduce coupling and clarify ownership in `DATura/main.cpp`  
**Status:** Active implementation roadmap; completed slices are recorded below.

## 1. Purpose

`main.cpp` has grown to roughly 8,000–8,700 source lines as title-screen, character creation, saved-character, NPC dialogue, debug-console, zone, and rendering features have been added. The repository already has many useful subsystem modules, but `main.cpp` still owns their coordination, much of their application state, and several complete workflows.

This plan identifies practical extractions that improve ownership and testability. The goal is not to move code solely to make the file shorter. Each extraction should leave the destination module with a clear responsibility, explicit state, and a narrow interface to the application shell.

This plan supplements the earlier [`DATURA_REFACTORING_REMAINDER_PLAN_2026-08-12.md`](DATURA_REFACTORING_REMAINDER_PLAN_2026-08-12.md). That document's line-count snapshot predates substantial feature growth and should be treated as historical rather than as the current `main.cpp` baseline.

## 2. Current structure and constraints

The file currently contains several distinct kinds of code:

- process startup, Win32 window-message routing, and frame orchestration;
- application settings persistence and side effects;
- title camera schedule parsing, camera state, and title-screen transitions;
- character creation, starting-job and nation selection, and saved-character selection/deletion;
- NPC event bytecode execution and dialogue presentation;
- zone loading, player-zone transitions, collision setup, and scene ownership;
- zone-object editing coordination;
- frame rendering and overlay dispatch; and
- companion model-viewer command handling.

Several existing modules already provide good patterns to extend: `ApplicationSettings::State`, `InteractionController::State`, `PlayerController::State`, `ConfigDialog::State`, typed panel commands, title and nation renderers, `SceneLoadContext`, and typed loader results.

The main constraint is the concentration of `static` application state in `main.cpp`. Moving functions while leaving all their dependencies as globals or adding one large `ApplicationState` parameter would relocate the coupling without resolving it. Each phase must move the state owned by that subsystem along with its behavior.

## 3. Goals and non-goals

### Goals

- Make subsystem state and resource lifetimes explicit.
- Keep `main.cpp` as the composition root for cross-system actions.
- Reuse typed commands, event callbacks, and request/result structures already used elsewhere in the project.
- Keep each phase buildable and reviewable on its own.
- Make pure policy and data-processing logic independently testable where it is practical.

### Non-goals

- Redesign game behavior or UI while moving code.
- Split stable generated tables or parser implementations just because they are large.
- Hide every global behind a generic service locator or one application-wide god object.
- Force the frame loop or Win32 message pump out of the application shell before their dependencies are understood.

## 4. Target ownership model

The application shell should own the top-level subsystem instances and forward events. A subsystem should own its own mutable state and expose operations in terms of domain requests and typed results.

For example, the shell may ask a character-flow controller to confirm a selected job. The controller can return a typed request to load nation selection, while the shell performs the actual scene load through its scene owner. This keeps the controller independent of graphics globals while retaining application-level control over scene changes.

Use narrow context structures for dependencies that cannot be expressed as data. Prefer callback tables or event handlers for outward actions. Avoid passing `main.cpp`'s whole global namespace, raw `WPARAM`/`LPARAM`, or unrelated subsystem states into a module.

## 5. Phased implementation plan

Line estimates below are rough ranges, may overlap, and are not commitments. Count extracted ownership and reduced coupling as the success measure.

### Phase 0 — Establish a current baseline and extraction map

**Work**

1. Record the current line and function counts for `main.cpp`.
2. Inventory its file-scope state and identify which subsystem reads or mutates each item.
3. Mark existing modules that already own part of the target behavior.
4. For each planned slice, record affected entry points, externally visible behavior, and resource-lifetime assumptions.

**Deliverable**

A short slice checklist attached to the implementation review or a follow-up note. Do not combine this inventory with broad formatting or unrelated cleanup.

**Baseline recorded September 29, 2026**

- `DATura/main.cpp`: 8,833 physical lines before the first extraction.
- `static` function declarations/definition starts: 313 matches for
  `^static .*\(`; this is a repeatable search heuristic, not a count of unique
  functions because signatures and declarations can span lines or overload.
- First extraction boundary: title-camera schedule parsing, retail spline
  sampling, fallback rail data/state, and elapsed rail time move to
  `title_camera_rail.*`. The application shell retains schedule-name mapping,
  zone loading, and applying a sampled pose to the active camera.
- Remaining Phase 0 work: finish the file-scope state ownership inventory for
  the other planned subsystem slices before expanding their boundaries.

### Phase 1 — Extract title-camera path state and sampling

**Source area**

Title camera schedule parsing, easing, sampling, and the custom camera rail (currently around the early title-camera helpers and title-screen loader).

**Target**

`title_camera_rail.h/.cpp` or a similarly named title-camera module.

**Module ownership**

- retail schedule key data and schedule name selection;
- rail clock and sampled camera state;
- interpolation/easing helpers; and
- fallback behavior for missing or incomplete schedule data.

**Boundary**

Accept the selected title-background identity and the needed schedule bytes/path source. Return a camera sample (eye/target or target/yaw/pitch/distance) and a load/sample status. `main.cpp` remains responsible for loading the zone and applying the sample to the active camera.

**Risk and checks**

Camera path parsing and fallback selection are sensitive to coordinate conventions and timing. Compare the opening sample, intermediate samples, loop boundary, and fallback against current behavior. Keep the Konschtat custom rail intact.

**Expected extraction:** approximately 300–450 lines.

**Implementation status (September 29, 2026):** extracted into
`DATura/title_camera_rail.h/.cpp`. The controller owns retail camera DAT
parsing, scheduler interpretation, camera spline/easing, the custom Konschtat
fallback rail, and its clock/base pose. `main.cpp` supplies the install path,
schedule identity, and coordinate-mirroring choice, and applies returned
camera poses. Debug x64 build succeeded. Runtime comparison at opening,
intermediate samples, loop boundary, and fallback remains a manual check.

### Phase 2 — Separate application preference persistence from side effects

**Source area**

Registry read/write helpers and initialization/save routines for title background/music/UI, custom texture settings, extended graphics settings, and key bindings.

**Target**

Extend `application_settings.*` or add `application_settings_store.h/.cpp` for persistence only.

**Module ownership**

- registry value names and serialization;
- defaulting, clamping, and migration of saved values; and
- round-trip of the `ApplicationSettings::State` fields.

**Boundary**

Persistence accepts/returns settings data. Device reset, zone reload, audio restart, UI refresh, dialogs, and other runtime side effects remain in `main.cpp` and are driven by typed config events.

**Risk and checks**

Preserve existing registry names and stored values. Verify missing values, out-of-range values, and save/reopen behavior. Do not change default choices as part of this extraction.

**Expected extraction:** approximately 150–300 lines.

**Implementation status (September 29, 2026):** added
`DATura/application_settings_store.h/.cpp`. Registry value names, typed DWORD
and string reads/writes, defaults, clamping, and serialization for custom
textures, extended graphics/shadows, key bindings, and title preferences now
belong to the store. `main.cpp` retains path/theme/runtime coordination and
calls the store at the same load and save points as before. The store exposes
the shared application registry key to the path and theme owners. Debug x64
build succeeded; registry round-trip behavior still needs a manual check with
missing and out-of-range values. `main.cpp` is now 8,222 physical lines.

### Phase 3 — Extract NPC event execution and conversation state

**Source area**

`NpcEventRuntime`, event operand/work helpers, bytecode stepping, conversation frame splitting, choice advancement, and retail dialogue lookup (currently roughly the mid-file NPC event/conversation block).

**Targets**

- `npc_event_runtime.h/.cpp` for the event interpreter and its runtime state;
- optionally `npc_conversation_controller.h/.cpp` for the conversation lifecycle if the first extraction leaves a coherent boundary.

**Module ownership**

- event program counter and local work state;
- event stepping and message/choice/done/blocked outcomes;
- conversation frame/choice state; and
- helpers that map event data to dialogue text.

**Boundary**

The interpreter returns typed steps and never displays Win32 dialogs itself. The conversation controller can raise events such as `ShowMessage`, `ShowChoices`, `Close`, or `RequestGameAction`. `main.cpp` or the existing chat window handles presentation and world mutations.

**Risk and checks**

This code can affect real event progression and game state. Preserve opcode behavior exactly. Add focused tests for operand decoding and control flow where feasible, then manually verify a message, a multi-frame dialogue, a choice, cancel/back, and a blocked or unmapped event.

**Expected extraction:** approximately 450–650 lines.

**Implementation status (September 29, 2026; first sub-slice):** extracted the
event interpreter, local/zone work arrays, jump stack, program counter, and
pending message/choice state into `DATura/npc_event_runtime.h/.cpp`. The module
returns typed outcomes and does not present UI or call Win32 diagnostics;
`main.cpp` adapts unsupported-opcode results to the existing debug message and
continues to own event/message table loading and chat presentation. Debug x64
build succeeded. A second sub-slice added
`DATura/npc_conversation_controller.h/.cpp`: it now owns fallback frame
splitting/progression and turns retail message/choice interpreter outcomes into
typed presentation data. `main.cpp` retains DAT lookup/loading, Win32 chat
presentation, and the shared Home Point choice/action flow. Debug x64 build
succeeded. Manual checks for a message, multiple frames, a choice, cancel/back,
and a blocked or unmapped event remain outstanding. `main.cpp` is now 7,911
physical lines.
An eighth Phase 4 sub-slice moved saved-character candidate enumeration and
DAT-set/metadata path derivation into `CharacterRosterController`. `main.cpp`
still loads each candidate's DAT set and owns the resulting preview asset
lifetime. Debug x64 build succeeded; `main.cpp` is now 7,916 physical lines.

### Phase 4 — Move character creation and lobby flow ownership

**Source area**

Creation panel synchronization, save preparation, saved-character scan/select/delete, starting-job selection, nation selection, and the associated navigation state. Existing rendering and panel modules should remain separate.

**Targets**

- `character_creation_flow.h/.cpp` for the creation → starting job → nation sequence;
- `character_roster_controller.h/.cpp` for saved-character listing, confirmation, and deletion; or a single `character_lobby_controller` if their shared state remains small and clearly owned.

**Module ownership**

- current creation stage and selected starting job/nation;
- loaded roster entries and selected row;
- navigation/back/confirm transitions; and
- character metadata preparation and save-path policy.

**Boundary**

Keep model loading, Direct3D assets, active-zone loading, player spawning, and scene transitions behind host operations or callbacks. Reuse `CharacterSaveData`, `CreationModelLoader`, `HighPolyCreationPanel`, `FFXINationSelectionRenderer`, and `CharacterRoster` drawing helpers rather than merging UI rendering into the controller.

**Risk and checks**

This is the most coupled user-facing flow. Preserve the order of screens and Back behavior. Verify that all six starting jobs remain the only offered jobs, that the chosen job and home nation are saved, that create/select/delete still return to the correct screens, and that saved DAT assets are released on exit.

**Expected extraction:** approximately 500–800 lines, likely split into multiple small changes.

**Implementation status (September 29, 2026; first sub-slice):** added
`DATura/character_creation_flow.h/.cpp` to own the six official starting-job
choices, their labels, selection wrapping/clamping, and confirmation into a
job value. `main.cpp` retains screen transitions, drawing/input dispatch,
creation-state synchronization, and saving the selected job. Debug x64 build
succeeded. A second sub-slice added
`DATura/character_roster_controller.h/.cpp` for roster row selection and
bounds-safe selected-index policy. The shell continues to own preview assets,
rendering/focus animation, file deletion, and scene transitions. A third
sub-slice moved selected-nation state and bounds handling into
`character_creation_flow.*`, alongside the starting-job policy. Debug x64 build
succeeded. A fourth sub-slice moved player-model-race mapping and initial
character save-data preparation into `character_creation_flow.*`; the shell
still owns file dialogs, DAT/Noesis generation, persistence, and user feedback.
Debug x64 build succeeded. Further Phase 4 work remains; `main.cpp` is now
7,901 physical lines. A fifth sub-slice moved the creation-flow stage
(`Customization`, `StartingJob`, `NationSelection`, or `Inactive`) and the Back
transition from job/nation selection into `CharacterCreationFlow::State`,
removing the two duplicated screen-active booleans from `main.cpp`. The shell
still coordinates drawing, input routing, and panel/scene side effects. Debug
x64 build succeeded; `main.cpp` is now 7,914 physical lines.
A sixth Phase 4 sub-slice added typed roster confirmation intents for selecting
or requesting deletion of the selected character. `main.cpp` still performs the
confirmation dialog and deletes files or loads the selected scene. Debug x64
build succeeded; `main.cpp` is now 7,921 physical lines.
A seventh Phase 4 sub-slice moved roster mode (`None`, select, or delete) and
the Back-to-title intent into `CharacterRosterController::State`, replacing the
two lobby booleans in `main.cpp`. The shell still clears preview assets and
loads the title scene. Debug x64 build succeeded; `main.cpp` is now 7,926
physical lines.
An eighth Phase 4 sub-slice moved saved-character candidate enumeration and
DAT-set/metadata path derivation into `CharacterRosterController`. `main.cpp`
still loads each candidate's DAT set and owns the resulting preview asset
lifetime. Debug x64 build succeeded; `main.cpp` was 7,916 physical lines at
that checkpoint. A ninth sub-slice made roster confirmation intents carry the
selected candidate and moved deletion-path preparation into the controller;
the shell still shows the irreversible-action warning and performs deletion.
Preview metadata now references the same candidate record. Debug x64 build
succeeded; `main.cpp` is now 7,917 physical lines.

### Phase 5 — Finish zone-object editor coordination

**Source area**

Panel population, refresh scheduling, selection synchronization, highlight updates, and world-pick coordination in `main.cpp`.

**Target**

Extend existing zone-object panel/state modules or add `zone_object_editor_controller.*`.

**Module ownership**

- selected object and selected row synchronization;
- panel refresh request state;
- highlight/visibility edit intent; and
- conversion of panel commands into typed edit requests.

**Boundary**

Leave model mutation, collision updates, scene reloads, and direct world picking in their current owners or expose narrow host callbacks. Avoid duplicating the zone-object list/tree/layout logic that has already been extracted.

**Risk and checks**

Check additive selection, world picking, rename/hide/show, refresh after zone reload, and undo or reset behavior if supported.

**Expected extraction:** approximately 200–350 lines.

### Phase 6 — Split frame rendering into composable passes

**Source area**

The large `Render()` function and overlay dispatch.

**Targets**

- `world_scene_renderer.*` for opaque/transparent world, actors, environment, and world overlays;
- retain UI-specific renderers such as `ffxi_title_screen_renderer.*`, `npc_nameplate_renderer.*`, and zone render modules;
- keep frame begin/resolve/present and pass ordering in the application shell until the new boundary is stable.

**Module ownership**

Each pass should receive an explicit render context with only its inputs. The renderer should not own app navigation, settings persistence, input, or scene-loading policy.

**Risk and checks**

Render ordering affects depth, alpha blending, weather, water, nameplates, and overlays. Move one pass or helper cluster at a time. Compare screenshots in representative zones and verify title, character creation, and gameplay scenes.

**Expected extraction:** 400–800 lines in the first pass; further reduction depends on the context boundary.

### Phase 7 — Reduce window-procedure routing

**Source area**

The main window procedure's keyboard, mouse, menu, and custom-window-message routing.

**Target**

`main_window_controller.*` or typed routing helpers beside `win32_application.*`.

**Boundary**

Translate raw Win32 messages into typed input events near the window procedure. Forward domain events to the owning controllers. Keep final application-wide arbitration in one small shell layer; do not move the window procedure into a module that must include every application header.

**Risk and checks**

Input precedence is easy to regress: developer console, chat input, modal panels, title/lobby screens, game menus, and gameplay each consume different keys. Extract after the flow controllers have explicit APIs. Verify key repeat, mouse capture/focus loss, and modal precedence.

**Expected extraction:** 300–600 lines after earlier phases have reduced direct dependencies.

### Phase 8 — Startup/shutdown and final shell review

Review `InitializeMainWindowState`, `InitializeGraphicsState`, `RunApplicationFrame`, and `ShutdownApplication`. Keep lifecycle ownership in the shell unless a resource-owning subsystem can take a complete, explicit lifetime. Remove obsolete forward declarations and duplicated transient state only after all call sites have migrated.

**Expected result**

`main.cpp` should primarily compose subsystems, route a small set of typed events, define the top-level frame ordering, and own genuinely application-wide decisions. A reasonable long-term target is to move roughly 3,000–4,000 lines of subsystem behavior out of the current file. This is an estimate, not a line-count quota.

## 6. Slice discipline

For every implementation slice:

1. State the behavior that must remain unchanged.
2. Move one cohesive responsibility and its owned state.
3. Replace implicit global access with a narrow state, request/result, or callback boundary.
4. Update the Visual Studio project and filter files when adding `.cpp` or `.h` files.
5. Build the affected application and test targets; run relevant automated and manual regression checks.
6. Review the diff for accidental feature changes, duplicate state, dead helpers, and lifecycle leaks.
7. Record any remaining coupling as a follow-up rather than widening the current slice.

Keep commits or review units small enough that a regression can be associated with one extraction. Do not combine these phases with unrelated UI redesigns, behavior fixes, renaming campaigns, or formatting sweeps.

## 7. Completion criteria

The refactor is complete when:

- each extracted subsystem owns its state and resource lifetimes;
- `main.cpp` no longer contains complete title-camera, NPC-event, character-flow, or zone-object workflows;
- render and input routing use explicit boundaries and remain understandable in the shell;
- no broad application-state object has become the new coupling point;
- supported Debug and Release builds pass without new warnings;
- relevant automated checks and manual smoke checks pass; and
- the existing refactoring smoke-test checklist is updated to reflect new ownership and entry points.

## 8. Suggested execution order

Proceed in this order:

1. Phase 0 inventory.
2. Phase 1 title-camera path.
3. Phase 2 settings persistence.
4. Phase 3 NPC event runtime, then conversation presentation boundary.
5. Phase 4 character creation and roster flow, in small slices.
6. Phase 5 zone-object coordination.
7. Phase 6 rendering passes.
8. Phase 7 window/input routing.
9. Phase 8 lifecycle and shell review.

This order starts with bounded state and policy extractions, establishes the callback/result patterns needed by the more coupled flows, and postpones broad rendering and input changes until their dependencies have clearer owners.
