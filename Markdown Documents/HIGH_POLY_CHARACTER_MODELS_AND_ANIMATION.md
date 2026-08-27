# High-Poly Character Models and Animation Sequences

## Purpose and scope

This document records what is currently known about FINAL FANTASY XI's high-poly character-creation assets and DATura's implementation of them. Its primary focus is the animation system: how motion files are associated with body and head meshes, how the two observed SQLE channel formats are stored, how their channels are mapped onto the skeleton, and how DATura turns them into skinned animation.

The information here comes from:

- direct inspection of the installed FFXI DAT files;
- behavior observed while developing DATura's character-creation preview;
- the current DATura loader and renderer implementation; and
- validated runtime measurements from DATura's Hume male preview and direct DAT-header inspection.

This is a reverse-engineering record, not an official format specification. Statements are labeled where useful:

- **Confirmed** means the structure has been observed in the DATs and is used successfully by the implementation.
- **Inferred** means the interpretation explains the data and observed behavior but has not been independently established as the retail engine's terminology or intent.
- **Unknown** identifies a field or behavior that remains unresolved.

### Revision and provenance note (August 2026)

The original version of this document described DATura's first working implementation. It has since been compared with the independent [`cexi-viewer` character-creation commit](https://github.com/CatsAndBoats/cexi-viewer/commit/cb4506d252eb983491851b15446a92b66083c328), which began from DATura's work and then performed additional DAT scans, Process Monitor capture, quaternion-error analysis, and camera/cue-track research.

That comparison materially changed several conclusions. In particular, the earlier PBChannel interpretation as LSB-first unsigned data was wrong; the five-clip cluster, equipment-specific body skeletons, cinematic camera tracks, and OC cue layer were also missing. This revision corrects the binary description and records the cross-project findings. Items demonstrated only in the external WIP implementation are identified as **cross-project findings** until they are independently reproduced and integrated into DATura.

## Executive summary

The high-poly characters are a distinct asset family from the ordinary in-game player models. A displayed character is assembled from several separate resources:

1. an RT/SHAPE body mesh DAT;
2. a DMB body material/texture DAT;
3. an RT/SHAPE head mesh DAT;
4. a DMB head material/texture DAT;
5. an SQLE body motion DAT;
6. an SQLE head motion DAT; and
7. the character-selection environment, `ROM/1/5.DAT`, used as the preview environment.

The body and head each carry their own skeleton and skin clusters. DATura combines the two skeletons into one render model, aligns the head to the body's neck attachment, and applies a matched body/head animation pair.

Two SQLE motion encodings have been observed:

| Encoding | Observed use | Storage | Typical duration |
|---|---|---|---:|
| `FrameChannel v.4` | Four short authored clips per race, including standing idle | Full 32-bit float for every channel and frame | Roughly 0.6–3.2 seconds |
| `PBChannel v.3` | The long character-creation pose track | Per-channel quantized sign-magnitude bit streams | 41.2–100 seconds, depending on race |

The PBChannel file is not a conventional looping run cycle. It is a long pose track containing held poses and transitions. For example, the Hume male sequence is 2,387 frames with a header time of 79.5333 seconds. The last frame duplicates the first so that playback can loop cleanly.

The pose track is accompanied by two authored PBChannel camera/FOV pairs and an `OC:01.00` frame-indexed cue track. Direct inspection of the tables referenced by the Hume and Galka cue IDs shows `SeSep` sound-sequence records, not a second skeletal-action stream. The earlier interpretation of `rthu`/`rtga` as missing character-action bytecode was therefore incorrect. Much of the apparent travel in the retail presentation comes from the cinematic camera; the decoded Hume male skeletal root itself spans only about 2.76 units in Z.

### Correction: `ROM/0/27.DAT` is not the character-creation zone

Some search summaries incorrectly identify `ROM/0/27.DAT` as the character-creation zone or its main `sel_` controller. Direct inspection of the installed retail DATs and the current relocated `xi-tools` documentation disproves that identification:

| DAT | Size in examined retail install | Magic | Confirmed or best-supported role |
|---|---:|---|---|
| `ROM/0/24.DAT` | 3,392 bytes | `sel_` | Character-selection/title scene controller |
| `ROM/0/25.DAT` | 3,808 bytes | `sel_` | Related selection-controller variant |
| `ROM/0/26.DAT` | 4,448 bytes | `sel_` | Related, more complex selection-controller variant |
| `ROM/0/27.DAT` | 4,960 bytes | `damv` | Damage/miss/cursor UI animation and value curves |
| `ROM/0/28.DAT` | 9,789,456 bytes | `selp` | Sel Phiner prototype exterior |
| `ROM/1/5.DAT` | 6,315,264 bytes | `f_ch` | Retail character-selection environment used by DATura |

`ROM/0/27.DAT` contains named blocks such as `dam0`, `dam1`, `mis0`, `mis1`, `cur0`, and `cur1`, plus compact position, rotation, scale, and alpha-like curve names. It contains no SQLE skeletal motion or zone geometry and does not explain the stiff PB skeletal motion.

`ROM/0/24–26.DAT` remain relevant to the broader retail selection-screen orchestration. They contain scene nodes (`s###`) and `main`, `mov1`, `mov2`, and `loop` control blocks, but their exact assignment to title, character-selection, or character-creation states is not fully established. Their `mov*` blocks enumerate internal scene nodes; they are not the PB character skeleton tracks described in this document.

## Asset organization

### Geometry and material pairing

Most high-poly face choices follow a regular pairing pattern:

- an RT/SHAPE mesh DAT contains geometry, skeleton, and skin data;
- the neighboring DMB DAT contains texture and material information;
- face A and face B generally share a head mesh but use different head material DATs; and
- “No Equipment” and “Initial Equipment” share a face selection, but the initial-equipment body pair is found two DAT indices after the no-equipment pair.

Each race currently exposes four face numbers with A/B variants. Internally, every face/material variant has two equipment entries, producing sixteen entries per race even though the UI presents race, face, and equipment separately.

The body mesh/material bases are:

| Race | No-equipment body mesh | No-equipment body material | Initial-equipment derivation |
|---|---|---|---|
| Hume Male | `ROM/63/81.dat` | `ROM/63/80.dat` | mesh/material DAT index + 2 |
| Hume Female | `ROM/63/61.dat` | `ROM/63/60.dat` | mesh/material DAT index + 2 |
| Elvaan Male | `ROM/63/21.dat` | `ROM/63/20.dat` | mesh/material DAT index + 2 |
| Elvaan Female | `ROM/63/1.dat` | `ROM/63/0.dat` | mesh/material DAT index + 2 |
| Tarutaru Male | `ROM/64/13.dat` | `ROM/64/12.dat` | mesh/material DAT index + 2 |
| Tarutaru Female | `ROM/63/121.dat` | `ROM/63/120.dat` | mesh/material DAT index + 2 |
| Mithra | `ROM/63/101.dat` | `ROM/63/100.dat` | mesh/material DAT index + 2 |
| Galka | `ROM/63/41.dat` | `ROM/63/40.dat` | mesh/material DAT index + 2 |

The complete face-to-head mesh/material table is maintained in `DATura/character_creation_dat_table.h`. The animation mapping is separate because each distinct head mesh needs its matching head skeleton channel layout.

### RT/SHAPE geometry

The creation mesh DATs begin with an RT block and contain one or more text-described shapes such as:

```text
SHAPE: TriStrip ver.2, <triangle count> tris, <code count> codes, <vertex count> verts
```

The implemented geometry observations are:

- a candidate shape begins with integer value `4` and an `RT` marker at offset `+8`;
- the shape text appears near the beginning of the block;
- position, normal, and UV arrays follow the text and small array headers;
- positions and normals are three 32-bit floats per vertex;
- UVs are two 32-bit floats per vertex; and
- the draw-code stream can describe both triangle strips and explicit triangle lists.

The draw-code stream is mixed-purpose. A negative command begins a triangle strip and its absolute value is the strip vertex count. Certain non-negative commands divisible by three can introduce explicit triangle-index lists. Other non-negative values appear to be state or material selectors and are currently skipped.

Creation meshes use the opposite vertical direction from DATura's ordinary model path. DATura reflects Y on positions and normals as the vertices are emitted. The same reflection must be applied to every local bone matrix; reflecting only the final model would break parent-child transform multiplication.

### DMB material and texture data

DMB material files begin with `DMB\0`. The current loader searches 16-byte-aligned candidate blocks for a plausible texture descriptor:

| Offset from texture block | Meaning used by DATura |
|---:|---|
| `0x40` | width |
| `0x44` | height |
| `0x48` | bytes per pixel, observed as 3 or 4 |
| `0x60` | pixel data |

The source color channels are stored in BGR/BGRA order and are converted to RGBA for rendering.

The fourth source channel is not treated as universal opacity. Opaque shapes remain opaque. Alpha is enabled only for shapes identified from DMB names such as `*_sort` or `alphaShape`, with a special hard black-key path for the Hume male `strapShape`. Different race/head families have different alpha modes because hair, ears, and other cutout regions do not all use the texture channel identically.

The apparent DXT3-style alpha nibble is expanded before alpha testing. DATura currently uses an alpha-test threshold of `0.5` for alpha and black-key materials. This material behavior is functional, but the semantic meaning of every DMB field and every race-specific alpha convention is not yet completely known.

## Skeleton and skinning data

### SQLE chunks embedded in mesh DATs

The mesh DATs contain aligned chunks beginning with `SQLE`. DATura recognizes at least two relevant chunk types:

| SQLE chunk type | Current interpretation |
|---:|---|
| `11` | skeleton and per-bone channel layout |
| `21` | skin clusters/influences |

#### Skeleton chunk type 11

The bone count is read at chunk offset `+96`, and fixed-size 64-byte bone records begin at `+100`.

The implemented 64-byte record layout is:

| Bone-record offset | Size | Meaning |
|---:|---:|---|
| `0x00` | 12 | bind translation, three floats |
| `0x0C` | 16 | bind quaternion, four floats |
| `0x1C` | 12 | bind scale, three floats |
| `0x28` | 20 | five signed integer channel counts |
| `0x3C` | 4 | parent bone index |

The five channel counts are interpreted as:

1. translation components, capacity 3;
2. quaternion components, capacity 4;
3. scale components, capacity 3;
4. an additional channel group not currently interpreted; and
5. another additional channel group not currently interpreted.

All five counts are still consumed when advancing through a motion frame. This is essential: ignoring unknown groups would shift every subsequent bone's channels.

Body and head skeletons are parsed independently and then appended into a combined bone array. Parent indices are rebased by the starting index of the source file's bone range. Generated bone names use the form `sqle_<file-index>_<source-bone-index>` because authoritative source names have not yet been recovered from these creation skeleton records.

#### Skin chunk type 21

The type-21 chunk begins with a cluster count at `+96` and another declared count at `+100`. The second value is **not** reliably the shape's vertex count. For example, a shape with 933 vertices can declare 510 there. DATura therefore sizes the skin table from the highest actual vertex index encountered.

Each cluster contains:

1. a bone index;
2. an influence count;
3. an array of vertex indices; and
4. a same-length array of floating-point weights.

For each emitted vertex, DATura:

- sorts influences by descending weight;
- keeps at most eight influences;
- renormalizes the retained weights;
- transforms the bind position and normal into each bone's local bind space; and
- stores a separately weighted local position contribution for each influence.

The stored local XYZ contribution is multiplied by its normalized weight. During animation, the homogeneous component supplied to the matrix is also the weight, which correctly weights translation. This detail fixed a prior failure mode in which animation could distort or displace the mesh severely.

## Body/head assembly

The body and head skeletons are authored in a shared world, but their independently animated roots can carry a nearly constant positional bias. The relevant attachment bones are:

- body attachment: `bone0004`;
- head attachment: `bone0001`.

DATura initially calculated only a static head offset while assembling the model. It now also preserves the bind-pose body-4-to-head-1 delta and re-pins the complete head skeleton to that delta every animated frame. Cross-project measurements found that the PB head track's bias is stable to within about 0.2 units over a full sequence—for example, roughly +3.18 Z for Mithra, -1.72 Z for Galka, and +0.64 Z for Elvaan male. Recomputing only the head translation removes this authoring bias without changing head rotation.

There is also a distinct per-face vertical-alignment problem. Comparing the bottom of each `faceShape`—used as a chin-height proxy—shows that faces within one race are not always authored at a common height. For example, Hume male faces 1/2 are near -14.69 while faces 3/4 are near -15.35/-15.24; Hume female face 1 is near -13.11 while its siblings are near -13.66. The cross-project implementation derives these corrections from geometry rather than using DATura's earlier visual estimates:

| Race / face | Derived head Y correction |
|---|---:|
| Hume Male 3 | +0.653 |
| Hume Male 4 | +0.546 |
| Hume Female 1 | -0.545 |
| Elvaan Male 4 | -0.842 |
| Elvaan Female 1 | -0.645 |
| Elvaan Female 3 | -0.645 |
| Tarutaru Male 1 | -0.069 |
| Tarutaru Male 3 | -0.069 |

All unlisted faces use zero relative correction in that derivation. These values have not yet replaced DATura's current face-table offsets.

The body and head skeletons remain distinct ranges inside the combined render model. Their animation streams also retain separate channel cursors.

The requirement for a head-specific SQLE mapping is important. Different face meshes can have different head skeletons and channel counts even within the same race. Reusing the face-1 head motion for all faces can therefore produce a channel-count mismatch or animate the wrong controls.

## SQLE motion files

### Common header

Observed animation DATs begin with `SQLE` and contain an ASCII description near the beginning:

```text
MOTION: <channel type>, time=<seconds>, size=<channel count>, frames=<frame count>[, step=<value>]
```

DATura currently records:

- channel type;
- duration in seconds;
- scalar channel count;
- frame count; and
- all decoded scalar values in frame-major order.

The cross-project playback analysis concludes that all five authored clips run at a flat **30 fps**. The header's `time` value should be retained as format metadata, not used directly to derive the runtime rate:

- PB headers generally describe `(frameCount - 1) / 30`, yielding a misleading derived value just above 30 fps if the loop frame is included in the numerator.
- FrameChannel headers behave approximately like `(frameCount - 2) / 60`; deriving FPS from them produces rates around 60–61 fps and makes the short motions play at roughly double speed.

DATura now uses a fixed 30 fps for both skeletal clips and the legacy FrameChannel fallback while retaining the header time as metadata.

### FrameChannel v.4

`FrameChannel v.4` stores uncompressed 32-bit floating-point samples. The confirmed tail layout is:

```text
[one 32-bit control word per channel]
[frameCount * channelCount 32-bit floats]
```

The float array occupies the end of the file and is frame-major:

```text
value(frame, channel) = values[frame * channelCount + channel]
```

An earlier incorrect interpretation assumed an extra bind-pose frame. That shifted the float-data start backward into the control words. Those control words became denormal or nonsensical transforms, after which animation safety checks restored the A-pose. The corrected parser uses exactly `frameCount * channelCount` floats from the file tail.

Every race has four matched body/head FrameChannel pairs clustered immediately before the PB sequence. Relative to the sequence DAT index, their offsets are `-4`, `-3`, `-2`, and `-1`; the `-2` pair is standing idle. The `-3` and `-1` pairs were absent from DATura's original mapping even though they are among the liveliest and most useful clips.

The five known animation choices are therefore:

| Relative index | Encoding | Current neutral name | Approximate character |
|---:|---|---|---|
| `-4` | FrameChannel v.4 | Motion 1 | short, roughly 0.6–0.9 s |
| `-3` | FrameChannel v.4 | Motion 2 | short, roughly 0.6–0.7 s |
| `-2` | FrameChannel v.4 | Standing idle | roughly 2–3 s |
| `-1` | FrameChannel v.4 | Motion 3 | short, roughly 0.9–1.3 s |
| `0` | PBChannel v.3 | Creation sequence pose track | roughly 41–100 s |

The neutral Motion 1/2/3 labels are intentional because their retail semantic names have not been established. The file originally called “walk/run” in DATura is only one member of this cluster and should not be confused with the long creation presentation.

### PBChannel v.3

`PBChannel v.3` is the encoding used by the long character-creation presentation. “PB” is the string found in the file; its expanded name is **unknown**.

#### Global fields

The following offsets are confirmed for the examined PBChannel v.3 files:

| File offset | Type | Meaning |
|---:|---|---|
| `0x60` | float | duration in seconds |
| `0x64` | uint32 | scalar channel count |
| `0x68` | uint32 | frame count |
| `0x6C` | float | global/header step value |
| `0x74` | — | first per-channel record |

The ASCII header contains the same duration, channel count, and frame count. The header's `step` value has been observed as `1` in the mapped PB files. Its precise runtime purpose is not yet known because each scalar record also contains its own quantization step.

#### Per-channel record

Starting at `0x74`, one variable-length record appears for each scalar channel:

| Record offset | Type | Meaning |
|---:|---|---|
| `+0x00` | uint32 | packed payload size in bytes |
| `+0x04` | uint32 | bits per sample |
| `+0x08` | float | quantization step |
| `+0x0C` | float | base/rest value |
| `+0x10` | bytes | packed sign-magnitude frame-delta codes |

Observed bit widths are `0`, `2`, `4`, `8`, and `16`. The 2-bit case occurs in the Elvaan female base pair and was omitted from the first DATura analysis.

For nonconstant channels:

```text
sampleCount   = frameCount - 1
payloadBytes  = ceil(sampleCount * bitsPerSample / 8)
signBit       = 1 << (bitsPerSample - 1)
magnitude     = code & (signBit - 1)
signedOffset  = (code & signBit) ? -magnitude : magnitude
decodedDelta  = signedOffset * quantizationStep
```

Bits are packed **most-significant-bit first**. A practical decoder reads a big-endian 24-bit window beginning at `bitOffset / 8`, shifts right by `24 - (bitOffsetWithinByte + bitsPerSample)`, and masks `bitsPerSample` bits. The high bit of each code is a sign bit; the remaining bits are the magnitude of a delta from the previous frame.

`baseValue` is frame zero. There are `frameCount - 1` packed deltas, each of which advances the preceding value:

```text
value[0] = baseValue
value[n + 1] = value[n] + signedMagnitude(code[n]) * quantizationStep
```

DATura accumulates every PB channel, including root translation. Treating the root triplet as bounded `baseValue + offset` samples makes the character animate in place and breaks alignment with the authored cinematic camera. The resulting long root trajectory is therefore retained as part of the presentation rather than constrained like a playable character.

MSB-first sign-magnitude decoding is supported by exact agreement with the record chain at the end of the file. Some PB frames still contain rotation outliers; those are repaired separately by rejecting non-unit quaternion frames and interpolating across the gaps.

When `bitsPerSample == 0`, the channel is constant and all decoded frames use `baseValue`. Despite containing no codes, every observed constant channel retains a one-byte padding payload, so its recorded payload size is `1`, not `0`.

Only `frameCount - 1` samples are stored. DATura reconstructs the final frame by copying frame zero. This matches the observed loop structure and avoids reading into the record that follows.

After all channel records, the examined files have a tail of exactly four bytes per channel. This is believed to be a channel-control table, but its individual bit fields are not yet interpreted. The skeleton's own five per-bone channel counts are sufficient for the current channel-to-transform mapping.

#### Invalid PB quaternion frames

Correct sign-magnitude decoding makes the vast majority of PB quaternion samples nearly unit length, but it does not eliminate every bad pose. The cross-project scan found that roughly 2% of PB frames contain one or more quaternions farther than 5% from unit length, with observed lengths as high as 2.27. FrameChannel rotations stay within approximately `3.6e-8` of unit length, making the PB outliers far too large to explain as ordinary quantization error.

Several checks argue that these are bad or deliberately non-pose samples rather than a remaining container-decoding error:

- the per-channel record chain lands exactly on the four-byte-per-channel tail;
- 16-bit samples are byte-aligned, ruling out a sliding bit-window error for the worst channels;
- alternative unsigned and two's-complement interpretations perform much worse under the unit-quaternion test; and
- separate body and head files produce bad rotations at the same performance frames 5–28 times more often than random coincidence predicts.

Simply normalizing an invalid quaternion creates an arbitrary rotation and causes the twitching/popping seen in the long sequence. The WIP cross-project repair marks rotations with `abs(1 - |q|) > 0.05` as missing, merges bad runs separated by only one or two apparently good frames, and bridges each gap between neighboring valid rotations. Cubic Hermite interpolation is used so the repaired interval also approaches the valid endpoints with compatible velocity; every result is renormalized. A second pass replaces isolated unit-length detours that jump over 30 degrees and immediately return close to the preceding trajectory.

This is a pragmatic reconstruction, not proof of the retail rule. The correlated bad frames may ultimately be explained by unused channel groups or per-channel flags. DATura now performs this targeted missing-frame repair before generating its per-frame skeletal matrices.

### Motion channels and bone transforms

Motion values are consumed in skeleton order. For every bone, DATura iterates all five channel groups in order and advances a file-specific cursor. Recognized values replace the corresponding bind component:

```text
translation: up to 3 values
quaternion:  up to 4 values
scale:       up to 3 values
groups 4–5: consumed, currently not applied
```

Missing components retain their bind values. Quaternions are normalized before conversion to matrices; a near-zero quaternion falls back to identity.

The resulting local matrix is:

1. rotation from the normalized quaternion;
2. basis rows multiplied by X/Y/Z scale;
3. translation assigned from the three translation channels;
4. conjugated by the creation model's Y reflection; and
5. multiplied by its parent world matrix.

Separate channel cursors are maintained for the body and head files. This prevents the combined skeleton's head bones from consuming the tail of the body stream.

Before an animation is built, DATura sums all five skeleton channel counts for each source file and requires an exact match with that motion file's declared channel count. A fully skeletal combined clip is created only when **both** body and head streams are valid and compatible. This avoids animating one half of the character while leaving the other half frozen.

### Equipment-specific body skeletons

The initial-equipment body is not always a geometry-only variant. A scan of the creation skeletons found that Tarutaru, Mithra, and Galka use different body channel layouts for their two equipment states:

| Race family | No-equipment / short-clip channels | Initial-equipment / PB-sequence channels |
|---|---:|---:|
| Tarutaru | 299 | 251 |
| Mithra | 335 | 407 |
| Galka | 349 | 389 |

The long PB creation sequence is authored for the `bodyMesh + 2` initial-equipment model for these races, while all four short FrameChannel clips match the no-equipment body. The changed counts appear to include equipment/cloth controls. Hume and Elvaan use a 299-channel-compatible body skeleton across both variants.

This explains a previously puzzling class of exact-match failures. The correct response is to load the body variant authored for the selected motion, not to relax channel validation or stretch one motion across a different skeleton. A viewer may either switch the visible equipment automatically or clearly report that the chosen clip belongs to the other body variant.

### Root motion in the long sequence

The PB sequence contains authored horizontal root travel. For example, the Hume male's first root translation is not centered near the preview origin. Applying it literally can place the character tens of world units away from the fixed character-creation camera.

DATura therefore rebases the root X and Z channels relative to frame zero:

```text
displayedRootXZ(frame) = decodedRootXZ(frame)
                       + bindRootXZ
                       - decodedRootXZ(frame 0)
```

Y is deliberately not rebased because vertical movement is part of the performance. DATura does not add a world-space grounding offset.

This preserves relative motion inside the performance while beginning it at the model's assembled bind origin. Whether the retail client additionally constrains, cancels, or stages root motion is still unknown.

DATura now keeps that rebased trajectory separate from skeletal deformation. During animation construction, the body-root displacement from frame zero is stored once per frame and subtracted from every body/head bone world matrix, leaving a root-relative skinned pose. The render path samples the trajectory at the same fractional frame as the pose and adds it to the character's Direct3D world transform. This makes the actor travel through the environment explicitly and prevents root translation from being lost, partially cancelled, or inconsistently inherited through CPU skinning. Translation is applied exactly once; it is not left in the bone matrices and then added again at render time.

## Known animation mappings

### Race-level five-clip clusters and representative face-1 head pairs

The table lists the body and face-1 head motion clusters in the order `Motion 1 / Motion 2 / Idle / Motion 3 / PB sequence`.

| Race | Body five-clip cluster | Face-1 head five-clip cluster |
|---|---|---|
| Hume Male | `66/12`, `66/13`, `66/14`, `66/15`, `66/16` | `66/18`, `66/19`, `66/20`, `66/21`, `66/22` |
| Hume Female | `65/82`, `65/83`, `65/84`, `65/85`, `65/86` | `65/88`, `65/89`, `65/90`, `65/91`, `65/92` |
| Elvaan Male | `64/94`, `64/95`, `64/96`, `64/97`, `64/98` | `64/100`, `64/101`, `64/102`, `64/103`, `64/104` |
| Elvaan Female | `64/36`, `64/37`, `64/38`, `64/39`, `64/40` | `64/42`, `64/43`, `64/44`, `64/45`, `64/46` |
| Tarutaru Male | `67/0`, `67/1`, `67/2`, `67/3`, **`67/58`** | `67/60`, `67/61`, `67/62`, `67/63`, `67/64` |
| Tarutaru Female | `67/0`, `67/1`, `67/2`, `67/3`, `67/4` | `67/6`, `67/7`, `67/8`, `67/9`, `67/10` |
| Mithra | `66/70`, `66/71`, `66/72`, `66/73`, `66/74` | `66/76`, `66/77`, `66/78`, `66/79`, `66/80` |
| Galka | `65/24`, `65/25`, `65/26`, `65/27`, `65/28` | `65/30`, `65/31`, `65/32`, `65/33`, `65/34` |

All paths in this table are relative to `ROM/` and end in `.dat`. Tarutaru male is the exception to the simple contiguous-body rule: Process Monitor capture showed that its retail PB body is `ROM/67/58.dat`, not `ROM/67/4.dat`. `67/58` has 1,624 frames and matches the face-1 head sequence at `67/64`; the old `67/4` mapping has 1,315 frames and belongs to Tarutaru female.

### Long-sequence timing by race

These values were read directly from the installed body PBChannel headers, with the Tarutaru male row corrected from the retail-captured body file. The matched head file uses the same frame count and header time for a given race/face pairing, though its channel count varies with the head skeleton.

| Race | Body PB channels | Frames | Header time | Playback rate |
|---|---:|---:|---:|---:|
| Hume Male | 299 | 2,387 | 79.5333 s | 30 fps |
| Hume Female | 299 | 1,808 | 60.2333 s | 30 fps |
| Elvaan Male | 299 | 1,237 | 41.2 s | 30 fps |
| Elvaan Female | 299 | 2,301 | 76.6667 s | 30 fps |
| Tarutaru Male | 251 | 1,624 | 54.1 s | 30 fps |
| Tarutaru Female | 251 | 1,315 | 43.8 s | 30 fps |
| Mithra | 407 | 3,001 | 100.0 s | 30 fps |
| Galka | 389 | 1,726 | 57.5 s | 30 fps |

The differing lengths strongly support the interpretation that these are race-specific presentations rather than interchangeable locomotion cycles.

### Face-specific head sequence mapping

Each row gives the head mesh followed by its `PB sequence / idle / Motion 1` DATs. Motion 2 and Motion 3 are respectively one DAT after Motion 1 and one DAT before the PB sequence. Paths are relative to `ROM/`.

| Race | Face | Head mesh | Head PB / idle / Motion 1 |
|---|---:|---|---|
| Elvaan Female | 1 | `63/5` | `64/46`, `64/44`, `64/42` |
| Elvaan Female | 2 | `63/9` | `64/58`, `64/56`, `64/54` |
| Elvaan Female | 3 | `63/13` | `64/52`, `64/50`, `64/48` |
| Elvaan Female | 4 | `63/17` | `64/82`, `64/80`, `64/78` |
| Elvaan Male | 1 | `63/25` | `64/104`, `64/102`, `64/100` |
| Elvaan Male | 2 | `63/29` | `64/116`, `64/114`, `64/112` |
| Elvaan Male | 3 | `63/33` | `65/0`, `64/126`, `64/124` |
| Elvaan Male | 4 | `63/37` | `65/12`, `65/10`, `65/8` |
| Galka | 1 | `63/45` | `65/34`, `65/32`, `65/30` |
| Galka | 2 | `63/49` | `65/40`, `65/38`, `65/36` |
| Galka | 3 | `63/53` | `65/46`, `65/44`, `65/42` |
| Galka | 4 | `63/57` | `65/52`, `65/50`, `65/48` |
| Hume Female | 1 | `63/65` | `65/92`, `65/90`, `65/88` |
| Hume Female | 2 | `63/69` | `65/104`, `65/102`, `65/100` |
| Hume Female | 3 | `63/73` | `65/116`, `65/114`, `65/112` |
| Hume Female | 4 | `63/77` | `66/0`, `65/126`, `65/124` |
| Hume Male | 1 | `63/85` | `66/22`, `66/20`, `66/18` |
| Hume Male | 2 | `63/89` | `66/34`, `66/32`, `66/30` |
| Hume Male | 3 | `63/93` | `66/46`, `66/44`, `66/42` |
| Hume Male | 4 | `63/97` | `66/58`, `66/56`, `66/54` |
| Mithra | 1 | `63/105` | `66/80`, `66/78`, `66/76` |
| Mithra | 2 | `63/109` | `66/92`, `66/90`, `66/88` |
| Mithra | 3 | `63/113` | `66/104`, `66/102`, `66/100` |
| Mithra | 4 | `63/117` | `66/116`, `66/114`, `66/112` |
| Tarutaru Female | 1 | `63/125` | `67/10`, `67/8`, `67/6` |
| Tarutaru Female | 2 | `64/1` | `67/22`, `67/20`, `67/18` |
| Tarutaru Female | 3 | `64/5` | `67/34`, `67/32`, `67/30` |
| Tarutaru Female | 4 | `64/9` | `67/46`, `67/44`, `67/42` |
| Tarutaru Male | 1 | `64/17` | `67/64`, `67/62`, `67/60` |
| Tarutaru Male | 2 | `64/21` | `67/76`, `67/74`, `67/72` |
| Tarutaru Male | 3 | `64/25` | `67/88`, `67/86`, `67/84` |
| Tarutaru Male | 4 | `64/29` | `67/100`, `67/98`, `67/96` |

Face A and B share the same head mesh and animation set; their material DATs differ.

## Cinematic camera tracks

Process Monitor capture showed that the retail creation screen loads four camera-related SQLE files alongside each long body/head sequence: a one-channel FOV track and a 16-channel matrix track for each of two cameras.

For seven races these commonly sit at offsets `-8` through `-5` from the PB body sequence:

| Relative index | Channels | Interpretation |
|---:|---:|---|
| `-8` | 1 | Camera A field of view |
| `-7` | 16 | Camera A world matrix |
| `-6` | 1 | Camera B field of view |
| `-5` | 16 | Camera B world matrix |

Tarutaru male is again an exception; its captured camera files are adjacent to its corrected `67/58` sequence and should be mapped explicitly rather than derived arithmetically.

| Race | Camera 1 FOV / matrix | Camera 2 FOV / matrix |
|---|---|---|
| Hume Male | `66/8`, `66/9` | `66/10`, `66/11` |
| Hume Female | `65/78`, `65/79` | `65/80`, `65/81` |
| Elvaan Male | `64/90`, `64/91` | `64/92`, `64/93` |
| Elvaan Female | `64/32`, `64/33` | `64/34`, `64/35` |
| Tarutaru Male | `67/54`, `67/55` | `67/56`, `67/57` |
| Tarutaru Female | `66/124`, `66/125` | `66/126`, `66/127` |
| Mithra | `66/66`, `66/67` | `66/68`, `66/69` |
| Galka | `65/20`, `65/21` | `65/22`, `65/23` |

All paths are relative to `ROM/` and end in `.dat`.

The 16 values per frame form a row-major 4×4 transform. The last row is `(0, 0, 0, 1)`, the upper 3×3 is orthonormal, and translation occupies the last column. Under DATura's reflected-Y creation coordinates:

```text
eye     = (m[3], -m[7], m[11])
forward = normalize(-m[2], m[6], -m[10])
```

The examined FOV track is constant at approximately 37.85 degrees. Camera A can span roughly 130 world units in Z, while the character's body track rotates only about 30 degrees over the corresponding presentation. This explains why the original screen feels much more active than a fixed-camera playback of the PB pose track.

## OC cue track and per-race sound-sequence layer

The retail client also loads an `OC:01.00` cue DAT beside the long sequence. For Hume male, the capture includes `ROM/66/17.dat` immediately after body sequence `ROM/66/16.dat`. The observed cue structure is:

| Offset | Structure |
|---:|---|
| `0x00` | ASCII `OC:0` followed by `1.00` |
| `0x08` | five 32-bit header values: `1, -1, 6, 6, -1` |
| `0x1C` | 32-bit record count |
| `0x28` | records of `[u32 frame, u32 actionId, u32 zero]` |

Frames are monotonic within the declared record count and fall inside the PB sequence. Data after the declared records is padding. Captured files contain on the order of 50–130 cues depending on race.

The second value indexes a separate per-race table identified by names such as `rthu` and `rtga`. Observed numbering families include 4001+ for Hume male, 0001+ for Galka, and 8001+ for Tarutaru male. Those table records begin with `SeSep` and point to sound-effect sequences; they should not be treated as skeletal action clips.

This leaves the PB body/head pair as the known skeletal performance stream. The OC timing data is still useful for synchronizing sound, but it does not explain or repair malformed skeletal samples.

## Playback and CPU skinning

For a compatible body/head pair, DATura precomputes one world matrix per bone per animation frame. At runtime it selects:

```text
frameIndex = floor(animationTime * fps) % frameCount
```

Every frame is CPU-skinned from immutable bind vertices rather than from the previous animated frame. For each influence:

1. the stored weighted local bind position is transformed by the current bone world matrix;
2. the transformed contributions are summed;
3. the local normal is transformed and multiplied by the influence weight; and
4. the accumulated normal is normalized.

The resulting CPU vertex array is uploaded to the D3D9 vertex buffer. This is simple and correct for the current preview, though precomputing every matrix and uploading all animated vertices every frame has obvious memory and performance costs.

The cross-project renderer samples fractional frames at 30 fps. Translation and scale are linearly interpolated. Quaternion endpoints are hemisphere-aligned and normalized linear interpolation is used so the shorter rotational arc is selected. When body and head clips differ in length, the head is sampled at the same normalized phase as the body. DATura now also samples between adjacent precomputed skeletal frames and blends their skinned vertex positions and normals, removing the old nearest-frame stepping.

The long file contains extended holds as well as movement. Hume male, for example, is reported motionless for about 13% of its frames, including approximately the first eight seconds and last twenty seconds. One useful analysis technique is to sum the absolute change of every body channel per frame, smooth that motion-energy signal over roughly one-sixth of a second, and use quiet runs of at least 0.6 seconds to separate active pose transitions. This exposes inspectable subsequences, but the boundaries are heuristic and should not be mistaken for decoded retail event names.

DATura uses that same motion-energy principle only to choose the initial preview time. If a PB presentation begins with at least two seconds of sustained near-zero channel movement, the preview opens at the first sustained transition rather than repeatedly restarting inside the hold whenever the race, face, equipment, or animation selection reloads the model. Playback remains modulo the original frame count, so it still wraps through every authored frame; no part of the sequence is deleted or time-scaled. For Hume male the detected preview start is approximately 10.9 seconds.

### Animation safety checks

Before committing a deformed pose, DATura verifies that:

- every animated position is finite and within the general numeric sanity limit;
- the model has not collapsed to an implausibly small fraction of its bind size; and
- the animated bounds have not expanded or translated implausibly far from bind bounds.

Ordinary clips use a compact allowance of `max(2, bindSize * 2.5)` per axis. PB creation sequences use a scoped extended-pose allowance of `max(24, bindSize * 8)` because the authored presentation contains turns, crouches, and broad depth changes. Numeric validity checks remain active. This exception is attached to the PB-created animation object and does not weaken validation for other animation systems.

This distinction originally appeared necessary because Hume male poses around 10 and 40 seconds extended much farther in Z than an idle or run cycle. Those original measurements used the incorrect unsigned PB decode. After correcting the decoder and reconstructing invalid rotations, native validation found stable character-scale bounds at frames 0, 300, 1,200, 1,800, and 2,380, with no non-finite matrices or vertices. The extended allowance remains useful for authored travel, but it is no longer masking the extreme malformed poses shown by the old decoder.

### Current fallback path

If DATura cannot build a compatible combined skeletal clip, FrameChannel motion has a legacy geometric fallback. It treats every seven values as translation plus quaternion, associates each vertex with the nearest transform group in frame zero, and applies that group's delta directly to the vertex and normal.

This fallback is an approximation, not true skinning. It does not understand the embedded skin clusters and should not be used as evidence of the actual SQLE channel semantics. Its purpose is to provide limited motion for otherwise incompatible short FrameChannel previews. PBChannel animation has no equivalent fallback.

## Placement in the character-creation zone

The preview uses `ROM/1/5.DAT`, the environment reported by community reverse-engineering as the scene shown during retail character selection. It is loaded through the regular zone environment path so terrain, atmosphere, and other authored environment content render behind the character. Direct inspection finds 2,699 placed objects and 252 MapGeo segments. `ROM/0/28.DAT` remains separately available as the Sel Phiner prototype exterior; its `selp` identifier alone is not evidence that it is the retail character-selection stage.

PB root translation supplies the authored trajectory. DATura leaves the character's altitude unchanged and searches the collision mesh for a walkable surface that intersects the soles' existing Y plane. The nearest matching point supplies only an X/Z scene-placement translation. The same X/Z translation is applied to the cinematic camera, preserving its alignment with the actor. This deliberately does not prevent later clipping as the authored trajectory crosses the currently loaded environment.

## Current UI semantics

The animation menu presently has three choices:

| UI choice | Behavior |
|---|---|
| `A-pose` | Stops time and restores immutable bind vertices |
| `Standing idle` | Loads the race/body and face/head FrameChannel idle pair |
| `Character creation sequence` | Loads the race/body and face/head PBChannel base pair |

The creation panel also provides a **Use animated camera** checkbox. It is enabled only for the long character-creation sequence. When checked, DATura samples the race's primary authored 16-channel camera matrix and one-channel FOV track at the same 30 Hz timeline as the body animation. When unchecked, the normal orbit/fly camera remains active and user-controlled. Disabling the checkbox immediately returns rendering to that free camera without reloading the character.

Changing race, face, equipment, or animation reloads the assembled model and resets animation time to zero.

This table describes DATura as it exists, not the complete known asset set. Motion 2 and Motion 3 are absent, and the PB option does not currently drive either authored camera or synchronize the OC sound cues. PB decoding, 30 fps timing, invalid-rotation repair, fractional-frame vertex blending, per-frame head attachment, equipment-specific body selection, and the Tarutaru male sequence path are implemented.

## Validated observations

The Hume male face-1 no-equipment case has been exercised most deeply. Confirmed runtime values include:

- body sequence: `ROM/66/16.dat`;
- head sequence: `ROM/66/22.dat`;
- encoding: PBChannel v.3;
- frames: 2,387;
- duration: 79.5333 seconds;
- body channels: 299;
- head channels: 488;
- playback rate: 30 fps, with the header time describing the nonduplicated sample interval;
- correctly decoded PB samples use MSB-first sign-magnitude offsets around each channel's rest value;
- some PB quaternion frames remain invalid and require further interpretation or reconstruction; and
- the final stored frame loops to frame zero.

The short Hume male files were also identified:

- body `ROM/66/12.dat`: 53 frames, 0.85 seconds, 299 channels;
- head `ROM/66/18.dat`: 53 frames, 0.85 seconds, 488 channels;
- body idle `ROM/66/14.dat`: 133 frames, 2.18333 seconds, 299 channels; and
- head idle `ROM/66/20.dat`: 133 frames, 2.18333 seconds, 488 channels.

The complete Hume male short cluster additionally includes body/head pairs `66/13` + `66/19` and `66/15` + `66/21`. The retail sequence also loads camera pairs `66/8` + `66/9` and `66/10` + `66/11`, plus OC cue track `66/17`.

## Known limitations and unresolved questions

### Format questions

- The expansion and precise meaning of “PBChannel” are unknown.
- The four-byte-per-channel PB tail is not decoded.
- SQLE skeleton channel groups four and five are consumed but not interpreted.
- The semantic meaning of FrameChannel control words is unknown.
- The global PB header step at `0x6C` is not understood beyond its observed value.
- The full semantics of non-negative RT/SHAPE draw commands are incomplete.
- DMB is only partially understood; material graphs, all texture blocks, and all alpha conventions have not been decoded.

### Animation questions

- The retail client's exact root-motion policy is unknown. DATura rebases horizontal frame-zero root position for preview usability.
- The long pose tracks can be segmented heuristically by motion energy, but retail semantic names and exact action boundaries are unknown.
- Two camera tracks and the OC frame cue list are associated with the PB frames, but the logic that selects/switches cameras is not known.
- The `SeSep` sound sequences addressed by OC values are not played or synchronized in DATura.
- The meaning of the correlated invalid-quaternion frame clusters is unresolved. Hermite repair reduces visible popping but may only be compensating for a missing control/event semantic.
- Body/head compatibility has been encoded from known pairings, but every race, face, and equipment combination still needs visual regression testing through the entire sequence.
- Tarutaru/Mithra/Galka equipment variants are now known to carry distinct animation channel layouts. The long sequence and short clips must be tested on their authored body variants rather than treated as interchangeable equipment skins.
- The eyelash/green-mesh problem remains unresolved. The leading hypothesis is incomplete DMB per-shape material/texture assignment and/or an attachment/morph-control issue, but no definitive field-level explanation has been established.

### Implementation limitations

- Animation is CPU-skinned and uploads dynamic vertices every frame.
- All frame/bone world matrices are precomputed, which is straightforward but memory-heavy for 3,001-frame sequences.
- Playback is nearest-frame with no interpolation.
- DATura exposes only A-pose, standing idle, and the PB sequence; two additional FrameChannel clips remain unmapped in its UI.
- DATura does not expose scrubbing, pause, frame stepping, speed, heuristic subsequences, or either authored camera.
- DATura does not yet use the geometry-derived per-face head offsets.
- DATura does not parse or execute the OC sound-cue layer.
- The fallback FrameChannel deformation is only an approximation when true skeleton compatibility is unavailable.
- Manually opening an SQLE DAT through the generic file loader still displays an outdated “loading is not implemented” information message, even though character creation now parses both known motion encodings through its specialized path.

## Recommended next investigations

1. Add automated quaternion-unit and multi-frame deformation tests for every race/body/head sequence pair, including both equipment bodies.
2. Add fractional-frame translation/scale/quaternion interpolation on top of the fixed 30 fps timeline.
3. Add the missing `-3` and `-1` FrameChannel pairs so all four short motions and the long sequence are selectable.
4. Replace visual head offsets with the measured chin-height corrections.
5. Implement both authored camera/FOV pairs and expose camera selection during the PB sequence.
6. Parse the OC cue DATs for every race and synchronize their referenced `SeSep` sound sequences with the PB timeline.
7. Decode the PB four-byte-per-channel flag table and compare it with invalid quaternion clusters, skeleton channel groups four/five, and OC cue frames before treating interpolation as the final explanation.
8. Diagnose DMB per-shape texture/material assignment and the eyelash/green-mesh issue using shape names, alpha flags, and any still-unmapped material records.
9. Add a timeline scrubber, frame/time readout, motion-energy plot, and heuristic subsequence selection for studying long presentations.
10. Validate all 32 head meshes across A/B materials and both equipment bodies, recording skeleton/channel mismatches and visible attachment errors.
11. Replace the FrameChannel nearest-transform fallback with true skeletal application wherever compatible skin and skeleton data are available.
12. Move animation matrix generation to on-demand sampling, then consider GPU skinning if the preview expands to multiple high-poly characters.

## Implementation reference

The principal implementation locations are:

| Area | File / symbol |
|---|---|
| Race, face, mesh, and DMB table | `DATura/character_creation_dat_table.h` |
| RT/SHAPE, DMB, SQLE skeleton, and skin parsing | `DATura/model_ff11_creation.cpp` |
| Animation resource mapping | `DATura/ffxi_creation_animation_paths.cpp` |
| FrameChannel and PBChannel decoding | `FFXISqle::ReadMotionInfo` in `DATura/ffxi_sqle_motion.cpp` |
| Channel-to-bone matrices, PB repair, and head pinning | `DATura/ffxi_sqle_model_animation.cpp` |
| Combined clip generation | `FFXISqleModelAnimation::BuildSkeletalAnimation` |
| Preview animation update | `FFXISqleModelAnimation::UpdatePreview` |
| Ground placement | `DATura/ffxi_creation_grounding.cpp` |
| CPU skinning and pose validation | `noesisModel_t::UpdateAnimation` in `DATura/noesis_rapi.cpp` |
| Cross-project decoder, camera, cue, and equipment findings | [`cexi-viewer` commit `cb4506d`](https://github.com/CatsAndBoats/cexi-viewer/commit/cb4506d252eb983491851b15446a92b66083c328), principally `ui/js/creation.js` |

This document should be updated whenever a new SQLE channel field, DMB material behavior, motion mapping, or retail staging rule is confirmed.
