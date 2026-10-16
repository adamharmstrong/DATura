# DATura

DATura is an open-source FINAL FANTASY XI DAT exploration and visualization project.

## DAT hex reader

Open **Tools > DAT Hex Reader...** to inspect and modify a DAT buffer. The file
picker starts in the current FFXI folder from Settings, with the standard
PlayOnline installation path as the fallback.

- Browse 64 KB pages, jump to an offset, or select a block by start and length.
- The Go box accepts absolute offsets and signed relative distances (`+20`,
  `-0x10`), using the selected offset base. Navigation offers start/end jumps,
  select-to-start/end, next/previous modified bytes with wraparound, and copy offset.
  Ctrl+Home/End jumps to the file boundaries; Ctrl+Shift+Home/End extends to them.
  Alt+Insert copies the offset (with `0x` for hex).
- Choose 8, 16, or 32 bytes per row, hexadecimal or decimal offsets, and ASCII or
  Shift-JIS (CP932), Windows-1252, DOS-437, EBCDIC-500, or Macintosh-10000 text. Offset and length inputs use the selected base; a `0x`
  prefix always means hexadecimal.
- View offers 1/2/4/8/16-byte grouping and combined, hex-only, or decoded-text-only
  displays. Grouping changes spacing without changing byte order. Selection,
  navigation, inspector values, and modified-byte highlights work in every mode.
- The selected display encoding is also used by Text search/replacement and
  Copy decoded text. EBCDIC/Macintosh require their Windows code pages to be
  installed; unsupported selections retain the previous encoding.
- Search for hex bytes, encoded text, signed/unsigned 8/16/24/32/64-bit integers,
  or Float32/Float64 values using the selected endian. Numeric inputs are decimal
  unless prefixed with `0x`. UTF-8 and UTF-16 little/big endian text searches
  are also available, without automatically adding a byte-order mark. Replace match uses the same type for the replacement
  and requires equal byte lengths; use Find first to select a match.
- Binary-byte search accepts complete groups of 8 bits, whitespace, and an
  optional `0b` prefix. Find/Find All and equal-length replacement support this type.
  The inspector also shows both Windows/network GUID layouts and LE/BE FILETIME.
- Search the whole file, including matches across page
  boundaries. Find Next wraps around; Cancel stops a background search.
- Replace All applies equal-length replacements across the whole buffer as one
  undoable operation. Matches are processed from the beginning without overlaps;
  inserted replacement bytes are not searched again. Cancel applies no changes.
  Exceeding 100,000 matches or 1 MB of replacement bytes rejects the whole operation.
- Choose whole-file or selection-only search. Find Next/Previous preserve the
  original search block and wrap within it; Find All and Replace All also obey
  the chosen scope. Matches must fit completely inside the range. Start a fresh
  Find to capture a new selection or scope.
- Find Previous (Shift+F3) wraps backward through matches. Find All counts every
  overlapping match and lists the first 10,000 offsets for navigation; edits clear
  the list. Searches run in the background and can be cancelled.
- Byte statistics show whole-buffer frequencies for all 256 byte values and
  Shannon entropy in bits per byte. Analysis includes edits and supports Cancel.
- After Byte statistics finishes, Distribution chart opens a resizable graph of
  all 256 byte counts. Hover for the byte, count, and percentage. The graph is a
  labeled snapshot of the analyzed whole file or selection; edits clear the
  chart-launch result, while open snapshots remain available for comparison.
- Select bytes in the hex or text columns, then copy hex or decoded text. The
  inspector shows binary, signed/unsigned integers and floating-point values
  in little or big endian (including 24-bit integers), Windows-layout GUIDs,
  UTC FILETIME timestamps, and CRC-32 for selections up to 64 KB.
- Edited bytes appear in red in both columns; undo restores their original color
  when their values return to the source bytes.
- Copy selections as C byte arrays, escaped bytes (`\xNN`), or Base64 using
  Copy formatted or the right-click Copy as menu (up to 1 MB).
- Fill pattern repeats the hex bytes in the overwrite input across the selected
  block (up to 64 KB), truncating the final repetition at the selection boundary.
  Each fill is one undoable operation and preserves the file size.
- Apply byte operations to a selected block (up to 64 KB): invert bits, AND/OR/XOR
  with a repeating hex mask from the overwrite/fill box, reverse the whole block,
  or swap byte order within 16/32/64-bit words. Word swapping requires a length
  divisible by the word size. Each operation supports undo/redo and preserves size.
- Bookmark selected blocks and return to them during the current file session.
  Ctrl+Shift+0–9 sets or reassigns a numbered bookmark; Ctrl+0–9 restores its block.
  Numbered slots appear in the bookmark list, Navigation, and the right-click menu.
  Removing a numbered bookmark frees its slot; opening another DAT clears all slots.
- Compare the current buffer with another binary file and navigate to the next
  differing byte. Size differences identify bytes missing from either file.
- Choose CRC-32, Adler-32, MD5, SHA-1, SHA-256, SHA-512, or CRC-32 + SHA-256
  checksums of the current buffer, including edits. Choose whole-file or selection
  analysis for both checksums and byte statistics; reports identify the byte range.
  Comparison and checksums run in the background and support Cancel.
- Additional checksum presets: additive 8/16/32/64-bit byte sums (modulo the
  chosen width), CRC-16/CCITT-FALSE, CRC-16/X-25, and CRC-64/ECMA-182. These
  support file/selection analysis, expected-value verification, and reports.
  CRC parameters and check vectors follow the [CRC RevEng catalogue](https://reveng.sourceforge.io/crc-catalogue/all.htm).
- Paste an expected hex checksum and use Verify checksum to compare it with the
  current whole-file or selection result. Reports show MATCH/MISMATCH and the range.
  The combined CRC-32 + SHA-256 option verifies the expected SHA-256 value.
- Copy a calculated checksum directly, or export completed analysis reports as
  new UTF-8 text files. Reports include the source path and whether the buffer
  contains edits. New calculations and edits clear previous exportable results.
- Copy/export source-code byte arrays for Pascal, Java, C#, and VB.NET in addition
  to C. Java output casts hex values to signed bytes; all formats preserve the
  original byte values. Source-code exports are declaration snippets for embedding.
- Export C byte arrays (`.h`), escaped bytes, or Base64 (`.txt`) to new files
  from Export selection or the right-click menu (up to 1 MB, including edits).
- Copy a selected hex table through **Copy as > Hex table with offsets and decoded text**,
  or export it as a new UTF-8 plain-text file from **Export selection**. Tables
  include original offsets and the chosen decoded-text encoding (up to 1 MB).
- Export Rich Text (`.rtf`) with decoded text and Unicode-safe escaping, or a
  standalone LaTeX (`.tex`) document containing an ASCII hex/offset table.
  Both support selections up to 1 MB and create new files only.
- Export Motorola S-record (S0/S3/S7, 32-bit offsets, record checksums) or an HTML
  hex table containing original offsets and decoded text in the chosen encoding.
  Both export up to 1 MB to new files; HTML escapes source paths and text.
- Export a selected block as a new binary file, or Intel HEX-386 text (up to 1 MB).
  Intel HEX preserves original file offsets, splits records at 64 KB boundaries,
  and includes record checksums and EOF. Offsets must fit in 32 bits.
- Enable editing to type hex byte pairs directly, paste hex with Paste Overwrite,
  overwrite a selected block with hex bytes or fill it with
  one byte. Undo and Redo work on the buffer. Editing preserves file size.
  Direct typing commits after two digits; Esc cancels an incomplete byte.
  Paste overwrites from the selection start, up to 64 KB, and cannot extend EOF.
- **Save Copy...** creates a new binary DAT. **Export page...** creates a new
  UTF-8 text file. Neither action can overwrite **any existing file**, including
  official DATs, the original, or a previously saved copy. Every save needs a
  new filename. The original is opened only for reading and is never modified.

In the byte display, Ctrl+C copies hex, Ctrl+F focuses search, Ctrl+G focuses
Go to offset, F3 finds the next match, Ctrl+Z/Ctrl+Y undo/redo, and Ctrl+S invokes
Save Copy. Ctrl+V pastes hex in overwrite mode, Ctrl+B bookmarks a block, and
Ctrl+A selects the entire file. Arrow keys move by byte/row; Page Up/Down moves
by 64 KB. Enter applies Go to offset, search, or the replacement-byte field.
Right-click provides copy, paste, bookmark, binary export and Save Copy commands. Null bytes display
as underscores; other nonprintable bytes display as dots. Shift-JIS pairs decode
within each row. This tool does not interpret DAT structures; it supports fixed
size byte changes rather than inserting or deleting bytes.

Intel HEX format reference: [Keil documentation](https://www.keil.com/support/docs/1584/_hlp_hexfile.htm).

The real shell-dialog smoke test is optional (`DATURA_TEST_NATIVE_PICKER=1`);
normal regression runs test picker configuration and STA/MTA routing without
opening an interactive dialog.

Regression checks: build `tests/DatHexEditorTests.vcxproj` (Release, x64) and run
`tests/bin/x64/Release/DatHexEditorTests.exe` from the repository root. Tests use
new fixture folders under `artifacts/dat-hex-tests` and check original protection,
hard-link/case aliases, save races, failed-save cleanup, search, selection,
inspector values, direct typing/paste, bookmarks, checksums, comparison, binary
exports and undo/redo. Creating hard-link test fixtures may require a
run outside the filesystem sandbox.

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

Long-running searches and analysis show elapsed time. Checksums, statistics,
full scans, replacements, and comparisons show percentage, bytes scanned, scan
speed, and estimated seconds remaining once enough progress is available.
Estimates use average scan speed; Cancel remains available during each job.

**Export selection > Join file before/after buffer (new copy)** concatenates
the entire current buffer (including edits) with another file in the chosen
order. Joining streams in the background with progress and cancellation; it
creates a new binary copy and never changes either input or replaces an existing
output. Cancelling removes the unfinished output.

**Export selection > Split buffer into new copies** writes the entire edited
buffer as numbered `.part0001`, `.part0002`, ... binary files, using 64 KB,
1 MB or 16 MB parts (up to 1,024 files). The final part may be smaller.
Splitting runs in the background with progress and cancellation. Any collision,
failure or cancellation rolls back the newly created set using owned file handles;
existing files and the source DAT are preserved.

In the hex view, **Ctrl+Left/Right** navigates individual hex digits (nibbles),
including across rows and pages. In edit mode, typing replaces the selected digit
while preserving the other digit, then advances. Each digit edit supports undo.
**Esc**, normal byte navigation, or clicking the view returns to byte mode;
digit navigation is unavailable in text-only view. Saving still creates a new copy.

Additional checksum presets: CRC-32C (Castagnoli), CRC-32/BZIP2, CRC-16/ARC,
and CRC-16/MODBUS. Each supports whole-file/selection calculations, expected
checksum verification, copying, and reports. Standard test vectors and parameters
are documented in the [CRC RevEng catalogue](https://reveng.sourceforge.io/crc-catalogue/all.htm).

Right-click the byte view and choose **Paste overwrite as** to paste decoded
text in the chosen encoding, UTF-8 text, or Base64 bytes. Pastes accept up to
64 KB of decoded bytes, require edit mode, preserve file size and support one-step
undo. Invalid Base64, unrepresentable text and invalid Unicode are rejected.
Ctrl+V continues to paste hex byte pairs. Saving always creates a new copy.

Clipboard options also support copying binary digits (eight per byte), pasting
binary digits, and pasting escaped bytes (`\x00\xFF`). Invalid or incomplete
bytes are rejected. All paste formats retain the 64 KB decoded limit and undo.

The data inspector shows floats with enough significant digits to preserve their
value (9 for Float32, 17 for Float64). Right-click the byte view to copy the
complete inspector readout, or use **Export selection > Data inspector report**
to create a UTF-8 snapshot with the source, edit status, byte order and offset.
Reports always use a new filename.

The inspector additionally decodes signed LEB128, unsigned ULEB128 (up to
64 bits, showing consumed byte counts) and little-endian DOS time/date fields.
Variable integers report truncated/overflowing encodings. DOS dates are validated
and show that their timezone is unspecified. These appear in inspector reports.

Search types include signed LEB128 and unsigned ULEB128 (decimal or `0x`
integers, up to 64 bits). Searches use the canonical shortest encoding and ignore
the byte-order setting. Forward/backward/find-all and range selection work as
usual. Replacement remains limited to values with equal encoded byte lengths.

The **Navigation** menu and byte-view context menu can follow 16/24/32/64-bit
pointers at the current byte, using the chosen endianness. Unsigned pointers
are absolute offsets; signed pointers are displacements from the current byte.
Truncated and out-of-file destinations are rejected, including overflow.
Navigation changes no bytes, and destinations at EOF are supported.

Hex-reader controls are grouped into **View & copy**, **Edit & select**,
**Search & replace**, **Navigation**, and **Analysis** tabs. Open, Save Copy,
Undo/Redo, editing, byte order, Cancel and export/file tools remain visible.
Ctrl+F and Ctrl+G open the appropriate tab. The hex view and inspector remain
visible while switching tabs; settings and entered values are preserved.

The hex view uses blue headers and offset addresses, alternating black and
reddish-brown byte columns (or groups), and bold red modified bytes in both
hex and decoded text. Colors are streamed in one update to keep paging responsive.

**Navigation > Next/Previous value by size** moves by 1, 2, 3, 4, 8 or
16 bytes and selects the complete destination value. The byte-view context menu
has the same options. Movement works across pages, preserves array alignment
relative to the starting offset, and keeps the selection when no full value fits.

The hex reader defaults to **32 bytes per row**, which aligns the entries in
many FFXI DAT tables. The View & copy tab still offers 8 and 16 bytes per row
for files with different layouts. Row width is a display choice, not a format assumption.

**View & copy > View > Hexadecimal integers in data inspector** switches integer
interpretations between decimal and hexadecimal. Signed hex values retain their
sign (for example, `-0x1`); raw hex remains available separately. This display
setting changes no bytes and is included in copied/exported inspector reports.
