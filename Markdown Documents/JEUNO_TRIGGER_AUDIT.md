# Jeuno trigger repairs

Date: 2026-10-10.

Marker floor-filter correction: 2026-10-11. Floating dots now use the actual
RID height instead of also enforcing a fixed four-unit distance from the
volume center. The redundant cutoff hid dots on valid sloped approaches,
including Upper Jeuno's Batallia exit (center Y -4.637, height 10, street
Y 0 inside its vertical bounds). All fourteen Jeuno exits have regression
coverage for marker floor eligibility inside/outside their installed bounds.
Marker position, depth testing, distance fade, and route mappings are unchanged.

All 14 installed active RID exits in Jeuno now have unique local routes:

| Zone | Active exits | Connections |
|---|---:|---|
| Ru'Lude Gardens (243) | 2 | Upper Jeuno; Mog House |
| Upper Jeuno (244) | 4 | Batallia Downs; Lower Jeuno; Ru'Lude Gardens; Mog House |
| Lower Jeuno (245) | 4 | Rolanberry Fields; Upper Jeuno; Port Jeuno; Mog House |
| Port Jeuno (246) | 4 | Sauromugue Champaign; Lower Jeuno; Qufim Island; Mog House |

Four reciprocal field connections were repaired: Batallia Downs z2x4,
Rolanberry Fields z326, Sauromugue Champaign z3c4, and Qufim Island z3i0.
This pass does not claim coverage of every other exit in those field zones.

The 18 placeholder city/field routes were replaced with 18 authored-token
routes. The existing Mog House exit fallback was retained with a corrected
Ru'Lude Gardens arrival. Ordinary destinations and arrival positions/headings
follow the local LandSandBoat data/zones/*/zone.yaml references. Missing
headings retain the local zero default. Installed ROM/1/39.DAT through
ROM/1/42.DAT provide the city trigger bounds and rotations. Field return
volumes come from the corresponding installed field DATs.

Approach/marker normals follow the narrow horizontal volume axis, oriented
away from the closest zero-target arrival record. Loaded RID geometry, not
the compatibility plane, remains authoritative for trigger hits. These routes
participate in the existing diagnostic red-box and nearby-dot rendering;
those renderers' distance, floor, and nearest-marker policies are unchanged.

All four Mog House entrance tokens (zmrm, zmro, zmru, zmrs) route to DATura's
shared Jeuno interior, local zone 0. The application now remembers the city
entrance used and returns outside its volume using the existing residential
return logic. This is local adapter policy: the references' residential
records point back into their city, and are not literal retail server
transitions into zone 0. Arrival inside the local house uses (0,0,-10).
Direct loading of the house without a remembered entrance uses the corrected
Ru'Lude Gardens return position (46.477,9.307,-72.806).

ZoneTransitionTests covers unique complete city-volume mapping, all 18
destinations and arrival references, packet headings, rotated-volume crossings
in both directions and coordinate modes, wrong-floor rejection, safe destination
arrivals, and the geometric remembered-Mog-return offset. The application
return-state integration is compiled but not exercised by this unit test.

Airship boarding, schedules, ticket/access checks, NPC teleports, and event
cutscenes remain separate mechanisms and were not implemented by this repair.
No manual in-game or grounded-collision walkthrough was performed.
