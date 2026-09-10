# DATura Refactoring Smoke-Test Checklist

**Purpose:** Provide a repeatable safety net for behavior-preserving refactor slices

**Applies to:** Debug x64 and Release x64 desktop builds

**Policy:** A refactor slice is not complete if it introduces a compiler warning, test failure, reproducible regression, or unexplained skipped check.

## 1. How to use this checklist

Run the automated checks for every slice. Run the manual checks named by the slice's affected systems. At a roadmap stage boundary, run the complete applicable checklist in both Debug and Release unless a check is explicitly marked not applicable.

Record each manual check as:

- **Pass** — observed behavior matches the expected result;
- **Fail** — behavior differs, crashes, hangs, leaks an obvious resource, or displays corrupt output;
- **Blocked** — required data, hardware, or environment is unavailable; or
- **Not applicable** — the build intentionally does not contain the capability.

Every Blocked or Not applicable result must include a short reason. Do not silently treat an unrun check as a pass.

## 2. Test record

Copy this block into the slice notes or pull-request description:

```text
Date:
Commit/worktree description:
Tester:
Windows version:
GPU and driver:
FFXI install/data source:
Configuration(s): Debug x64 / Release x64
Automated logic tests: Pass / Fail
Compiler warnings: 0 / other
Manual test IDs run:
Failures or blocked checks:
Notes:
```

## 3. Automated build and test commands

Run these commands from the repository root in PowerShell. The path shown matches the repository's Visual Studio 18 Community toolchain; adjust only the Visual Studio edition or installation path when needed.

```powershell
$daturaMsbuild = "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe"

& $daturaMsbuild "tests\DATuraLogicTests.vcxproj" /t:Build /p:Configuration=Debug /p:Platform=x64 /m
& "tests\bin\x64\Debug\DATuraLogicTests.exe"

& $daturaMsbuild "DATura\DATura.vcxproj" /t:Build /p:Configuration=Debug /p:Platform=x64 /m
```

At a stage boundary, before integration, or after changing compiler/linker behavior, also run:

```powershell
& $daturaMsbuild "tests\DATuraLogicTests.vcxproj" /t:Build /p:Configuration=Release /p:Platform=x64 /m
& "tests\bin\x64\Release\DATuraLogicTests.exe"

& $daturaMsbuild "DATura\DATura.vcxproj" /t:Build /p:Configuration=Release /p:Platform=x64 /m
git diff --check
```

Expected automated result:

```text
PASS: 82 application-settings checks
```

Both projects must finish with zero errors and zero new warnings. The logic-test executable must return exit code zero. The exact check count may rise as coverage is added; a lower count is only acceptable when the test source was intentionally revised and reviewed.

## 4. Test data and setup

Before a broad manual run:

- preserve a known-good FFXI installation path or data root;
- identify one standalone model DAT, one valid DAT set, one outdoor zone, one indoor zone, one NPC or monster, and representative audio assets;
- keep a known-good low-poly player preset available;
- note the expected default title/menu state;
- begin with no modal dialogs left open; and
- when testing saved-path behavior, back up any user configuration whose exact contents matter.

The checklist verifies program behavior. It does not authorize deleting or overwriting user configuration or FFXI data.

## 5. Startup, shell, and lifecycle

- [ ] **APP-01 — Valid saved path:** Launch with a valid saved FFXI path. The main window opens, the title view is usable, and no path error appears.
- [ ] **APP-02 — No saved path:** Launch without a saved path. DATura reaches a usable state and presents the intended path-selection/configuration flow without crashing.
- [ ] **APP-03 — Invalid saved path:** Launch with a deliberately invalid saved path. The error is understandable, recovery is possible, and the application remains responsive.
- [ ] **APP-04 — Window and menus:** Verify the main window title, menu bar, enabled states, and initial scene. Commands must not be duplicated or routed to the wrong action.
- [ ] **APP-05 — Configuration lifetime:** Open, close, and reopen the configuration dialog. Values are populated consistently and no stale window handle prevents reopening.
- [ ] **APP-06 — Menu exit:** Exit from the application menu. The process ends normally with no crash or lingering DATura process.
- [ ] **APP-07 — Window close:** Exit with the window close button. Shutdown behavior matches menu exit.
- [ ] **APP-08 — Partial-startup failure:** When practical, trigger a recoverable initialization failure. The error is reported once and already-created resources are released safely.

## 6. Configuration and display

- [ ] **CFG-01 — Settings round trip:** Change settings, accept the dialog, reopen it, and verify every changed control reflects the accepted values.
- [ ] **CFG-02 — Cancel behavior:** Change settings and cancel. Runtime and reopened values remain unchanged.
- [ ] **CFG-03 — Window modes:** Exercise windowed, borderless, and fullscreen modes. Each transition succeeds and the selected mode is reflected in configuration.
- [ ] **CFG-04 — Resolution:** Change to at least two supported resolutions. The viewport and window presentation update without corrupted rendering.
- [ ] **CFG-05 — Resize:** Resize a windowed build repeatedly, including narrow and wide aspect ratios. Rendering remains valid and controls do not become permanently unusable.
- [ ] **CFG-06 — Minimize/restore:** Minimize, wait briefly, and restore. Device-lost/reset handling recovers the scene without a crash or permanent blank frame.
- [ ] **CFG-07 — Alt-tab/focus:** Move focus away and back in each relevant display mode. Input capture, audio-background policy, and rendering resume correctly.
- [ ] **CFG-08 — Rendering options:** Toggle mip mapping, bump mapping where applicable, texture compression, lighting quality, draw distance, environmental animation, mirroring, and collision display. Each setting affects only its intended behavior.
- [ ] **CFG-09 — Reset persistence:** With a model or zone loaded, cause a resolution or display reset. Required textures, UI resources, models, and debug geometry remain usable afterward.

## 7. Loading and scene transitions

- [ ] **LOAD-01 — Standalone DAT:** Open a known model DAT. Loading completes, the model is visible, and its menu/scene state is correct.
- [ ] **LOAD-02 — DAT set:** Open a valid DAT set. All expected pieces are combined and the scene can still be manipulated.
- [ ] **LOAD-03 — Invalid selection:** Cancel a file picker and attempt one known unsupported or malformed input. Cancellation is silent; invalid data reports a bounded error without destabilizing the current scene.
- [ ] **LOAD-04 — Normal zone:** Load a representative normal zone through the menu/browser. Geometry and textures appear and the application remains responsive.
- [ ] **LOAD-05 — Prototype area:** Load one supported prototype or special area. Its special-case path still resolves correctly.
- [ ] **LOAD-06 — NPC/monster:** Load a representative NPC or monster. Model, texture, animation, and nameplate behavior match the pre-refactor baseline.
- [ ] **LOAD-07 — Zone reload:** Change a setting that requires reloading the remembered zone, such as mirroring. The same logical zone reloads once with the updated setting.
- [ ] **LOAD-08 — Return to title:** Return from loaded content to the title screen. Scene-owned resources are released and title interaction works again.
- [ ] **LOAD-09 — Nation and character flow:** Traverse title, nation selection, and character-related scenes in both forward and back directions. State does not leak between scenes.
- [ ] **LOAD-10 — Repeated replacement:** Load several different asset types in succession. The newest scene replaces or coexists with the prior scene according to the intended behavior, without obvious cumulative corruption.

## 8. Player, camera, and input

- [ ] **INPUT-01 — Orbit:** Drag to orbit in each scene that supports it. Motion direction, sensitivity, and release behavior remain unchanged.
- [ ] **INPUT-02 — Pan/zoom:** Pan and zoom through their supported ranges. The camera does not jump when capture begins or ends.
- [ ] **INPUT-03 — Mode transition:** Enter and leave game mode. Camera ownership, cursor visibility, and menus transition correctly.
- [ ] **INPUT-04 — Movement:** Walk and turn in multiple directions. Player yaw, camera following, and animation remain coherent.
- [ ] **INPUT-05 — Grounding:** Traverse slopes, steps, and representative uneven ground. The player remains grounded according to the existing collision rules.
- [ ] **INPUT-06 — Fall/respawn:** Trigger an out-of-bounds or fall recovery in a safe test zone. Respawn uses the expected safe position.
- [ ] **INPUT-07 — Unstick:** Trigger the unstick action. The player returns to a valid position without changing unrelated scene state.
- [ ] **INPUT-08 — Focus loss during drag:** Begin mouse look, move focus away, then return. Capture is released and no drag remains stuck.
- [ ] **INPUT-09 — Cursor policy:** Exercise hardware-cursor enabled and disabled settings. Visibility is balanced and the cursor does not remain hidden after leaving game mode.
- [ ] **INPUT-10 — Shortcuts:** Verify open, mode, weather, unstick, and other documented shortcuts. Each produces exactly one intended command.

## 9. Character workflows

- [ ] **CHAR-01 — Low-poly panel:** Open, close, and reopen low-poly customization. Current state populates consistently.
- [ ] **CHAR-02 — Appearance:** Change race, face, and each supported equipment slot. The rendered character reflects every selection.
- [ ] **CHAR-03 — Animation:** Change the character animation and verify playback remains stable across another appearance change.
- [ ] **CHAR-04 — Randomizers:** Randomize each supported section independently, then randomize all. Values remain valid and the model rebuilds successfully.
- [ ] **CHAR-05 — Preset save/load:** Save a preset, change the character, and load the preset. The restored state matches what was saved.
- [ ] **CHAR-06 — High-poly creation:** Enter high-poly creation, change selections and animation, and navigate back. Scene and UI state remain coherent.
- [ ] **CHAR-07 — Creation export:** Save/export a creation model to a safe destination. The operation reports success or a useful error and produces the expected output.
- [ ] **CHAR-08 — Workflow switching:** Switch repeatedly between low- and high-poly workflows. No prior model, animation, or panel state corrupts the next workflow.

## 10. Zone-object and inspection tools

- [ ] **ZONE-01 — Panel lifetime:** Open, close, and reopen the zone-object panel with a zone loaded. It repopulates without stale handles or duplicate controls.
- [ ] **ZONE-02 — Layout:** Resize the panel and drag all splitters. Minimum sizes hold and panes repaint correctly.
- [ ] **ZONE-03 — Data population:** Populate placed, unreferenced, collision, and draw-batch views. Counts and groupings match the loaded zone.
- [ ] **ZONE-04 — Tree expansion:** Expand representative deep tree nodes. Expansion remains responsive and labels/data correspond to the selected category.
- [ ] **ZONE-05 — Selection actions:** Select, show, hide, highlight, and center representative objects. The scene and panel selection stay synchronized.
- [ ] **ZONE-06 — Transform edits:** Apply translation, rotation, and scale edits to a disposable/test object state. The intended object updates and invalid text is rejected safely.
- [ ] **ZONE-07 — Collision visibility:** Toggle collision mesh visibility from the relevant controls. Rendering and menu/panel state agree.
- [ ] **ZONE-08 — Active-zone replacement:** Keep the panel open while loading a different zone. All rows and selections rebind to the new zone with no stale references.

## 11. Rendering and textures

- [ ] **RENDER-01 — Outdoor reference:** Render a known outdoor zone and compare geometry, textures, orientation, fog, sky, and draw distance to the baseline.
- [ ] **RENDER-02 — Indoor reference:** Render a known indoor zone and verify lighting, alpha materials, occlusion/culling behavior, and close-range geometry.
- [ ] **RENDER-03 — Weather/time:** Cycle supported weather and time-of-day states. Sky, fog, lighting, and environmental animation update together.
- [ ] **RENDER-04 — Water:** Inspect representative water from several angles and distances. Animation, transparency, depth interaction, and reflection-related behavior remain stable.
- [ ] **RENDER-05 — Vegetation:** Inspect animated vegetation with animation off, simple, and smooth. Modes differ as intended and static geometry is unaffected.
- [ ] **RENDER-06 — Orientation:** Toggle mirrored/corrected zone orientation. Geometry, object placement, player movement, and collision remain aligned.
- [ ] **RENDER-07 — Debug overlays:** Toggle collision and other debug overlays. They align with their source geometry and restore render state afterward.
- [ ] **RENDER-08 — Texture viewer:** Open representative compressed, alpha, paletted, and animated textures where available. Decoding and presentation are correct and closing the viewer is safe.
- [ ] **RENDER-09 — NPC/nameplates:** Render NPCs and nameplates at representative distances. Depth, alpha, scale, and visibility behavior remain consistent.

## 12. Audio

- [ ] **AUDIO-01 — Music:** Start, replace, and stop background music. Only the intended stream remains active.
- [ ] **AUDIO-02 — Sound effect:** Play representative sound effects repeatedly. Playback is correct and does not block the UI.
- [ ] **AUDIO-03 — Enable/disable:** Toggle sounds off and on. Existing and new playback follow the setting without requiring a restart unless explicitly designed otherwise.
- [ ] **AUDIO-04 — Background policy:** Test focus loss with background sound enabled and disabled. Playback follows the selected policy.
- [ ] **AUDIO-05 — Simultaneous limit:** Exercise at least one finite sound limit and Unlimited. Excess voices are handled according to the established behavior.
- [ ] **AUDIO-06 — Shutdown:** Exit while audio is playing. The process terminates promptly without a crash or lingering audio.

## 13. Slice-to-check mapping

Use this as the minimum manual subset; add checks when a slice touches more systems than expected.

| Roadmap slice | Required manual groups |
|---|---|
| 1. Verification baseline | APP-01, APP-04, APP-06, APP-07 plus automated tests |
| 2. Graphics lifecycle | APP, CFG, RENDER-01, RENDER-02, RENDER-07, RENDER-08 |
| 3. Player/camera | INPUT, LOAD-04, RENDER-06 |
| 4. Input controller | INPUT, APP-05, CFG-07 |
| 5. Scene coordinator | APP, LOAD, CHAR-08, ZONE-08 |
| 6. Zone-object state | ZONE, LOAD-04, LOAD-07 |
| 7. Zone-object controller | ZONE, INPUT-10 |
| 8. UI panels | APP-05, CFG-01, CHAR-01, ZONE-01, ZONE-02 |
| 9. Menu/command completion | APP-04, CFG-08, LOAD-01 through LOAD-08, INPUT-10 |
| 10. Rendering state cleanup | CFG-09, RENDER |
| 11. Runtime services | APP, LOAD-03, AUDIO |
| 12. `main.cpp` reduction | Entire checklist |
| 13. Test expansion | Automated tests plus affected manual groups |
| 14. Final regression hardening | Entire checklist in Debug and Release |

## 14. Failure handling

When a check fails:

1. record the test ID, build configuration, asset/zone, and exact reproduction steps;
2. distinguish a compile/test failure from a behavioral or visual regression;
3. capture the first relevant error message and diagnostic log rather than only the final symptom;
4. compare against the last known-good build using the same data and settings;
5. keep the refactor slice open until the regression is fixed or explicitly adjudicated; and
6. add an automated check when the failure came from logic that can be exercised without the GUI.

## 15. Completion gate

A slice may be called complete when all of the following are true:

- the required projects build for the slice's configuration gates;
- compilation produces zero errors and zero new warnings;
- all automated logic tests pass with exit code zero;
- `git diff --check` reports no whitespace errors;
- every required manual check is Pass, Blocked with a reason, or Not applicable with a reason;
- no obsolete symbol, duplicate project entry, or abandoned implementation remains from the extraction; and
- affected startup, reset, scene replacement, and shutdown paths have been reviewed.
