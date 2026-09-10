# Bastok Markets zoning

Game-mode player movement checks the collision-resolved movement segment against
outward crossing planes in `DATura/zone_transition.h`. This catches crossings
between frames without using a proximity radius or triggering while stationary.
Respawns, edit-camera movement, and title/creation scenes do not trigger zoning.
Native positions, entrance facing, and triggers are reflected with the world.

## Routes and sources

LandSandBoat's client-derived zone-line centers and arrivals, retrieved 2026-09-09:

| Markets token | Destination | Return token |
| --- | --- | --- |
| z6j0 | Bastok Mines (234) | z6i2 |
| z6j2 | Port Bastok (236) | z6k2 |
| z6j4 | South Gustaberg (107) | z2z2 |
| z6j6 | Metalworks (237) | z6l0 |

Sources: [Markets](https://github.com/LandSandBoat/server/blob/base/data/zones/bastok_markets/zone.yaml),
[Mines](https://github.com/LandSandBoat/server/blob/base/data/zones/bastok_mines/zone.yaml),
[Port](https://github.com/LandSandBoat/server/blob/base/data/zones/port_bastok/zone.yaml),
[Gustaberg](https://github.com/LandSandBoat/server/blob/base/data/zones/south_gustaberg/zone.yaml),
[Metalworks](https://github.com/LandSandBoat/server/blob/base/data/zones/metalworks/zone.yaml).

The table does not provide source trigger volumes. The local implementation uses
explicit corridor normals, half-widths, and a four-unit vertical tolerance around
the published source centers. Destination `scale` is not treated as source trigger
size. Retail MAPRECT decoding and edge-of-corridor visual verification remain
future work. Mog House tokens `zmra`/`zmrc` require instanced interior handling;
their same-zone return positions are not treated as destinations.

## Loading and failure behavior

The existing zone loader refreshes geometry, doors, NPCs, selection/chat, weather,
and collision. Zoning restores game mode and camera distance/pitch, preserves the
player asset and equipment, grounds the arrival locally when possible, and resets
respawn/last-safe points. The authored inward facing and arrival offset keep the
player inside the destination's return threshold; no timer delays walking back.

Unresolvable destinations leave the player before the crossing. A parse failure
attempts to reload the source scene and restores the previous player state. Failed
routes suppress repeated attempts until the player retreats two units inside the
threshold or loads a zone. The shared loader reports errors.

## Verification

Build and run `tests/ZoneTransitionTests.vcxproj` (Release/x64). Its executable is
`tests/bin/zone-transition/Release/ZoneTransitionTests.exe`. Checks cover eight
routes in both coordinate modes, inward/stationary movement, wrong zones/floors,
corridor misses, arrival facing, and arrivals inside the return threshold.

Build `tests/ZoneTransitionCollisionTests.vcxproj` and run
`tests/bin/zone-transition-collision/Release/ZoneTransitionCollisionTests.exe`
with the FFXI installation directory. This loads the five actual zone DATs and
walks the player through each threshold using collision in both coordinate modes.

Manual check: load Bastok Markets, enter game mode, walk through each exit and
back, and verify the label, entrance, facing, NPCs, music, and equipment. Also
check corridor edges and jumping. The automated collision check does not exercise
the windowed zone-loader flow or establish exact retail trigger bounds.
