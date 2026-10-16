# Windurst-adjacent trigger repairs

All 45 installed active RID exits in the following eleven zones have unique
destination mappings. This repairs the 41 missing mappings from the audit
while retaining the four working city return connections.

| Zone | Active exits |
| --- | ---: |
| West Sarutabaruta | 9 |
| East Sarutabaruta | 7 |
| Tahrongi Canyon | 4 |
| Buburimu Peninsula | 5 |
| Giddeus | 2 |
| Balga's Dais | 2 |
| Outer Horutoto Ruins | 4 |
| Inner Horutoto Ruins | 3 |
| Toraimarai Canal | 4 |
| Full Moon Fountain | 2 |
| Maze of Shakhrami | 3 |

Nine reciprocal connections are mapped: four from Windurst city and five
from Meriphataud Mountains, Mhaura, Labyrinth of Onzozo, Bibiki Bay and
Attohwa Chasm. Only those return connections in the five peripheral areas
are covered by this pass, not every exit in those areas.

Destination and arrival data come from local LandSandBoat
data/zones/*/zone.yaml files. Source dimensions and rotations were checked
against installed DAT RID records. Marker directions are oriented away from
the closest paired arrival into the source passage. Unspecified destination
headings retain the local zero default. These are DATura destination policies,
not a claim to reconstruct retail server events or access restrictions.

Placeholder source-zone routes were replaced, including tokens that previously
collided with unrelated region routes. The shared strict RID hit logic is
unchanged. All newly mapped exits qualify for the existing nearby floating-dot
renderer; its nearest-row selection and distance/floor filtering are unchanged.

ZoneTransitionTests checks unique active-volume coverage, all 54 mappings and
authored crossings in both coordinate modes, wrong-floor rejection, and
destination containment to ensure arrivals do not immediately zone again.
Existing Bastok, San d'Oria, Windurst-city and Tower portal tests also pass.
No manual in-game or grounded-collision walkthrough was performed in this pass.
The glowing Tower portal asset is not addressed by these route repairs.
