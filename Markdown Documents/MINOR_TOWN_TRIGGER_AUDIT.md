# Minor town land-exit repairs

Date: 2026-10-11.

Six installed RID connections were added:

| Source | Token | Destination |
|---|---|---|
| Rabao (247) | z6v0 | Western Altepa Desert (125) |
| Western Altepa Desert (125) | z3h4 | Rabao (247) |
| Kazham (250) | z6y0 | Yuhtunga Jungle (123) |
| Yuhtunga Jungle (123) | z3f0 | Kazham (250) |
| Norg (252) | z700 | Sea Serpent Grotto (176) |
| Sea Serpent Grotto (176) | z4w2 | Norg (252) |

Each town has exactly one active RID exit, now uniquely mapped. The surrounding
field/dungeon networks are only repaired for these specific town returns; this
pass does not claim full coverage of those expansion zones.

Installed DAT centers, sizes, yaw angles, and target arrival IDs are authoritative
for the trigger geometry. Local LandSandBoat zone.yaml supplies DATura's routing
and heading policy. Each receiving DAT contains the specified zero-target arrival
ID, and its center agrees with the reference coordinates within rounding error.
Normals face away from the nearest source arrival, preserving the floating-dot
approach offset and existing height, proximity, floor, and occlusion behavior.

Eight placeholder routes were removed: the six obsolete ordinary land routes
and the invented Kazham/Manaclipper pair. No ferry, airship, or retail access
restriction/event system was implemented. Selbina/Valkurm and Mhaura/Buburimu
remain unchanged.

The six cases extend the existing installed-DAT regression checks for unique
coverage, absence of town placeholders, destinations, arrival positions/headings,
RID target identities, safe arrivals, two-way volume crossing, stationary hits,
wrong floors, and marker approach normals. The audit extraction script accepts
`-SelectedZones 247,250,252` to reproduce the source/reference data.
