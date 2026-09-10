# DATura

DATura is an open-source FINAL FANTASY XI DAT exploration and visualization project.

## DAT replacements

Preview edited DATs from a separate folder using **Resources > DAT Replacements
and Sources...**. Mirror retail paths inside it (for example `ROM/1/35.DAT`),
enable replacements, save, and restart DATura. Missing replacements fall back to
retail; existing replacements are used as-is. The same window shows logical
asset paths and the physical sources opened. Replacements are disabled by default.

See the [replacement guide](Markdown%20Documents/DAT_REPLACEMENT_RESOLVER_2026-09-09.md)
for supported paths, cache behavior, source diagnostics and regression tests.

## Zone geometry diagnostics

After loading a zone, open **Resources > Zone Geometry Diagnostics...** to compare
rendered geometry, collision, and water. Set the cell size and surface slope, or
limit the Y range to inspect an interior floor; Y increases downward. **Analyze**
uses a captured scene snapshot. Reopen the window to capture later scene changes.

Save an offline HTML report for layer toggles, pan/zoom and cell coordinates, or
CSV for occupied-cell data. Coverage is a projected inspection aid, not a
walkability score. See the [validation guide](Markdown%20Documents/RENDERING_COORDINATE_VALIDATION_2026-09-09.md)
for coordinate conventions, snapshot policy, limitations and regression commands.

## Game text

NPC and player nameplates load the Latin-only `font font` atlas and its `fontshp` glyph
rectangles from the English UI resource `ROM/119/51.DAT`. This includes the larger
italic Latin letters; they are distinct from the kanji-containing `font moji`
atlas in `ROM/0/1.DAT`. Game-screen text uses the clean English `font moji`
atlas in `ROM/119/51.DAT`, with bilinear scaling for fractional sizes. Nameplate UVs and
logical glyph sizes come from the DAT rather than an assumed character grid.
No font installation or copied texture asset is required.

Changing the configured FFXI path reloads the atlases and clears cached nameplates.
The current mapping covers printable ASCII; unsupported names or titles use the
system-font fallback independently, so an accented title does not change the
font of an ASCII name. Missing or unsupported atlas layouts also use the fallback.
Native Windows controls (including the dialogue transcript) still use Windows
fonts. Game UI font-size settings continue to apply; configured font families
apply to fallback text.

Build `tests/BitmapFontTests.vcxproj` and run it with the FFXI installation path and
an output BMP path to check atlas loading, spacing, transparency, and path reloads
and produce a preview using the actual renderer.

Nameplates isolate and restore their Direct3D state so scene fog, face culling,
and material texture transforms cannot tint or suppress the labels. Depth testing
still occludes them behind foreground geometry. `tests/NameplateRenderingTests.vcxproj`
checks Config selection/persistence, GPU colors, scene-state independence/restoration,
depth occlusion, composite/rank icons, Job Master placement, and Linkshell tint/cache
isolation. Run it with the FFXI installation
path and an output BMP path to produce a gallery of the supported player icons.

Player names use white lettering and follow the character-creation name by
default. `[Player.Nameplate]` in `DATura.game-ui.config` beside the executable
controls visibility, an optional name override, one icon, and Linkshell RGB color.
Config > Appearance > Player icon changes the selection immediately and saves it;
the adjacent Job Master checkbox independently enables the three stars above the name.
The Subtitle selector offers None, Linkshell name, or Jobs + levels. Its detail
row edits the Linkshell name or the two job/level pairs (for example, `WAR 99 / NIN 37`).
Edits apply when leaving a field; choices and values persist between sessions.
The levels are independent, and selecting no subjob displays only the main job.
These are temporary per-player presentation values until gameplay profiles are connected.
Restart after manually changing the file. The source template is
`DATura/DATura.game-ui.config` and is copied beside the executable by the build.
Icons are loaded from the same installation DAT's `menu ustatshd` atlas, including
Linkshell, Mentor, staff, status, Ballista, Campaign combinations, and Monstrosity
rank variants; the config lists supported IDs. Bronze/silver/gold Mentor options
are explicitly labeled previews of Assist Channel chat badges. The current retail
HD shape table maps GM, SGM, and LGM to the same image, so these choices share artwork.
The former `busy` ID remains an alias for the corrected `auto-party` label.
Only Linkshell icons are tinted. Unknown or unavailable icons leave the name alone.
These are local viewer appearance settings, not online account or status data.

## Player controls

In game mode, crossing Bastok Markets' exits loads Bastok Mines, Port Bastok,
South Gustaberg, or Metalworks. The corresponding return entrances also work.
Zoning keeps the player model/equipment, places the player at the destination
entrance facing inward, resets the respawn point, and refreshes NPCs, collision,
environment, and music. Edit-camera movement does not trigger zoning.

Transition centers and arrivals come from LandSandBoat's client-derived zone data.
Trigger widths and height tolerance are provisional bounded planes, pending retail
MAPRECT decoding; these are not verified exact retail trigger volumes. Mog House
entries are not included. See [zoning notes](Markdown%20Documents/BASTOK_ZONING.md).

In game mode, **W/S** move forward/backward, **A/D** turn, **Q/E** strafe left/right,
and **Space** jumps. Hold **left + right mouse buttons** to move forward with the
camera locked behind the player; move the mouse horizontally to steer. Release
either button to stop mouse-driven movement. This uses the selected walk/run speed.
Press **Shift** to toggle walking/running (walking by default);
**Alt** toggles extra-fast running (16 units/second), overriding the Shift mode until
pressed again. Its run animation speeds up to match. Backward and sideways steps
retain walking speed. **Ctrl** slows movement and its animation. Diagonal
movement keeps the same speed, and jumping requires a new press after landing.
In edit mode, Q/E still move the free camera vertically.

Normal player movement uses the relaxed upper-body tracks. Strafing uses the
directional lower-body motion with the selected weapon bank's upper-body track.
Jumping plays the jump motion once and returns to locomotion on landing.

Build `tests/PlayerAnimationTests.vcxproj` and run its executable with the FFXI
installation directory to validate the clips and D3D9 vertex buffers for all
eight races. Input and jump/collision checks are in `tests/DATuraLogicTests.vcxproj`.

## NPC interaction

NPCs with verified fresh-character quests show a gold `!` above their name, drawn
from the official FFXI nameplate font with a dark outline. The starter catalog
covers 70 NPCs in Bastok, San d'Oria, and Windurst. Markers scale with distance and
are occluded by foreground geometry. Missing font assets suppress the marker.
These are static starter assignments: speaking to an NPC does not accept a quest,
remove its marker, or turn it into a `?`. Quest tracking comes later.
See the [starter quest catalog](Markdown%20Documents/STARTER_QUEST_MARKERS.md) for
sources, eligibility assumptions, exclusions, and editing instructions.

NPCs with researched roles show a smaller title beneath their name. The editable
`DATura/npc_roles.csv` catalog tracks all placements by zone and NPC ID, with
sources and review status. The initial 225 verified titles cover Bastok's four
city zones; unresearched titles remain hidden. See the
[NPC role catalog guide](Markdown%20Documents/NPC_ROLE_CATALOG.md) for editing,
validation, and deployment instructions.

In game mode, left-click an NPC to select it (marked with `> name <`). Click the
selected NPC again to open **NPC Dialogue**, a resizable, scrollable window with
speaker names and conversation history. Click empty space to deselect; Escape
clears the target and hides chat. Changing zones or toggling edit/game mode resets
selection and chat history. Right-drag still orbits the camera.

Picking uses projected head-to-feet bounds for NPCs in the current render pass;
overlapping targets prefer the closest depth. It is an approximate body hit area.

NPC catalog CSV rows accept an optional eleventh field containing UTF-8 dialogue
(use CSV quoting for commas). Existing ten-field catalogs still load. Dialogue can
also be supplied through `Placement::dialogue`. NPCs without assigned dialogue show
an explicit unavailable message. Retail zone dialogue tables are not yet mapped
to NPC event scripts; the chat does not infer those associations from entity IDs.

## Resource inspection

The **Resources** menu exposes non-model data learned from the
[Windower/POLUtils](https://github.com/Windower/POLUtils) readers:

- **Current Zone Dialog / NPCs...** resolves and combines the loaded zone's dialog and entity-name DATs.
- **Open Resource DAT...** inspects an individual resource file.

The resource browser currently recognizes:

- VTABLE/FTABLE file IDs across `ROM` through `ROM19`;
- zone dialog tables and 32-byte NPC/mob lists;
- `d_msg` string tables (all three known layouts), `XISTRING`, and legacy 64-byte string tables;
- encrypted 0xC00-byte item records, including general items, equipment, usable items, currencies, slips, and related layouts;
- `BGMStream` and `SeWave` audio headers; and
- embedded FFXI graphic-header metadata.

Text is decoded as Windows CP932 with known FFXI element, Auto-Translate,
resource-reference, and dialog control markers. Rare private glyphs may still
appear as fallback characters. The graphic scanner inventories embedded headers
without exporting their pixels.

## Water rendering

Zone loads include supported generator-owned river, sea, canal and basin surfaces,
with authored placement, transparency, time-of-day colors and texture flow.
Water participates in the object inspector's visibility controls.
This first pass covers permanent surfaces; shoreline foam, spray, waterfalls and
other particle lifetimes still need their own effect playback.
See the [water implementation notes](Markdown%20Documents/WATER_RENDERING_2026-09-09.md)
and [water graphics tests](tests/WATER_RENDERING.md).

## Zone interiors

Loading the cataloged districts of Bastok, San d'Oria, Windurst, Jeuno, Whitegate,
Nashmau, Selbina, Mhaura, Kazham, Norg, or Adoulin automatically loads their
verified companion room DATs, including companions in other DAT folders.
Room geometry, textures, and collision use their authored world positions. Room
DATs replace their linked outdoor proxy meshes, which are released from CPU/GPU
memory after the companion loads successfully. Missing rooms retain the proxies.
Zone `_h`/`_m`/`_l` meshes now switch with the placement's authored distance
thresholds; the object inspector exposes these fields and the replacement link.
Room objects appear in the object panel with their source DAT in the name and support
the usual visibility and transform controls.

The catalog covers 230 drawable rooms across 25 zones, plus one environment-only
Windurst Walls reference with no drawable geometry. Ownership is verified against
MZB replacement links, RID room-entry triggers, file-table mappings, and room
headers. This includes Adoulin rooms that have triggers but no outdoor proxies.
Tavnazian Safehold, Al Zahbi, Rabao, and the three past-era city zones have no
separate room references in the audited data; their main DATs remain intact.
Neighboring files are never loaded merely because they are nearby. Missing,
unexpected, and empty companions are skipped. Opening a room DAT on its own
still loads only that file. See [city coverage and audit](Markdown%20Documents/CITY_ROOM_COVERAGE_2026-09-07.md).

Rooms use frustum culling while the main zone retains its outdoor visibility tree
and environment. Room-specific weather/effects and portal-based room streaming
are not yet implemented. The raw chunk inventory remains that of the main DAT.

Build `tests/ZoneRoomTests.vcxproj` and run its executable with the FFXI installation
directory to check geometry, materials, collision, visibility, and missing-file
handling against installed assets. Add `--gpu` to also check D3D9 buffer transfer
and rebuilding. Without an installation argument it tests catalog resolution and LOD behavior.

## Audio playback

The **Audio > Music / SFX Player...** menu scans the configured FFXI installation
for installed `BGMStream` music and `SeWave` sound effects. The player supports
searching by name, ID, bank, format, or path; double-click playback; stop and loop
controls; and opening an individual `.bgw` or `.spw` file. ADPCM and raw PCM
payloads are decoded to a temporary WAV cache for playback without modifying or
redistributing the source game files. Catalog scanning and first-time decoding
run in the background so the DATura interface remains responsive. ATRAC3 headers
are listed for inspection, but that codec is not currently previewable.

For parser regression checks, build `tools/ffxi_resource_probe.vcxproj` and run:

```text
ffxi_resource_probe <resource-path>
ffxi_resource_probe --id <ffxi-install-directory> <file-id>
```

## License

Except for separately identified third-party material, DATura is free software licensed under the [GNU General Public License, version 3 or later](LICENSE).

Third-party components and reference material retain their respective copyright and license terms. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for the currently identified material.

The DATura license does not grant permission to copy or redistribute FINAL FANTASY XI game data or other assets owned by their respective rights holders.
