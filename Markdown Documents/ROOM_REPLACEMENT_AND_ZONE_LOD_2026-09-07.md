# Room replacement and zone LOD

## Identified fields

Offsets below are relative to a decrypted 100-byte (`0x64`) MZB placement,
not the enclosing DAT chunk or the MMB geometry header.

| Offset | Type | Meaning |
| --- | --- | --- |
| `0x00` | 16-byte name | Resource name; terminal `_h`, `_m`, `_l` identify detail variants |
| `0x38` | float | High-detail distance threshold |
| `0x3C` | float | Medium-detail distance threshold |
| `0x40` | float | Placement draw distance; zero is unlimited |
| `0x50` | uint32 | Replacement sub-area ID; zero means no linked sub-area |

DATura previously kept the distance fields in `vec[1..3]` and the sub-area
link in `data2[3]`, but did not use them for these decisions. There is no
single newly identified MMB "LOD bit" involved in this zone-selection path.
The runtime `levelMask` added by this change is DATura metadata, not a DAT flag.

## Evidence

The project rendering corrections and findings documents identified these byte
offsets. They were cross-checked against Xim's `ZoneDefParser.kt`,
`ZoneObjectUtils.kt`, `ZoneDrawer.kt`, and `ZoneTables.kt`, obtained from the
[published source mirror](https://github.com/Masin-M/xim-docker/blob/main/src.zip)
after the [original source download](https://xim.pages.dev/source.zip) returned 403.
Those sources are a behavioral reference, not proof of every retail executable policy.
The behavior was independently implemented and checked against the installed DATs.

In Bastok Markets, 30 main-zone placements carry nonzero replacement links in
the range 274–287. `FTABLE[subAreaId + 0x64]` resolves that range to
`ROM/1/61.DAT` through `74.DAT`. These are sub-area IDs, not direct VTABLE IDs.
The catalog now records the corresponding IDs for all four supported Bastok zones.

## Behavior

After each companion has successfully produced geometry, its linked outdoor
proxies are marked replaced. Their submeshes and CPU vertex/index vectors are
erased; shared GPU buffers are released and rebuilt from surviving geometry.
Derived visual-collision triangles for those proxies are also removed, with
collision indices remapped. Dedicated MZB collision remains authoritative and
is retained; it is not deleted by a bounding-box heuristic. Missing, invalid, or
empty room files keep their outdoor proxies. Small diagnostic placement records
remain to preserve the main zone's visibility indices and explain replacements.

For authored LOD families, distance from the player/orbit target to the placement
origin selects high below the high threshold, medium below the medium threshold,
and low otherwise. The placement draw-distance limit applies separately. Missing
variants fall back in order H/M/L, M/H/L, or L/M/H respectively. Distinct available
variants remain separate in the geometry accumulator and only the selected one
is drawn. Alternate variants are not emitted again as unreferenced geometry.
Physics uses the most detailed available visual mesh independently of LOD distance.

The object inspector labels the distance fields, replacement sub-area, and
whether a placement's geometry was replaced. Scene-wide opaque batches are not
used for placements requiring distance selection.

## Validation and scope

`tests/ZoneRoomTests.vcxproj` covers threshold boundaries, missing variants,
accumulator separation, absent-room fallback, proxy removal, room materials,
visibility, and GPU-buffer rebuilds. Installed-data runs verify 55 rooms and
replacement counts of 30 (Markets), 14 (Mines), 14 (Port), and 21 (Metalworks).

The subsequent [city expansion](CITY_ROOM_COVERAGE_2026-09-07.md) covers 230 drawable rooms
across 25 zones. Rooms still load eagerly. Portal-triggered room
streaming and character/equipment LOD are separate work. The tests validate
parsed geometry and GPU resource ownership; they are not a retail frame comparison.
