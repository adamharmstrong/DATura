# Starter quest markers

DATura draws a gold `!` above NPC nameplates using the official `font font`
punctuation glyph from `ROM/119/51.DAT`. The marker is 56 logical pixels high,
with a dark outline and a gap above the name. It shares the nameplate's distance
scaling, depth occlusion, and isolated D3D9 state. Names and role titles retain
their original position and colors. No marker texture is bundled or invented
from a substitute font. If the official atlas is unavailable, the marker is hidden.

## Version 1 eligibility

The baseline is a fresh level-1 character, fame 1, no completed quests, missions,
licenses, or travel unlocks, with retail expansions enabled. Scope is the eleven
present-day public districts of the three starting nations. Missions, seasonal
events, menus such as Records of Eminence, and distant settlements are outside
this starter-city catalog. Fame-2 quests are not assumed available based on the
character's race or allegiance.

“Available” means the player can begin the quest's introductory interaction now.
It does not mean they can finish it safely or immediately. Required turn-in items,
combat, party formation, and later travel are completion objectives rather than
automatic exclusions. Introductory NPCs count even when a later interaction
formally adds the quest to the quest log. Only one marker appears per NPC.

There are 70 verified NPC assignments: 25 Bastok, 22 San d'Oria, and 23 Windurst.
These remain visible after interaction and zone reloads. No account data is read,
and dialogue does not simulate accepting or completing a quest. Future player
quest tracking should replace the assignment of `Placement::starterQuestAvailable`.

## Research and review

Reviewed September 9, 2026. The starting inventory came from the FFXIclopedia
reputation lists for [Bastok](https://ffxiclopedia.fandom.com/wiki/Bastok_Quests/Reputation_List),
[San d'Oria](https://ffxiclopedia.fandom.com/wiki/San_d%27Oria_Quests/Reputation_List), and
[Windurst](https://ffxiclopedia.fandom.com/wiki/Windurst_Quests/Reputation_List).
Individual quest pages resolve conditions that a fame column cannot capture.
Public [LandSandBoat quest scripts](https://github.com/LandSandBoat/server/tree/base/scripts/quests)
were used as a secondary implementation cross-check, not as proof of retail behavior.
No upstream script code is incorporated into this feature.

Decisions requiring particular care:

- [A Squire's Test](https://ffxiclopedia.fandom.com/wiki/A_Squire%27s_Test)
  requires level 7: Balasiel is excluded.
- [A Discerning Eye](https://ffxiclopedia.fandom.com/wiki/A_Discerning_Eye)
  requires an airship pass: Eddy, Grin, and Pygmalion are excluded.
- [The Dismayed Customer](https://www.bg-wiki.com/ffxi/The_Dismayed_Customer)
  follows A Taste for Meat: Gulemont is excluded.
- [Hoist the Jelly, Roger](https://www.bg-wiki.com/ffxi/Hoist_the_Jelly%2C_Roger)
  requires Cook's Pride: Maysoon is excluded.
- [Eco-Warrior](https://ffxiclopedia.fandom.com/wiki/Eco-Warrior_%28Bastok%29)
  has a combat level cap, not an acceptance minimum. All three starter NPCs are
  included; fresh characters have no conflicting Eco-Warrior quest or weekly completion.
- [Exit the Gambler](https://ffxiclopedia.fandom.com/wiki/Exit_the_Gambler)
  is included for Aurege and Guilberdrier. The current quest page and public
  implementation allow its introduction without completing The Pickpocket;
  the older starter guide is inconsistent on this point.
- [Fully Mental Alchemist](https://www.bg-wiki.com/ffxi/Fully_Mental_Alchemist)
  starts with Titus before the required travel to Grauberg (S).
- [Babban Ny Mheillea](https://ffxiclopedia.fandom.com/wiki/Babban_Ny_Mheillea_%28Quest%29)
  begins in present-day Windurst with Khoto Rokkorah; later travel is an objective.
- [Atelloune's Lament](https://www.bg-wiki.com/ffxi/Atelloune%27s_Lament)
  remains pending and unmarked. Sources list Seeing Spots as its previous quest
  without clearly establishing whether completion is mandatory. This is a known
  coverage uncertainty rather than an assertion that the quest is unavailable.

## Catalog and deployment

Edit `DATura/npc_starter_quests.csv`. Columns are `zone_id`, `entity_id`,
`npc_name`, `quest_name`, `status`, `source_url`, `reviewed_on`, and `notes`.
Quote fields containing commas. `verified` rows with a source and review date
enable markers. `excluded` and `pending` rows document research but never enable
markers. Match the exact zone, ID, and name in `DATura/npc_placements.csv`.
Multiple quests for one NPC still produce one marker.

The build copies the catalog beside DATura.exe. Zone loading reads it beside the
placement catalog; reloading the zone applies edits. Missing catalogs leave NPCs
unmarked. The optional fourth `LoadCatalogForZone` argument supplies another
quest catalog for tools and tests. The placement and renderer fields default to
false, including for players and NPCs created through other APIs.

## Validation

Build `tests/NameplateRenderingTests.vcxproj` (Debug, x64), then run its executable
with the installed FFXI directory and a writable output BMP path. The suite checks
all 70 assignments against real placements, excluded NPCs, identity collisions,
missing catalogs, malformed IDs, CSV quoting, duplicate rows, and zone resets.
GPU tests check the gold glyph, unchanged names/titles, cache isolation, hostile
scene state, and depth occlusion. The extra `<output>.quest.bmp` shows Arawn's
marker rendered through the application nameplate pass.

Validated with the installed retail font: Debug application build and the full
nameplate regression suite pass. The GPU preview was visually inspected.
