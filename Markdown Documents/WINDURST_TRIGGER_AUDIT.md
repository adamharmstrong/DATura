# Windurst zoning repairs

All 16 active RID exits in Waters, Walls, Port and Woods now have routes,
including four local Mog House entrances (shared zone 302). The shared house
remembers which entrance was used and returns outside that volume. Four return
routes from West/East Sarutabaruta and Toraimarai Canal are mapped as well.
Placeholder city routes were removed. Destination positions/headings are from
the local LandSandBoat zone.yaml files; unspecified headings default to zero.
Installed DAT bounds/yaw remain authoritative for the RID hit geometry.

Heavens Tower has no active RID exits. Local adapters use the reference script
event regions rather than fabricated ordinary zone planes:

- Walls entrance: (-2,-17,140) to (2,-16,142); reference event 86 arrives in
  Tower at (0,0,-22.4), packet heading 192.
- Tower exit: (-1,-1,-35) to (1,1,-33); reference event 41 arrives in Walls
  at (0,-17,135), packet heading 60.

These regions are checked separately from retail RID logic, with 0.001 units
of boundary tolerance for event floor contact. They are also available to
marker rendering and are appended to the loaded diagnostic exit-volume list
for red-box visualization. This is DATura's direct local transition adapter;
it does not replay the retail portal cutscenes or server event protocol.
The internal Tower upper-floor event-83 warp and airship transport are not
part of these inter-zone repairs. Door/quest access restrictions are unchanged.

ZoneTransitionTests checks all 16 exits have unique mappings, all 20 RID routes
against installed volumes in both coordinate modes, and both event portals
for crossing, floor contact, width/height rejection and safe arrivals. No
manual in-game or grounded-collision walkthrough was performed in this pass.
