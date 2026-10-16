# Bastok-adjacent trigger repairs

The 40 active RID exits in North Gustaberg, South Gustaberg, Konschtat
Highlands, Dangruf Wadi, Palborough Mines, Zeruhn Mines, Korroloka Tunnel,
Gusgen Mines, Oldton Movalpolos and Waughroon Shrine now have destination
routes. Placeholder source-zone routes were replaced, including the incorrect
South Gustaberg `z2z4` destination (North Gustaberg, not Konschtat).

Ten reciprocal connections from Newton Movalpolos, Pashhow Marshlands,
Valkurm Dunes and the Altepa deserts were also mapped. Other exits in those
five peripheral zones were not audited or repaired in this pass.

Destination positions and optional headings come from the local extracted
LandSandBoat `data/zones/*/zone.yaml` files. Absent headings retain the local
zero-heading default. Installed DAT RID volumes provide the actual hit bounds;
PS2 hit semantics remain in the shared volume implementation. These mappings
are DATura destination policy, not a reconstruction of retail server zoning.

`ZoneTransitionTests`, supplied the installed game root, checks all 40 volumes
have exactly one route and verifies the 50 destination mappings and authored
crossings in both coordinate modes. Existing 14-city-exit checks remain enabled.
No manual in-game walkthrough was performed.
