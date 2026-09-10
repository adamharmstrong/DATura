# Installed water asset probe

Read-only inspection of installed DATs at `C:\Program Files (x86)\PlayOnline\SquareEnix\FINAL FANTASY XI` on 2026-09-09. No reference application or game-file writer was run. `probe_water_assets.py` imports DATura's existing diagnostic parsers and writes `water_assets.json` and a generator summary here. The bundled Python runtime is required for the imported diagnostic's Pillow dependency.

## Minimal confirmed surface family

| Zone | DAT | Permanent autorun generators | Resource scope | Texture |
|---|---|---|---|---|
| East Ronfaure | ROM/0/121.DAT | 44 placements using 36 `ka1..ka22` / `kb1..kb14` resources | Generators `f_ro/effe/kawa`; resources `f_ro/effe` | `effect  kaw1` |
| Valkurm Dunes | ROM/0/102.DAT | 11 placements: `umi0` x3, `umif` x6, `ukro` x2 | Generators `f_ki/effe/umi1`, `umi2`, `umi3`; resources `f_ki/effe` | `effect  umi0` |
| Bastok Markets | ROM/1/35.DAT | `alse` -> `alls`, `lows` -> `lows` | `t_ba/effe/sea` | `sea     sea01` |

All these generators have StandardParticleSetup data type 11, lifetime zero, autorun bit 0x10, attachment zero, no batched-weather flag. Generator `particlesPerEmission` is zero for these permanent surfaces; do not require it to be positive as precipitation does. Permanent lifetime is zero, not a missing/invalid record.

Resolve MMB resource chunk IDs from the generator's own directory, then ancestor directories, stopping within the effect scope. Exact-directory matching misses both East Ronfaure and Valkurm. The MMB object's embedded 16-byte name may differ from the chunk ID: `alls` is `allsea`, `lows` is `lowsea`, `allc` is `allcol`, and `rvco` is `rvcol`.

Restrict a first surface implementation to known textured surface families after proving scoped generator linkage. East Ronfaure also contains `kt01` on `effect  tak1` (waterfall overlay) and mist on other element types; Valkurm contains finite-lifetime `nmia`, `nmib`, `nmic`, `nmsa`, etc. that require lifetime/curve playback. Bastok's `lowcol`, `allcol`, `mizu`, `rvcol`, and `funmiz` are untextured color layers requiring separate validation. Directory membership or autorun alone admits nonwater and incomplete layers.

## Required parsing/transform distinctions

- Water setup commands 0x60..0x63 use a 16-byte form with the four-character curve ID at **opcode + 8**, not +4. Example East Ronfaure ka01: `63 04 00 00 00 00 00 00 74 6b 77 61 01 00 00 00` refers to `tkwa` in its own `kawa` directory. Valkurm uses different `trgb`/`trgg` curves in each `umi1/2/3` directory.
- Updater stream 0x27 and 0x28 supply U/V scroll floats at opcode +4. Do **not** copy XI-Test-Client's heuristic that any 0x27 means a hidden plane. Bastok `alse` has U -0.00063, V +0.0008 per authored frame; `lows` has U +0.001, V +0.0012. These are ordinary scroll commands.
- Preserve zero components in scale. Valkurm base ocean planes are authored with scale `(6,0,6)` and local Y=0. Substituting 1 for zero changes authored transforms, even if a flat Y0 surface happens to look the same.
- Preserve full SRT in native Y-down coordinates. Bastok `lows` uses scale100 and yaw -1.5707494. East `ktb1` uses yaw -1.5707481 with nonunit X scale.
- No sampled base-surface stream had meaningful operations after a zero-op marker; a generalized NOP parser change is not required by these fixtures alone.

## Validation fixtures

East Ronfaure generator `f_ro/effe/kawa/ka01` is at `(420,-48.0862427,180)`, scale1, rotation0, maps to `ka1`, 94 vertices / 82 valid triangles. Its world bounds are `(416,-48.0862427,160)` to `(424.0269198,-48.0862427,184)`. V scroll is -0.002 per authored frame (-0.12/s at the existing 60Hz convention). A possible elevated camera is `(420,-58,194)` looking at `(420,-48,172)`.

East `kb09` -> `kb9` is at `(260,-40.1331635,100)` and contains sloping water geometry: world bounds `(255.54018,-48.1331635,80)` to `(264.37528,-38.1331635,120)`. It has 144 vertices / 113 valid triangles and V scroll -0.003/frame. Do not flatten rivers to a universal water height. Two extra placements (`kb15` -> `kb14`, `kb16` -> `ka6`) share transforms with other placements but have different generator colors; do not deduplicate on geometry/position alone.

Valkurm `f_ki/effe/umi1/uma1` -> `umi0` is at `(294.3991699,3.9561744,-202.2947083)`, scale `(6,0,6)`, zero rotation. Across two MMB batches it has 398 vertices / 523 valid triangles and approximate world bounds `(219.41777,3.9561744,-235.89477)` to `(404.93777,3.9561744,-164.254707)`. V scroll is +0.002/frame. A possible elevated shore camera is `(295,-6,-155)` looking at `(295,3.956,-205)`.

Bastok `alse` -> `allsea` is at `(-220,0,-100)` with scale100. Its geometry contains multiple native Y levels, not one harbor plane. XI-Test-Client comments report drawing it over an auction-house floor before disabling it using the uncertain 0x27 heuristic. This needs an independent local terrain/depth comparison; the presence of a scroll command cannot decide visibility. Use East Ronfaure and Valkurm as primary first-pass correctness fixtures.

These observations establish parser, placement, and asset facts. They do not establish retail visual fidelity, correct missing color-layer composition, or complete shoreline/fountain support.
