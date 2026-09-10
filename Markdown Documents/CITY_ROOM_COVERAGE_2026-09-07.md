# City room coverage

The loader now resolves 230 drawable companion DATs across 25 city zones,
175 more than the original Bastok catalog. It also recognizes one referenced
Windurst Walls environment placeholder with no drawable geometry, skipped by
the existing empty-room fallback.

| City / settlement | Zone and drawable room counts |
| --- | --- |
| Bastok | Mines 13; Markets 14; Port 9; Metalworks 19 |
| San d'Oria | Southern 18; Northern 13; Port 6; Chateau d'Oraguille 6 |
| Windurst | Waters 21; Walls 6; Port 7; Woods 8; Heavens Tower 4 |
| Jeuno | Ru'Lude Gardens 10; Upper 10; Lower 13; Port 14 |
| Aht Urhgan Whitegate | 11 |
| Nashmau | 1 |
| Selbina | 5 |
| Mhaura | 6 |
| Kazham | 8 |
| Norg | 2 |
| Adoulin | Western 4; Eastern 2 |

Tavnazian Safehold, Al Zahbi, Rabao, Southern San d'Oria [S], Bastok Markets [S],
and Windurst Waters [S] have no nonzero room references in the audited MZB
placement and RID trigger tables. Their existing main-zone geometry is unchanged.
These six zones plus the 25 above total 31 audited zones. This audit does not
claim every lore location or unrelated standalone interior is a room companion.

## Ownership evidence

`tools/audit_city_rooms.py` produces `tools/city_room_audit.json` using installed
DATs and the project's zone table. No room is assigned by physical proximity alone.

- MZB placement field +0x50 supplies the replacement sub-area ID.
- RID interaction entries whose source ID starts with `m` supply additional room
  IDs, even when no outdoor placement needs replacing. The RID header's +0x10
  points to the interaction table; after its 16-byte header, records are 64 bytes,
  with source ID at +0x24 and room ID at +0x2C.
- All audited city sub-area IDs are below 0x271 and resolve through
  VTABLE/FTABLE at `subAreaId + 100`.
- Each resolved companion is checked against its four-byte directory root and
  loaded by the real parser in the regression tests.

RID decoding and sub-area table offsets were cross-checked with
`ZoneInteractionSection.kt`, `Scene.kt`, and `ZoneTables.kt` from the
[Xim source mirror](https://github.com/Masin-M/xim-docker/blob/main/src.zip).
The mapping was independently checked against the installed files.

Important exceptions captured by the catalog:

- Northern San d'Oria crosses from `ROM/1/127.DAT` into `ROM/2/0.DAT`.
- Southern San d'Oria and Port Windurst contain gaps in sub-area IDs.
- Selbina and Southern San d'Oria share the root name `r_1s`; it is not a unique
  ownership identifier.
- Adoulin's rooms are in `ROM9/5/49..52.DAT` and `ROM9/3/84..85.DAT` and appear
  only in room-entry triggers, not placement replacement links.
- Windurst Walls `ROM/2/45.DAT`, sub-area 407, is a referenced environment-only
  placeholder; it does not produce render meshes or retire any outdoor proxy.
- Unreferenced nearby files (such as `ROM/2/71.DAT`, `112.DAT`, and `120.DAT`)
  are not automatically added.

## Runtime and validation

The catalog uses installation-relative paths and explicit verified ID spans.
The loader continues to preserve each main zone's environment and visibility
metadata, namespace room materials, transfer collision, and remove outdoor
proxies only after the matching room produces geometry. Trigger-only rooms
append without deleting unrelated main-zone geometry.

The installed-data test run covers all 31 audited zones, including those without
companions. The D3D9 run loaded all 230 drawable rooms, resolved every named room
material, and passed proxy-removal, collision, visibility, missing/wrong-file,
GPU ownership, and device-buffer recreation checks.

```powershell
python tools/audit_city_rooms.py 'C:/Program Files (x86)/PlayOnline/SquareEnix/FINAL FANTASY XI'
& tests/bin/zone-rooms/Debug/ZoneRoomTests.exe 'C:/Program Files (x86)/PlayOnline/SquareEnix/FINAL FANTASY XI' --gpu
```

Rooms remain eagerly loaded. Per-room environment effects and portal streaming
remain separate work. These are parser/resource tests, not visual comparisons of
every interior against the retail client.
