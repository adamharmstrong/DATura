# Four-zone Bastok trigger audit

Audited the installed ROM/1/34–37.DAT RID resources using the PS2 0x36
resource layout and ordinary `ZoneRectHitCk` filtering. Each city has one
inactive target/arrival record per active trigger; zero-target records do not
initiate transitions. Counts below include only active zoning triggers.

| Zone | Active tokens | Destinations |
| --- | --- | --- |
| Bastok Mines (234) | z6i0, z6i2, zmrg, z6i4 | South Gustaberg, Markets, Mog House, Zeruhn Mines |
| Bastok Markets (235) | z6j0, z6j4, z6j2, z6j6, zmra, zmrc | Mines, South Gustaberg, Port, Metalworks, two Mog House entrances |
| Port Bastok (236) | zmri, z6k0, z6k2 | Mog House, North Gustaberg, Markets |
| Metalworks (237) | z6l0 | Markets |

Corrected z6i4's destination from South Gustaberg to Zeruhn Mines and added
missing z6i0, zmrg, and zmri routes. Replaced the fabricated North Gustaberg
route token with z6k0 and added/corrected reverse routes z2z0, z4s0, and z2y0.
Ordinary exit arrivals were checked against the local client-derived
LandSandBoat zone.yaml records. Residential routes use DATura's shared local
Mog House (301); the application remembers the source entrance and returns
outside that trigger, facing into the city. This remains local interior policy.

All 14 active records map to exactly one destination route. Geometry tests
check both mirror modes, bidirectional volume hits, and wrong-floor rejection.
Collision tests load each city's actual DAT geometry, ground a player on the
approach side, and simulate walking into every trigger. All 28 cases grounded
and triggered successfully. The Release application builds successfully.

This is a data/collision audit, not a manual visual walkthrough. Airship
boarding is scheduled transport/event behavior, not an ordinary active RID
zone line in these four city resources. The PS2 interpolation sampling
difference described in AUTHORED_ZONE_TRIGGERS.md still applies.
