# Remaining mainland exit-trigger repairs

Date: 2026-10-11.

All 50 active installed RID exits across the fifteen zones listed below now
have exactly one DATura route. This pass added 43 missing source routes and
11 missing peripheral return routes, retaining the 14 already-correct routes
among the 68 reference cases. Thirty-three obsolete source/return placeholders
were removed. The shared route table now contains 503 routes.

| Zone | Active exits verified |
|---|---:|
| Pashhow Marshlands (109) | 3 |
| Beaucedine Glacier (111) | 10 |
| Xarcabard (112) | 3 |
| Meriphataud Mountains (119) | 4 |
| Beadeaux (147) | 4 |
| Qulun Dome (148) | 3 |
| Castle Oztroja (151) | 3 |
| Altar Room (152) | 2 |
| Upper Delkfutt's Tower (158) | 4 |
| Castle Zvahl Baileys (161) | 2 |
| Castle Zvahl Keep (162) | 2 |
| Throne Room (165) | 1 |
| Fei'Yin (204) | 5 |
| Qu'Bia Arena (206) | 2 |
| Gustav Tunnel (212) | 2 |

## Sources and scope

Installed zone model DATs provide the RID IDs, target arrival IDs, centers,
rotations, and dimensions. Local LandSandBoat data/zones/*/zone.yaml supplies
DATura's destination zone and arrival-heading policy. Each of the 68 reference
cases was checked against the receiving DAT: its RID target names a zero-target
arrival record there, whose center matches the selected arrival coordinates.
This is not a reconstruction of retail server access restrictions or events.

The eighteen reciprocal peripheral cases are in Uleguerand Range (one),
Pso'Xja (six), Valkurm Dunes, Batallia Downs, Konschtat Highlands, Rolanberry
Fields, Cape Teriggan, Tahrongi Canyon, Sauromugue Champaign, the Sanctuary of
Zi'Tah, Ranguemont Pass (one each), and Cloister of Frost (two). Only those
specific returns are covered in partially audited neighboring expansion zones.
No claim is made that the rest of those expansion-zone networks are repaired.

Upper Delkfutt's Tower's four active exits are same-zone crossings between
sections of its geometry, not ordinary exits to Middle Delkfutt's Tower.
These preserve four distinct receiving records, floors, and headings. The
invented Upper/Middle ordinary crossing was removed. Other removed placeholders
include the incorrect Qu'Bia/Shrouded Maw, Gustav/Valley of Sorrows, and
Xarcabard/Throne Room [S] ordinary links.

## Floating dots and verification

The routes resolve the installed volumes used by both the red debug boxes and
floating-dot renderer. Approach normals use the thin horizontal axis oriented
away from the nearest source arrival record. The existing dot height, depth
testing, near-face offset, proximity/floor filtering, and nearest-row selection
are unchanged. This repairs eligibility, not a guarantee that every row is
visible from every position or through walls.

ZoneTransitionTests now verifies all 50 exits have unique mappings, no source
placeholders remain, and all 68 references have correct destinations, arrivals,
and encoded headings. Tests additionally check RID target identities, safe
arrivals outside destination exits, volume crossings from both directions,
stationary containment, wrong-floor rejection, and dot-normal/approach-side
alignment in both coordinate modes. Generic fallback arrival checks now exclude
passages on another floor, needed for vertically stacked Delkfutt sections.

The complete zone-transition regression suite passes for 503 routes in both
coordinate modes. The Release x64 application build succeeds. No manual
in-game or grounded-collision walkthrough was performed.

The read-only tools/audit_mainland_exit_records.ps1 script extracts the installed
source/receiving RID data and the local reference cases used for this audit.

