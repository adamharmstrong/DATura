# Base-game dotted zone-line visibility audit

Date: 2026-10-11. Read-only audit of runtime behavior; no marker or trigger fixes applied.

## Scope and method

The previously connected original/pre-Zilart source zones contain 286 active
RID exits. All have unique local route mappings. Current installed DATs also
include passages leading to later expansion areas; those source exits are
included, but the expansion destination networks are not claimed as audited.

The record-level script mirrors the current renderer's near-face normal,
width, strict authored-height filter, 35-unit fade distance, and nearest-row
selection. Nine approach samples per exit (5/15/30 units before the near face,
center and +/-40% lateral width) use the nearest source arrival's Y as a floor
reference. 2,565 of 2,574 samples select a row; the nine failures all concern
Ru'Lude Gardens zmrm. This reference-height check alone is not ground truth.

The collision diagnostic therefore also loads the same referenced and
unreferenced collision triangles used by environment loading. For each exit it
samples the center approach at 3, 10, and 20 units before the near face. It
chooses the intersecting floor nearest the source arrival height within 20
vertical units. It separately probes the floor beneath the center dot at the
near-face offset of 1.5 units. This is a static geometry probe, not a grounded
movement simulation, camera-based visibility test, or retail walkthrough.

Heaven's Tower htin/hout/hret are intentional glowing-circle replacements,
not missing ordinary dots, and are excluded from the 286-volume count.
Ferries, airships, and non-RID event travel are excluded. Normal UI visibility,
game mode, transition state, and the Config marker toggle must allow drawing.

## Findings

- No missing or duplicate ordinary source-route mappings.
- No reversed approach normals relative to the nearest source arrival.
- Valkurm z2v0's old route direction has an alignment of about 0.972 with
  the DAT narrow axis. The renderer normalizes to the actual DAT axis, so this
  is not itself a misplaced-dot failure.
- Nine far-approach samples in Upper/Lower Jeuno select another nearby exit's
  row. Only one nearest row is rendered by design. This does not prove either
  exit is unmapped or never draws; all those exits select their own row at the
  closer sampled approach distances.
- 98 of 858 collision approach samples fail the vertical eligibility filter,
  across 74 exits in 37 zones. Some farther samples are legitimately outside
  the exit's vertical range; they are not all confirmed defects.
- 41 otherwise floor-eligible approach samples put the center dot below the
  sampled floor at the row, across 40 exits in 24 zones. This follows directly
  from setting dot Y to player Y minus one while keeping its X/Z fixed at the
  exit. Sloped floors can therefore hide eligible dots via depth testing.
- 24 samples have no suitable floor (23 exits). Straight approach samples may
  leave a winding corridor or hit a geometry gap; these remain inconclusive.

## Near-exit height-filter concerns

These twelve exits reject the sampled collision floor only three units before
the near face. Values are native DAT Y (increases downward). They warrant
checking marker approach-floor policy separately from the actual trigger bounds.
Changing the marker policy must not enlarge the zoning volumes.

| Source zone | Token | Sampled floor Y | Trigger vertical bounds |
|---|---|---:|---|
| Valkurm Dunes | z2v4 | -2.50762 | -12.5288,-2.52885 |
| Buburimu Peninsula | z3a0 | -44.8255 | -55.0814,-45.0814 |
| Upper Delkfutt's Tower | z4e4 | -49.2758 | -46.2118,-38.2118 |
| Castle Zvahl Baileys | z4h2 | -33.5 | -44.4968,-34.4968 |
| Bostaunieux Oubliette | z4n2 | -7.25 | -15.7225,-7.7225 |
| Inner Horutoto Ruins | z5c4 | -8.75 | -16.9916,-8.99162 |
| Inner Horutoto Ruins | z5ca | -10.5 | -19.0569,-11.0569 |
| Outer Horutoto Ruins | z5e0 | -8.75 | -16.8661,-8.86608 |
| Outer Horutoto Ruins | z5e2 | -8.75 | -17.1053,-9.10525 |
| Outer Horutoto Ruins | z5e8 | -8.75 | -16.9925,-8.99251 |
| Northern San d'Oria | z6fa | 12 | 7.96575,11.9657 |
| Ru'Lude Gardens | zmrm | 10.0015 | 12.1902,22.1902 |

Ru'Lude Gardens zmrm is especially clear: its source arrival Y is 9.307;
nearby sampled floors are about 9–10, while the exit's vertical range is
12.190–22.190. The current strict marker-height filter rejects every tested
approach. This audit does not establish whether the trigger itself becomes
reachable after moving farther down the passage.

Northern San d'Oria z6fa leads to Carpenters' Landing, not the Mog House.

## Eligible dots potentially buried by sloped floors

| Source zone | Tokens flagged |
|---|---|
| East Ronfaure | z2ta |
| Valkurm Dunes | z2v6 |
| Jugner Forest | z2w4 |
| North Gustaberg | z2yc, z2ye |
| Konschtat Highlands | z300 |
| Beaucedine Glacier | z338, z33c, z33e, z33g, z33i |
| West Sarutabaruta | z37a, z37c, z37e |
| East Sarutabaruta | z388, z38a, z38c |
| Buburimu Peninsula | z3a6 |
| Palborough Mines | z3z4 |
| Davoi | z452, z454, z456, z458 |
| Monastic Cavern | z462, z466, z468 |
| Castle Zvahl Baileys | z4h2 |
| Outer Horutoto Ruins | z5e6 |
| The Eldieme Necropolis | z5fc, z5fe |
| Garlaige Citadel | z5k0, z5k2 |
| Chateau d'Oraguille | z6h2 |
| Bastok Mines | z6i4 |
| Windurst Waters | zmr6 |
| Windurst Walls | zmr8 |
| Port Windurst | z6o2 |
| Windurst Woods | zmrk |
| Upper Jeuno | z6s4 |
| Lower Jeuno | z6t2 |

These are center-dot collision-surface findings. The rendered surface can
occasionally differ from collision/helper geometry; no camera walkthrough was
performed. Testing lateral dots and complete paths remains useful before a fix.

## Reproduction

Record/selection audit:

```powershell
./tools/audit_base_zone_markers.ps1 -GameRoot '<FFXI install>'
```

Installed collision-floor diagnostic (Release x64 build succeeded):

```powershell
./tests/bin/zone-transition-collision/Release/ZoneTransitionCollisionTests.exe '<FFXI install>' --markers
```

The marker diagnostic returns success when the scan completes, not when every
sample is visible. Final output: checked=286, noFloor=24, floorRejected=98,
belowFloor=41. Existing collision-walk tests retain their separate default mode.

## Raw collision findings

```text
FLOOR_REJECT 100:z2s8 distance=20 floor=-63.3408 bounds=-73.4778,-63.4778
NO_FLOOR 101:z2t8 distance=20
BELOW_FLOOR 101:z2ta distance=20 dotY=-68.5877 surface=-70.1654
FLOOR_REJECT 102:z2u6 distance=20 floor=8.47872 bounds=15.5942,25.5942
FLOOR_REJECT 103:z2v4 distance=3 floor=-2.50762 bounds=-12.5288,-2.52885
FLOOR_REJECT 103:z2v4 distance=10 floor=-1.71078 bounds=-12.5288,-2.52885
BELOW_FLOOR 103:z2v6 distance=10 dotY=-13.8547 surface=-14.6824
FLOOR_REJECT 103:z2v0 distance=20 floor=0.950144 bounds=-9.63902,0.36098
FLOOR_REJECT 104:z2w0 distance=10 floor=-6.18665 bounds=-26.6829,-6.68291
FLOOR_REJECT 104:z2w0 distance=20 floor=-4.12444 bounds=-26.6829,-6.68291
BELOW_FLOOR 104:z2w4 distance=20 dotY=-9.42573 surface=-10.1806
FLOOR_REJECT 104:z2wa distance=20 floor=6.38908 bounds=-5.16374,4.83626
FLOOR_REJECT 104:z2w2 distance=10 floor=-4.60904 bounds=-25.2937,-5.29371
FLOOR_REJECT 105:z2x6 distance=20 floor=-50.5277 bounds=-45.5411,-35.5411
NO_FLOOR 105:z2xk distance=10
FLOOR_REJECT 105:z2xk distance=20 floor=5.05317 bounds=16.9983,26.9983
FLOOR_REJECT 106:z2y0 distance=20 floor=2.48834 bounds=-8.08514,1.91486
FLOOR_REJECT 106:z2y8 distance=20 floor=-31.3822 bounds=-42.5166,-32.5166
FLOOR_REJECT 106:z2ya distance=20 floor=98.9333 bounds=87.1747,97.1747
BELOW_FLOOR 106:z2yc distance=20 dotY=-14.416 surface=-14.5812
BELOW_FLOOR 106:z2ye distance=20 dotY=-4.47258 surface=-4.61721
FLOOR_REJECT 107:z2z0 distance=20 floor=2.49111 bounds=-8.22465,1.77535
FLOOR_REJECT 107:z2z2 distance=20 floor=2.49609 bounds=-8.22465,1.77535
BELOW_FLOOR 108:z300 distance=20 dotY=-71.7155 surface=-72.4476
FLOOR_REJECT 108:z304 distance=20 floor=72.4831 bounds=57.3828,72.3828
FLOOR_REJECT 110:z322 distance=20 floor=24.9296 bounds=4.5663,24.5663
FLOOR_REJECT 110:z328 distance=20 floor=-38.224 bounds=-37.1129,-27.1129
FLOOR_REJECT 110:z32a distance=20 floor=-52.6372 bounds=-45.4763,-35.4763
BELOW_FLOOR 111:z338 distance=10 dotY=3.15 surface=2.85915
FLOOR_REJECT 111:z338 distance=20 floor=-0.250001 bounds=2.2072,8.2072
FLOOR_REJECT 111:z33a distance=20 floor=-0.25 bounds=2.56776,8.56776
BELOW_FLOOR 111:z33c distance=10 dotY=-16.85 surface=-17.1409
FLOOR_REJECT 111:z33c distance=20 floor=-20.25 bounds=-17.5679,-11.5679
BELOW_FLOOR 111:z33e distance=10 dotY=-36.85 surface=-37.1409
FLOOR_REJECT 111:z33e distance=20 floor=-40.25 bounds=-37.9234,-31.9234
BELOW_FLOOR 111:z33g distance=10 dotY=-56.85 surface=-57.1409
FLOOR_REJECT 111:z33g distance=20 floor=-60.25 bounds=-58.0847,-52.0847
BELOW_FLOOR 111:z33i distance=10 dotY=-96.85 surface=-97.1409
FLOOR_REJECT 111:z33i distance=20 floor=-100.25 bounds=-98.082,-92.082
FLOOR_REJECT 112:z342 distance=10 floor=-44.5053 bounds=-60.4413,-45.4413
FLOOR_REJECT 112:z342 distance=20 floor=-44 bounds=-60.4413,-45.4413
FLOOR_REJECT 112:z344 distance=20 floor=-41.1699 bounds=-31.8161,-16.8161
FLOOR_REJECT 115:z374 distance=20 floor=-22.6021 bounds=-18.6888,-8.68882
FLOOR_REJECT 115:z376 distance=20 floor=-37.8057 bounds=-34.4831,-24.4831
BELOW_FLOOR 115:z37a distance=10 dotY=-14.65 surface=-16.3
FLOOR_REJECT 115:z37a distance=20 floor=-17.25 bounds=-16.1123,-8.11226
BELOW_FLOOR 115:z37c distance=10 dotY=5.35 surface=3.70001
FLOOR_REJECT 115:z37c distance=20 floor=2.75 bounds=4.09682,12.0968
BELOW_FLOOR 115:z37e distance=10 dotY=-3.3 surface=-4.54999
FLOOR_REJECT 115:z37e distance=20 floor=-5.5 bounds=-4.63428,3.36572
FLOOR_REJECT 115:z37g distance=10 floor=-14.5162 bounds=-14.4024,-6.40237
FLOOR_REJECT 116:z382 distance=20 floor=-24.732 bounds=-18.6958,-8.6958
FLOOR_REJECT 116:z384 distance=20 floor=-24.064 bounds=-35.6921,-25.6921
FLOOR_REJECT 116:z386 distance=20 floor=-46.2687 bounds=-42.7534,-32.7534
BELOW_FLOOR 116:z388 distance=10 dotY=-34.65 surface=-36.3
FLOOR_REJECT 116:z388 distance=20 floor=-37.25 bounds=-36.3442,-28.3442
BELOW_FLOOR 116:z38a distance=10 dotY=-10.65 surface=-12.3
FLOOR_REJECT 116:z38a distance=20 floor=-13.25 bounds=-12.2867,-4.28665
BELOW_FLOOR 116:z38c distance=10 dotY=-14.65 surface=-16.3
FLOOR_REJECT 116:z38c distance=20 floor=-17.25 bounds=-16.1166,-8.11664
NO_FLOOR 117:z394 distance=20
FLOOR_REJECT 118:z3a0 distance=3 floor=-44.8255 bounds=-55.0814,-45.0814
FLOOR_REJECT 118:z3a0 distance=10 floor=-42.8633 bounds=-55.0814,-45.0814
FLOOR_REJECT 118:z3a0 distance=20 floor=-40.0378 bounds=-55.0814,-45.0814
FLOOR_REJECT 118:z3a2 distance=20 floor=-17.3832 bounds=-11.0945,-1.09453
NO_FLOOR 118:z3a4 distance=20
FLOOR_REJECT 118:z3a6 distance=10 floor=0.569322 bounds=11.322,19.322
BELOW_FLOOR 118:z3a6 distance=20 dotY=13.695 surface=12.42
FLOOR_REJECT 118:z3a8 distance=10 floor=0.636944 bounds=10.8911,18.8911
NO_FLOOR 119:z3b6 distance=10
FLOOR_REJECT 119:z3b0 distance=10 floor=-24.7263 bounds=-46.606,-26.606
FLOOR_REJECT 119:z3b0 distance=20 floor=-24 bounds=-46.606,-26.606
FLOOR_REJECT 120:z3c0 distance=10 floor=-11.6186 bounds=-31.8171,-11.8171
FLOOR_REJECT 120:z3c0 distance=20 floor=-8.29993 bounds=-31.8171,-11.8171
FLOOR_REJECT 120:z3c6 distance=20 floor=-23.9068 bounds=-30.3072,-24.3072
FLOOR_REJECT 120:z3c8 distance=20 floor=-0.257603 bounds=-6.40235,-0.402346
FLOOR_REJECT 126:z3i0 distance=10 floor=12.0807 bounds=0.979361,10.9794
NO_FLOOR 139:z3v2 distance=20
FLOOR_REJECT 140:z3w4 distance=20 floor=-17.3071 bounds=-26.8125,-18.8125
NO_FLOOR 142:z3y6 distance=20
NO_FLOOR 142:z3y8 distance=20
NO_FLOOR 142:z3ya distance=10
NO_FLOOR 142:z3ya distance=20
BELOW_FLOOR 143:z3z4 distance=10 dotY=-43.0017 surface=-44.1824
NO_FLOOR 146:z420 distance=20
NO_FLOOR 147:z434 distance=20
BELOW_FLOOR 149:z452 distance=10 dotY=0.594207 surface=0.292209
BELOW_FLOOR 149:z454 distance=10 dotY=0.799352 surface=0.391513
BELOW_FLOOR 149:z456 distance=10 dotY=1.54565 surface=0.507628
BELOW_FLOOR 149:z458 distance=10 dotY=1.31758 surface=0.482618
FLOOR_REJECT 149:z45a distance=10 floor=-2.73864 bounds=-2.26593,5.73407
NO_FLOOR 150:z460 distance=10
BELOW_FLOOR 150:z462 distance=10 dotY=-20.37 surface=-21.9875
NO_FLOOR 150:z462 distance=20
BELOW_FLOOR 150:z466 distance=10 dotY=-20.8299 surface=-22.2209
BELOW_FLOOR 150:z468 distance=10 dotY=-20.8277 surface=-22.2236
NO_FLOOR 158:z4e0 distance=20
NO_FLOOR 158:z4e2 distance=20
FLOOR_REJECT 158:z4e4 distance=3 floor=-49.2758 bounds=-46.2118,-38.2118
NO_FLOOR 158:z4e4 distance=20
NO_FLOOR 158:z4e6 distance=20
FLOOR_REJECT 161:z4h2 distance=3 floor=-33.5 bounds=-44.4968,-34.4968
BELOW_FLOOR 161:z4h2 distance=10 dotY=-40.0128 surface=-43.2628
FLOOR_REJECT 161:z4h2 distance=20 floor=-34.0128 bounds=-44.4968,-34.4968
FLOOR_REJECT 162:z4i2 distance=20 floor=-68 bounds=-75.7305,-69.7305
FLOOR_REJECT 167:z4n2 distance=3 floor=-7.25 bounds=-15.7225,-7.7225
FLOOR_REJECT 167:z4n2 distance=10 floor=-5.5 bounds=-15.7225,-7.7225
FLOOR_REJECT 170:z4q0 distance=20 floor=5.36952 bounds=-3.28101,4.71899
FLOOR_REJECT 170:z4q2 distance=20 floor=5.17259 bounds=-3.29947,4.70053
FLOOR_REJECT 192:z5c4 distance=3 floor=-8.75 bounds=-16.9916,-8.99162
FLOOR_REJECT 192:z5c4 distance=10 floor=-8 bounds=-16.9916,-8.99162
FLOOR_REJECT 192:z5c4 distance=20 floor=-7.95183 bounds=-16.9916,-8.99162
FLOOR_REJECT 192:z5ca distance=3 floor=-10.5 bounds=-19.0569,-11.0569
FLOOR_REJECT 192:z5ca distance=10 floor=-9.9526 bounds=-19.0569,-11.0569
FLOOR_REJECT 192:z5ca distance=20 floor=-10.0574 bounds=-19.0569,-11.0569
FLOOR_REJECT 194:z5e0 distance=3 floor=-8.75 bounds=-16.8661,-8.86608
FLOOR_REJECT 194:z5e0 distance=10 floor=-8 bounds=-16.8661,-8.86608
FLOOR_REJECT 194:z5e0 distance=20 floor=-8 bounds=-16.8661,-8.86608
FLOOR_REJECT 194:z5e2 distance=3 floor=-8.75 bounds=-17.1053,-9.10525
FLOOR_REJECT 194:z5e2 distance=10 floor=-8 bounds=-17.1053,-9.10525
FLOOR_REJECT 194:z5e2 distance=20 floor=-8 bounds=-17.1053,-9.10525
FLOOR_REJECT 194:z5e6 distance=10 floor=-24 bounds=-32.9756,-24.9756
BELOW_FLOOR 194:z5e6 distance=20 dotY=-29.7531 surface=-31.3781
FLOOR_REJECT 194:z5e8 distance=3 floor=-8.75 bounds=-16.9925,-8.99251
FLOOR_REJECT 194:z5e8 distance=10 floor=-8 bounds=-16.9925,-8.99251
FLOOR_REJECT 194:z5e8 distance=20 floor=-8 bounds=-16.9925,-8.99251
FLOOR_REJECT 195:z5f8 distance=10 floor=-31 bounds=-41.354,-31.354
BELOW_FLOOR 195:z5fc distance=20 dotY=-61 surface=-64
BELOW_FLOOR 195:z5fe distance=10 dotY=-53.3169 surface=-54
NO_FLOOR 196:z5g0 distance=20
NO_FLOOR 196:z5g2 distance=20
FLOOR_REJECT 198:z5i0 distance=10 floor=-10 bounds=-20.3225,-10.3225
NO_FLOOR 198:z5i2 distance=20
FLOOR_REJECT 200:z5k0 distance=10 floor=-11.25 bounds=-19.5057,-11.5057
BELOW_FLOOR 200:z5k0 distance=20 dotY=-16.9981 surface=-19
FLOOR_REJECT 200:z5k2 distance=10 floor=-17.25 bounds=-25.5957,-17.5957
BELOW_FLOOR 200:z5k2 distance=20 dotY=-22.9938 surface=-25
NO_FLOOR 204:z5o4 distance=20
FLOOR_REJECT 230:z6e0 distance=20 floor=1 bounds=-9.07888,0.921119
FLOOR_REJECT 230:z6e2 distance=20 floor=1 bounds=-9.07481,0.92519
FLOOR_REJECT 231:z6fa distance=3 floor=12 bounds=7.96575,11.9657
FLOOR_REJECT 231:z6fa distance=10 floor=12 bounds=7.96575,11.9657
FLOOR_REJECT 231:z6fa distance=20 floor=12 bounds=7.96575,11.9657
BELOW_FLOOR 233:z6h2 distance=20 dotY=5.25272 surface=2.5
BELOW_FLOOR 234:z6i4 distance=20 dotY=-12.755 surface=-13.7042
NO_FLOOR 235:z6j6 distance=20
FLOOR_REJECT 238:z6m2 distance=20 floor=-2.00448 bounds=-12.1151,-2.11512
BELOW_FLOOR 238:zmr6 distance=10 dotY=-10.5486 surface=-11.8674
BELOW_FLOOR 239:zmr8 distance=10 dotY=-10.5516 surface=-11.9342
BELOW_FLOOR 240:z6o2 distance=20 dotY=-7.77704 surface=-8.05214
FLOOR_REJECT 241:z6p2 distance=20 floor=-11.2937 bounds=-6.70689,3.29311
BELOW_FLOOR 241:zmrk distance=10 dotY=-16.4135 surface=-16.9608
BELOW_FLOOR 241:zmrk distance=20 dotY=-15.5608 surface=-16.9608
FLOOR_REJECT 243:zmrm distance=3 floor=10.0015 bounds=12.1902,22.1902
FLOOR_REJECT 243:zmrm distance=10 floor=10 bounds=12.1902,22.1902
FLOOR_REJECT 243:zmrm distance=20 floor=9.00001 bounds=12.1902,22.1902
BELOW_FLOOR 244:z6s4 distance=10 dotY=-2.79841 surface=-2.99834
FLOOR_REJECT 244:z6s4 distance=20 floor=-1.20184 bounds=-11.5418,-1.54182
BELOW_FLOOR 245:z6t2 distance=10 dotY=-2.79817 surface=-3.19816
FLOOR_REJECT 245:z6t2 distance=20 floor=-1.201 bounds=-11.2797,-1.27969
FLOOR_REJECT 246:z6u2 distance=10 floor=-1.79814 bounds=-10.3245,-2.32445
FLOOR_REJECT 246:z6u2 distance=20 floor=-1.201 bounds=-10.3245,-2.32445
NO_FLOOR 246:z6u4 distance=20
```

