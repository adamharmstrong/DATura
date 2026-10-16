# Authored zone fog

The 2026-10-16 local installation audit scanned all 295 model paths in
`DATura/Zone_DATs_Table.csv`, with no missing DATs. It found 9,519 environment
records and enabled terrain distance fog in 292 zone files. The three files
without environment records are IDs 229, 278, and 279 (unnamed in that table).
The full per-zone inventory is `tools/zone_fog_inventory.csv`.

This is an inventory of authored environments, including event and interior
variants. It does not prove that every weather variant occurs naturally in a
zone. Most zones author distance haze even in fair weather; a fog-enabled
record does not necessarily mean visibly dense mist. DATura should use the
active environment rather than maintain a hardcoded list of foggy zones.

Examples from the ordinary `weat` environments (minimum full-fog distance
across time records, in DAT coordinates):

| Zone | Fair weather | Mist |
| --- | ---: | ---: |
| Pashhow Marshlands | 401.0 | 69.3 |
| Jugner Forest | 351.5 | 99.0 |
| Konschtat Highlands | 341.6 | 128.6 |
| La Theine Plateau | 351.5 | 128.6 |
| Valkurm Dunes | 341.6 | 148.5 |
| The Sanctuary of Zi'Tah | 130.0 | 99.0 |

## Rendering

DATura already parses terrain fog from 0x2F records, interpolates its color
and distances by time and weather, and applies it to terrain. A zero far
distance is an authored off sentinel and stays discrete during interpolation.

World NPCs and players now opt into that same active zone fog. Previously,
their calls used the standalone model path, which disabled fog. Character
creation, roster, and companion previews retain their separate atmosphere.
Sky exclusion and existing weather selection are unchanged. No additional
ground-mist particles or volumetric effects are implemented by this change.

Both fog setup paths and the shadow fog attenuation reject nonfinite
distances, nonpositive far distances, and reversed/equal ranges.

## Reproduce and validate

Run `python tools/audit_zone_fog.py "<FFXI installation root>"`. It writes
`artifacts/zone-fog/zones.csv` and `records.csv`; the latter includes environment
paths, time labels, distances, original packed colors, and enabled status.
Only the model DATs are scanned; separately loaded room resources are outside
this inventory. Retail files are read without modification.

The audit passed synthetic enabled, disabled-sentinel, and truncated-chunk
checks. Material rendering tests now cover world actor fog versus preview
fog, infinity/NaN rejection, and a zero-far sentinel paired with negative near.
The texture shader now consistently stores a `RendererBackend::Shader*`, creates
and binds it through the graphics interface, and deletes that owning wrapper.
That correction was already present when follow-up verification began.
The full DATura Release/x64 application built successfully to
`artifacts/zone-fog/app/DATura.exe`.

The local Visual Studio installation also reported LNK1101 (MSPDB140.DLL version
mismatch). Verification builds disabled debug symbols using an imported targets
file; shader and application code compiled normally. This workaround does not
repair the installed Visual Studio debug-symbol tools.

Follow-up results on 2026-10-16:

- Focused material/depth/fog suite: **226 assertions, zero failures**, including
  texture/bump shader execution, world actor fog, preview isolation, fog endpoints,
  invalid values, time interpolation, and sky exclusion.
- Installed zone suite: **277 assertions, zero failures**. Pashhow, Jugner,
  Konschtat, La Theine, Valkurm, and Zi'Tah render at 640x640 in fair weather and
  mist at midnight and noon. Each capture is paired with fog disabled under the
  same weather lighting. Fog changes visible terrain and moves at least 95% of
  materially changed pixels toward the authored fog color.
- Inspected day/night comparisons: nearby detail remains visible, distant
  scenery fades, and mist is denser than fair-weather haze. Night fog retains
  the darker authored colors. Forest foliage remains transparent rather than
  becoming opaque fog-colored rectangles.
- The broader suite initially passed 2,504 assertions. Concurrent shadow work
  added new tests; its subsequent 2,657-assertion run had five failures in the
  full-resolution grazing-ground shadow case. The focused fog checks still pass.

Run the installed-zone mode with:

```text
MaterialRenderingTests.exe --zone-fog "<FFXI installation root>" "<capture directory>"
MaterialRenderingTests.exe --materials-only "<fixture capture directory>"
```

Captures and per-case metrics are in `artifacts/zone-fog/captures/`; the reviewed
contact sheets are `day-comparison.jpg` and `night-comparison.jpg`. Raw captures
include the weather and minute in their names. These are real DAT geometry,
textures, environment lighting, and sky-dome renders through DATura's renderer.
They exclude generated cloud/weather particles and live NPC placement. Actor
fog is checked separately by GPU pixel fixtures. This validates representative
zone fog behavior; it is not a retail screenshot parity comparison of all 292
fog-bearing zone files.
