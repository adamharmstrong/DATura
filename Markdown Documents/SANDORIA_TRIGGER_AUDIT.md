# San d'Oria RID zoning repairs

All 13 active exits are mapped: Southern San d'Oria (4), Northern San d'Oria
(6), Port San d'Oria (2), Chateau d'Oraguille (1). Installed DAT resources are
ROM/1/31.DAT, ROM/1/32.DAT, ROM/0/113.DAT and ROM/1/33.DAT respectively.
The three zmr exits use DATura's shared San d'Oria Mog House (300), preserving
the entry zone and returning outside the same entrance. Six reciprocal routes
from East/West Ronfaure, Carpenter's Landing and Bostaunieux Oubliette are mapped.

RID bounds and yaw come from installed DATs. Destination positions and headings
come from local LandSandBoat data/zones/*/zone.yaml, with zero as the default for
unspecified headings. These are local routing policies, not retail server logic.

The Northern San d'Oria Chateau gate is NOT an active RID exit. The reference
Northern_San_dOria/Zone.lua registers an event region from (-7,-3,110) to
(7,-1,155), with mission/rank access checks; event 569 moves to (0,0,-13) in
zone 233. This repair removes the invented ordinary gate plane but does not
implement or bypass that separate event-controlled access system. Airship
transport is likewise outside this RID mapping pass.

ZoneTransitionTests verifies unique coverage of all 13 active records,
destinations and bidirectional authored crossings in both coordinate modes,
plus the six reciprocal routes. Manual in-game validation remains outstanding.
