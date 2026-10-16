# San d'Oria-adjacent trigger repairs

Date: 2026-10-10.

All 67 installed active RID exits across the following sixteen zones now
resolve to exactly one local route. Seven previously repaired routes remain
unchanged; 60 missing exits and four peripheral returns were added.

| Zone | Active exits |
|---|---:|
| West Ronfaure (100) | 8 |
| East Ronfaure (101) | 6 |
| La Theine Plateau (102) | 6 |
| Valkurm Dunes (103) | 4 |
| Jugner Forest (104) | 6 |
| Horlais Peak (139) | 2 |
| Ghelsba Outpost (140) | 3 |
| Fort Ghelsba (141) | 4 |
| Yughott Grotto (142) | 6 |
| Davoi (149) | 6 |
| Monastic Cavern (150) | 5 |
| Ranguemont Pass (166) | 2 |
| Bostaunieux Oubliette (167) | 2 |
| King Ranperre's Tomb (190) | 3 |
| Ordelle's Caves (193) | 3 |
| Selbina (248) | 1 |

The four peripheral returns are Carpenters' Landing z020/z022 to Jugner
Forest, Beaucedine Glacier z330 to Ranguemont Pass, and Gustav Tunnel z5w2
to Valkurm Dunes. Other exits in these three peripheral zones are not
claimed as completely audited by this pass.

Thirty-one source-zone placeholders were removed, including invented
East Ronfaure/Jugner, West Ronfaure/Ranperre's Tomb, Fort Ghelsba/Horlais
Peak, Valkurm/Mhaura, and direct Jugner/Phanauet Channel ordinary crossings.
The shared table now contains 481 routes. Distinct entrances into the same
dungeon remain distinct routes rather than being collapsed into one.

## Data and rendering

Installed zone DAT RID tokens, dimensions, centers, and rotations supply
the authoritative crossing volumes. The existing shared volume implementation
follows the PS2 hit semantics documented in
[Authored zone triggers](AUTHORED_ZONE_TRIGGERS.md).
Destinations and arrival coordinates/headings follow local LandSandBoat
data/zones/*/zone.yaml references. These references provide DATura's routing
policy, not reconstructed retail server access or event policy.
Omitted headings retain the existing zero default.

Approach normals use the narrow horizontal volume axis, oriented away from
the closest zero-target arrival record. Floating dots and diagnostic exit
boxes use the existing renderers; marker height, depth testing, approach
offset, range, floor filtering, and nearest-row selection are unchanged.

## Verification and limits

ZoneTransitionTests verifies complete unique mapping of all 67 active exits,
all 71 destination and arrival references (including peripheral returns),
encoded headings, crossings from both sides in both coordinate modes,
wrong-floor rejection, and arrivals outside every active destination exit
volume. Existing city and other region checks remain passing.
The complete regression suite passes for 481 routes in both coordinate modes,
and the Release x64 application build succeeds.

No manual in-game or grounded-collision walkthrough was performed.
Selbina ferry boarding, the Phanauet barge, battlefield entry events,
quest/access restrictions, and the event-controlled Chateau entrance remain
separate systems. Removing invented ordinary crossing placeholders does not
implement those transport or event mechanisms.
