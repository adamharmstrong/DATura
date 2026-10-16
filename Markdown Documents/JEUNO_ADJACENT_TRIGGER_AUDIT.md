# Jeuno-adjacent trigger repairs

Date: 2026-10-10.

This pass repairs 22 previously unsupported exits in Jeuno's four surrounding
fields and 18 reciprocal returns from adjoining zones. The four existing
returns into Jeuno remain unchanged.

| Field | Installed active exits | Connections |
|---|---:|---|
| Batallia Downs (105) | 12 | Upper Jeuno; Jugner Forest; Rolanberry Fields; Beaucedine Glacier; eight Eldieme Necropolis entrances |
| Rolanberry Fields (110) | 6 | Lower Jeuno; Pashhow Marshlands; Batallia Downs; Sauromugue Champaign; two Crawlers' Nest entrances |
| Sauromugue Champaign (120) | 5 | Port Jeuno; Meriphataud Mountains; Rolanberry Fields; two Garlaige Citadel entrances |
| Qufim Island (126) | 3 | Port Jeuno; Behemoth's Dominion; Lower Delkfutt's Tower |

All 26 installed active exits in these fields now resolve to exactly one route.
The 18 reciprocal returns are in Jugner Forest, Beaucedine Glacier, the Eldieme
Necropolis, Pashhow Marshlands, Meriphataud Mountains, Crawlers' Nest, Garlaige
Citadel, Behemoth's Dominion, and Lower Delkfutt's Tower. Other exits in those
nine adjoining zones are outside this pass.

Forty authored-token routes replace 18 old placeholder routes. These include
removing the fictitious direct Qufim/Middle Delkfutt's Tower connection: the
installed Qufim records connect to Lower Delkfutt's Tower instead.
The total route table now contains 448 entries.

## Sources and behavior

Trigger geometry and tokens come from the installed zone DAT RID records.
The existing decoder and rotated-volume hit tests follow the PS2 client
implementation described in [Authored zone triggers](AUTHORED_ZONE_TRIGGERS.md).
Destination zones and arrival positions/headings come from the local
LandSandBoat data/zones/*/zone.yaml references, not from claims that the
PS2 client supplies retail server arrival policy. Omitted headings retain
the existing zero default.

Field DATs: ROM/0/122.DAT, ROM/0/126.DAT, ROM/1/2.DAT, ROM/0/58.DAT.
Reciprocal DATs: ROM/0/114.DAT, ROM/0/72.DAT, ROM/0/77.DAT, ROM/0/125.DAT,
ROM/0/101.DAT, ROM/0/76.DAT, ROM/1/17.DAT, ROM/1/3.DAT, ROM/1/13.DAT.

Loaded RID volumes remain authoritative for crossings. Compatibility planes
and floating-dot approach normals use the narrow horizontal volume axis,
oriented away from the nearest zero-target arrival record. Existing marker
height, depth testing, approach offset, range, floor, and nearest-line policies
are unchanged. These routes also use the existing exit-volume debug rendering.

## Verification

ZoneTransitionTests passes for all 448 routes in both coordinate modes.
Installed-DAT regression checks cover complete unique mapping of the 26 field
exits, the 40 repaired destinations and arrival references, encoded headings,
crossings from either side, wrong-floor rejection, and arrival positions outside
every active destination exit volume. The existing four Jeuno-return fixtures
remain passing. The Release x64 application build also succeeds.

No manual in-game or grounded-collision walkthrough was performed.
Quest/access restrictions, airship boarding, NPC teleports, and cutscene logic
are separate from this local route repair.
