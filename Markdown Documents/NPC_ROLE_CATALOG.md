# NPC role titles

`DATura/npc_roles.csv` is the editable, unofficial role database. It covers every
entry in `npc_placements.csv`, including interactive objects and placeholders.
Titles are editorial descriptions of documented FFXI roles, not client DAT fields.
They describe the original NPC's role; they do not imply that DATura implements
the corresponding shop, quest, or service yet.

The initial research prioritizes Bastok Mines (234), Bastok Markets (235), Port
Bastok (236), and Metalworks (237). There are 225 verified entries across those
four zones. Other entries remain pending rather than receiving guessed titles.
Sources and review dates are recorded per entry. Generic quest titles can be
refined as individual quests are researched; they do not indicate current quest
availability. Conflicting or unclear roles should remain pending.

## Editing

Open the CSV in a text editor or import it into a spreadsheet as UTF-8 comma-separated
text. Preserve the header and save as UTF-8 CSV. Each record must stay on one line;
quoted commas and doubled quotation marks are supported, embedded newlines are not.

| Column | Meaning |
| --- | --- |
| zone_id | Numeric FFXI zone ID; part of the identity |
| zone_name | Human-readable zone name for filtering |
| entity_id | Numeric placement ID; part of the identity |
| npc_name | Exact placement name; checked to prevent stale assignments |
| title | Short, player-facing role, at most 128 UTF-8 bytes |
| status | `pending`, `verified`, or `not_applicable` |
| source_url | Page supporting the role; required for verified titles |
| reviewed_on | Research date, `YYYY-MM-DD`; required for verified titles |
| notes | Uncertainty, additional roles, or editorial context |

Filter by zone and `pending` to work through the research queue. Use service-specific
titles where supported (for example `Alchemy Guildmaster` or `Equipment Storage`).
Use `not_applicable` for placeholders or objects that should have no role label.
Do not classify a named NPC as a resident solely because its service is unknown.
Two NPCs with the same name retain separate records and may have different titles.

From the repository root:

```powershell
python tools/update_npc_roles.py
python tools/update_npc_roles.py --sync
```

The first command validates identities, research fields, and complete placement
coverage, then reports progress. The second adds new placement records while
preserving existing research. Removed or renamed placements require manual review;
the tool reports them instead of silently reassigning their titles.

## Runtime

The build copies the CSV alongside the executable and `npc_placements.csv`.
Zone loading joins the role data by zone ID, entity ID, and exact NPC name.
Only nonempty, sourced `verified` titles appear. Missing role data leaves the
existing nameplate intact. Reload the zone after editing the deployed CSV, or
rebuild to deploy changes from the source CSV.

Names remain green and bold; roles appear centered underneath in smaller, pale
text with an outline. Unresearched NPCs have a single-line nameplate. Selection
markers apply to the name. Rendering caches both name and title, so namesakes
with different roles do not share the wrong label.
