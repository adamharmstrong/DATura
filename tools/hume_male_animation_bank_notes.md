# Hume Male Animation Bank Notes

These notes are derived from `tools/ffxi_animation_bank_candidates.csv`,
`tools/ffxi_animation_catalog.csv`, and `tools/ffxi_schedule_catalog.csv`.

AltanaView appears to use a two-level selector:

1. Action category, such as `General`, `Battle`, `Sword`, `Dagger`, or `Emote`.
2. A concrete animation bank inside that category, such as `Battle - 1`.

Each concrete bank is a DAT containing:

- type `0x07` schedule chunks
- type `0x2B` motion chunks

The UI count shown by AltanaView appears to count display names, not raw chunks. Raw `0x2B`
motion chunks frequently come in split-body pairs, such as `wlk0` + `wlk1`, which AltanaView
collapses to `wlk`.

## Confirmed Matches

| AltanaView context | DAT | Schedule chunks found | Motion display names found | Notes |
| --- | --- | ---: | ---: | --- |
| General | `ROM/27/82.DAT` | 67 raw, likely 66 UI-visible | 70 | Matches the screenshot showing `66 Schedules and 70 Motions`. One schedule is probably hidden/internal. |
| Battle - 1 | `ROM/32/13.DAT` | 18 | 29 | Matches the screenshot showing `18 Schedules and 29 Motions`. |

## Likely Battle Banks

These are contiguous Hume Male banks with the same battle schedule family as `ROM/32/13.DAT`.
They likely correspond to AltanaView's `Battle - 1`, `Battle - 2`, etc. entries, but the exact
label-to-DAT index still needs visual confirmation.

| Likely UI bank | DAT | Schedules | Motion names | Distinguishing motion names |
| --- | --- | ---: | ---: | --- |
| Battle - 1 | `ROM/32/13.DAT` | 18 | 29 | `ind`, `inb`, `otd`, `otb` |
| Battle - 2 | `ROM/32/14.DAT` | 16 | 27 | `inb`, `otb` |
| Battle - 3 | `ROM/32/15.DAT` | 22 | 33 | `in0`, `ina`, `ot0`, `ota`, `wa4`, `wa5`, `at3`, `at4` |
| Battle - 4 | `ROM/32/16.DAT` | 16 | 27 | `inc`, `otc` |
| Battle - 5 | `ROM/32/17.DAT` | 16 | 27 | `ine`, `ote` |
| Battle - 6 | `ROM/32/18.DAT` | 16 | 27 | `inf`, `otf` |
| Battle - 7 | `ROM/32/19.DAT` | 16 | 27 | `ing`, `otg` |
| Battle - 8 | `ROM/32/20.DAT` | 16 | 27 | `ini`, `oti` |
| Battle - 9 | `ROM/32/21.DAT` | 15 | 26 | `ink`, `otk` |

## Remaining Work

- Identify the DAT for AltanaView's `Battle - 10` and dual-wield banks.
- Map the action category names (`Hand-to-Hand`, `Dagger`, `Sword`, etc.) to DAT ranges.
- Decode the type `0x07` schedule binary format beyond embedded tokens. The current scanner
  extracts motion references and helper/action tokens, but not timing, branching, or event fields.
- Confirm whether the one extra raw schedule in `ROM/27/82.DAT` is hidden/internal or counted
  differently by AltanaView.
