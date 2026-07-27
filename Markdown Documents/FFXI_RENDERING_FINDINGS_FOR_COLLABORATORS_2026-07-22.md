# FFXI Rendering: Findings for the DATura and xim/cexi Teams

**Date:** 2026-07-22
**From:** the third RE effort (FFXiMain disassembly + DAT corpus tooling)
**Replying to:** `FFXI_GEOMETRY_AND_TEXTURE_RENDERING.md` (DATura) and the xim/cexi corrections list

> **DATura provenance note:** This is a contributed findings document. Its disassembly and corpus claims have not been independently reproduced by the DATura project, and the underlying binary, Ghidra project, raw instruction bytes, and corpus fixtures are not part of this repository. Terms such as “confirmed (retail)” below express the contributor's evidence assessment, not an independent DATura verification. In particular, the 19-bit versus 20-bit chunk-size mask remains unresolved until a discriminating header or inspectable original-client trace is available.

## How to read this document

Everything below is keyed to *your* documents — your section names, your open questions, your numbered correction items — so you can ingest it directly. Where we confirm something you marked "Strongly supported" or "DATura reconstruction," you can promote it. Where we correct something, we show the evidence. Where you asked "Unknown," we give the answer if we have it.

**Our evidence base, so you can weigh each claim:**

1. **Retail disassembly (strongest).** A Ghidra project over the POL/LZSS-unpacked retail `FFXiMain.dll` — 12.46 MB image, base `0x10000000`, embedded build string `"Version 18:52:04 Apr 28 2026"`, 99.1% of 12,426 functions decompiled. On 2026-07-22 we additionally ran a capstone scan that enumerated **all 924 `SetRenderState` call sites** in the image (2 unresolved; 4 "dynamic" sites individually verified to pass 0/bool/param — never a material field) plus targeted decompiles of the zone render pass. All virtual addresses quoted below are from that single build; they will shift in other builds but the *logic* is stable and replicable.
2. **DAT corpus scans.** Full-corpus byte-verification tooling: 625 zone DATs (573 encrypted-placement + 52 plaintext), 4,021 environment records, 37,880 character-geometry blocks, 50,129 DATs total in the block-type census.
3. **Viewer/decoder source RE (weakest, labeled).** AltanaView (2020 community viewer, D3D9) and the Noesis fmt_ff11 lineage. Where a claim comes only from these, we say so — we've been burned this round by exactly that class of evidence (see §1.2), so we now tag it explicitly.

We say "confirmed (retail)" only for machine-code byte-reads of the retail DLL. A house rule we'd suggest for the joint document: **evidence class beats confidence adjective** — retail-disasm > retail-DAT-corpus > reimplementation/viewer source > visual test.

---

# 1. Corrections — things one of our three efforts had wrong

## 1.1 xim item 5 (chunk size formula): the "correction" is itself wrong; DATura had it right

The retail chunk walker executes, literally:

```
0x10176E46:  mov ecx, [ecx+0x24]
             shr ecx, 3
             and ecx, 0x7FFFF0      ; 19-bit size field
```

That is exactly DATura's `size = (info >> 3) & 0x7FFFF0`. xim's `(info >> 3) & 0xFFFFF0` (20-bit) preserves a bit **the retail client masks off**. No real section approaches the ~8 MB limit, so xim's wider mask is harmless in practice — but the framing "DATura drops the top size bit" is backwards. Suggested action for xim: match the 19-bit mask (or keep 20-bit with a comment that retail is 19-bit). Suggested action for DATura: promote your formula to **Confirmed** with this disassembly as the citation.

(War story for the joint doc: we ourselves once used a 17-bit mask, which truncates ≥2 MB zone blocks. 19-bit walks every monolithic zone cleanly.)

## 1.2 DATura's character alpha-test 0.5 is wrong; xim's 69/255 is retail-exact

See §2.1 for the full table. The retail character/actor pass sets `ALPHAREF = 0x45` (69) — xim's 0.27 character discard is the retail value. DATura's 0.5 generic character threshold should be replaced.

We'll own our half of this: **our own previously-published "flat ALPHAREF = 0x20" was wrong for retail too.** It came from AltanaView decoder source — a viewer, not the client. The retail enumeration below supersedes it. This is the concrete case for the "evidence class beats confidence adjective" rule.

## 1.3 DATura's "XYZ Euler" placement rotation is the wrong composition order

xim's item 7 correction is right, and our corpus decode independently agrees: the placement transform is `world = (Rz·Ry·Rx) · (diag(scale) · v) + T` — matrix product `Rz·Ry·Rx`, i.e. per-axis application order X→Y→Z. "XYZ Euler" and "Z·Y·X matrix order" describe the same operation from opposite conventions; the joint doc should print the matrix product explicitly and kill the ambiguity.

## 1.4 xim item 8 ("0x2F is fully parsed") — two refinements from our 4,021-record corpus

Your parse is good but two details matter:

- The packed color at **`+0x54` is the clear color, not a fog color**. Fog colors live *inside* each LightConfig (see §4.1 layout).
- **`fogFar == 0.0` is a fog-disabled sentinel and can coexist with a nonzero `fogNear`.** Never assume `near ≤ far`; treat the pair as (sentinel, value) first. Per-hour records interpolate linearly between bracketing hours, but the fog-off sentinel must stay discrete (don't lerp into or out of it).

## 1.5 xim item 9 (zone lighting model): the structure is right, the sun math is not retail's

Ambient + two directionals + point lights, driven by vertex normals — confirmed. But retail computes **no sun arc**. The directional-light builder (`FUN_10188a80`) constructs two `D3DLIGHT8` (Type=3) reading **pre-baked sun/moon direction vectors straight out of the environment object** (`+0xa4` sun, `+0xc4` moon; colors `+0xe4`/`+0xf4`), then scales colors by a per-channel daylight factor. No trig, no time-angle, no 06:00/18:00 sun↔moon swap. If xim computes an analytic arc, it will diverge from retail at dawn/dusk. Daylight-factor details in §4.2.

## 1.6 DATura's `0x2A` header LOD regions (offsets `0x24`–`0x32`) — flag before you build on it

We could not adopt this as-is: your claimed LOD-region range **overlaps a field we have byte-verified across a 37,880-block corpus** (an f32-pool offset at `body+0x26` in our reading). One of us has a header-origin offset shift (we already found one such shift on the mirror flag — see §5.4), or one reading is wrong. Before either team builds LOD parsing on these offsets, let's reconcile the two header maps against the same file bytes. Happy to exchange raw header dumps for a few known DATs.

---

# 2. Retail state values — reconstructions you can now replace with facts

This section is the core payload: exact D3D8 render state, byte-read from the retail client. Everything here is **confirmed (retail)** unless noted.

## 2.1 Alpha test: per-pass constants, never per-material

`D3DRS_ALPHAREF` is **never sourced from a material or DAT struct** anywhere in the image. It is a constant per render pass:

| Pass | ALPHAREF | ALPHAFUNC | Effective cutoff | Sites |
|---|---|---|---|---|
| Device default | `0x60` (96) | ALWAYS (8) | — | `0x10009b81/91` |
| **Zone pass** | **`0x60` (96)** | **GREATER (5)** | **96/255 = 0.376** | `0x1017b6ef`, `0x1017b6d9` |
| **Character/actor pass** | **`0x45` (69)** | GREATER | **69/255 = 0.271** | `0x1002be86` (restores 0x60/ALWAYS at `0x1002bee8/bed2`) |
| Particle/effect batches | `0x7f` (127) | GREATER | 0.498 | `0x1000c201` |
| UI / second renderer | param (0x7f observed) | **GREATEREQUAL (7)** | — | `0x10281f99`, `0x102e798e` |
| Minimap blit | `0x10` | GREATER | 0.063 | `0x10253b26` |
| Shadows / nameplates / overlays | `0` | GREATER | any α>0 | multiple |

**Ledger moves this enables:**
- DATura zone cutout `0.375` — "described in source as a retail threshold... treated as strongly supported" → **Confirmed** (96/255 = 0.376; the GREATER comparison makes 0.375-as-float the exact working threshold).
- xim character discard `69/255` → **Confirmed**.
- DATura character `0.5` → replace with `0x45`.

**Per-mesh control is only the enable toggle.** Zone geometry carries a runtime short (`geom+0xd6` in the loaded object): `0` = alpha test off + texture WRAP, `1` = **on** + WRAP, `2` = off + CLAMP (sites `0x1017d918`, `0x1017d94e`). So your three-class model (opaque / cutout / blend) maps to: cutout = this toggle on; the *reference* never moves within a pass.

The `0x8010` tail float is **not an alpha-test reference**. `ModelRenderer.cpp:401` consumes the float at state-data `+36` as `TEXTUREFACTOR` alpha after multiplying it by `0.5`. It controls reflection/specular intensity for the environment-mapped second texture stage. The earlier “unlocated alpha-ref” interpretation is retired.

## 2.2 The fragment equation: color ×2, alpha ×4 — xim item 10 confirmed exactly

The zone render-pass prelude (`FUN_1017b420`) is a **recorded D3D8 state block** (created once, handle cached, `ApplyStateBlock` thereafter). Stage 0:

```
COLOROP = MODULATE2X   (ARG1 = TEXTURE, ARG2 = CURRENT/diffuse)
ALPHAOP = MODULATE4X
stages 1–2 disabled
```

A full scan of the zone draw router (`FUN_1017c8d0` through `0x1017dd30`; routes by `switch(model+0x1c)`, FVF `0x152`) shows **zero COLOROP/ALPHAOP writes** — every fixed-function zone draw inherits the sticky stage-0 state. Therefore, retail's zone fragment equation is exactly xim's:

```
rgb = 2 · tex.rgb · diffuse.rgb
a   = 4 · tex.a   · diffuse.a
```

**For DATura:** your "≈1.875 DXT3 alpha expansion" and the historical "left-shift alpha by two" are both artifacts of the same underlying convention — alpha authored on a 0..0x80 scale (0x80 = opaque) on *both* sources, doubled per source in hardware (15/8 is just ×2 seen through 4-bit quantization). We'd recommend deleting the empirical expansion factor from the pipeline and modeling it as MODULATE4X, which also explains why DMB/full-range RGBA atlases must *not* get the expansion: they're consumed outside this state block.

## 2.3 Blending, depth writes, and depth bias — xim's zbias=8 confirmed, and the full rule

- `SRCBLEND/DESTBLEND` are set once to `SRCALPHA / INVSRCALPHA` and **never changed by the zone pipeline**. Additive and exotic blend modes exist only in the particle/UI systems.
- Per material sub-group, one flag (runtime short `grp+0x20`) switches the whole triple:
  - **blend-on:** `ALPHABLENDENABLE=1`, `ZWRITEENABLE=0`, **`ZBIAS=8`**
  - **blend-off:** `ZWRITEENABLE=1`, `ZBIAS=0`
- `ZFUNC` is left at the device default (**LESSEQUAL**) — never touched around these draws.
- Zbias globals (initialized in `FUN_10177c10`/`FUN_101780e0`): opaque `0`, alpha-blended `8`, **sky/celestial billboards `15`** (sites `0x10184cd2`, `0x101850a5`, `0x10185d1d`). There is a driver-workaround variant (gated on a device byte at `+0x920`) that drops the set to 0/1/2/2 — worth knowing if you ever capture on odd hardware.

**Ledger moves:** xim item 12 ("FFXI uses zbias = 8") → **Confirmed** for blended zone layers, with the sky=15 extension. DATura's "small negative depth bias" + depth-writes-off + LEQUAL-family reconstruction → **Confirmed in structure**, and you can now cite the exact retail values. Note the retail comparison function is LESSEQUAL by *omission* (device default), which matches the coplanar-overlay requirement: blended detail layers are genuinely coplanar with their base geometry, and LEQUAL + no-zwrite is what makes them draw at all.

DATura's far-to-near submesh sorting remains a viewer reconstruction — we have not yet byte-read retail's transparent traversal order (open item, §6).

## 2.4 Culling: per-pass CCW default with per-group disable — your per-batch reading is mechanistically right

38 `CULLMODE` sites enumerated:

- The **zone pass defaults to CCW (3)** each pass. (The "everything is globally two-sided" claim you may have seen from viewer-derived sources — including ours — is viewer behavior, not retail.)
- **Per-material-group cull disable exists:** runtime short `grp+0x24 != 0` → `CULLMODE = NONE` (site `0x1017dab6`).
- Per-geometry winding flip: runtime short `geom+0xd4` → CW (2). (Consistent with your negative-scale-determinant handling.)
- Outside the zone pass the device default is NONE.

The DAT-to-runtime mapping is now traced end to end. `MeshBlockResource.h:33–39` reads the dword immediately after the 16-byte texture name as `VerticeCountAndFlags`: low 28 bits are the vertex count, `0x80000000` becomes runtime u16 flag `0x8000` (`IsTransparent`), and `0x20000000` becomes `0x2000` (`DisableCulling`). `0x40000000` and `0x10000000` become two additional runtime flags whose effects remain unnamed. `MeshBlockManager.cpp:514–517` consumes the transparent/cull flags. The old “high nibble is a color multiplier” interpretation is therefore retired.

## 2.5 Fog: linear only, and where the values come from

- World fog is **linear only**. Mode is capability-selected per device: TABLE fog if caps byte (`device+0x92d`), else VERTEX fog (`+0x92e`); RANGEFOG per `+0x92f`; the shader path forces vertex fog. Your linear formula `f = (far − d)/(far − near)` (xim item 8) is the right model.
- **`FOGDENSITY` has exactly one call site in the entire image** — the title/login scene (`FUN_10013a90`: fixed color `0x80808080`, start 80, end 100). In-game fog never uses density. If either renderer exposes an exponential-fog path for FFXI content, it's dead weight.
- Value flow: `FUN_1018b930 → FUN_10189030` (defaults: color `0x808080`, start 0, end 400), with day/night colors and distances pulled from the `0x2F` environment record via small getters and blended, then applied to the device from context fields (`FOGCOLOR = ctx+0x39254`, `FOGSTART = ctx+0x3925c`, `FOGEND = ctx+0x39258`). There is a per-mesh environment override slot (`model+0xe0`) — individual meshes can bind a different environment record (this pairs with the placement record's per-object environment link, §3.1).

**This closes DATura's fog section** ("Speculation: fog parameters may reside in environment (`0x2F`) data, zone tables, scripted weather resources, executable constants, or some combination") — the answer is: `0x2F` records (per-LightConfig fogNear/fogFar + fog color) combined with the weather system, applied linearly, with executable constants only as defaults.

---

# 3. Answers to DATura's "Unknowns and prioritized research plan"

Keyed to your numbered list.

**#1 "Decode character `0x8010` draw state."** The reflection path is now closed: state-data `+16` enables the camera-normal cubemap stage, and float `+36 × 0.5` supplies its `TEXTUREFACTOR` alpha/intensity. See §5.3 and §5.5. Other display/blend/lighting fields still need full naming.

**#2 "Map zone state words completely."** The key DAT-side mapping is now confirmed: the post-texture `VerticeCountAndFlags` dword uses low 28 bits for vertex count; `0x80000000` and `0x20000000` become runtime transparency/cull-disable flags. The `0x40000000` and `0x10000000` behaviors, `flags2`, winding, and alpha-test/addressing interactions remain to be completed.

**#3 "Prove the fragment equation."** Done — §2.2. No GPU capture needed; the disassembly settles the operator, the ×2/×4 factors, and the blend factors.

**#4 "Determine native mip layout."** Partial, two leads:
- Your Konschtat observation (every native DXT payload followed by 11 alignment bytes, no second level) matches ours: DXT payloads are top-level-only.
- There is a **native multi-image texture container you don't list**: texture marker byte `0x05` — a palettized "MULTI" (5-image) container with **no inline palette** (palette pointer at `p+57`, indices at `p+57+1024`, per the AltanaView decoder's `IMGINFO05` struct — viewer-source evidence, so treat as a lead). Whether its image set is a mip chain is unverified; it's the best candidate for "other payload classes contain additional levels." Retail-side mip *sampling* state (MIPFILTER/LOD bias/auto-gen) is still untraced (§6.4).

**#5 "Decode placement-table tail."** Done at the map level — §3.1 below.

**#6 "Complete `0x2A` LOD stream parsing."** Blocked on the header-map conflict (§1.6) — reconcile first.

**#7 "Determine the extra 12 bytes in 48-byte map vertices."** xim answered this (second position; wind-sway blend target, lerped by a global wind factor — their item 3). We've adopted it as the working model; our corpus confirms the 12 bytes' existence and position, and the vegetation-family correlation you observed in Konschtat (`_con_hana_*` carrying 48-byte batches) fits it. Independent retail confirmation still pending; treat as strong theory.

**#8 "Complete animation translation/scale semantics."** **Solved — this was your "collapsing limbs" bug.** The composition rules (viewer-source evidence, but validated against 570/570 corpus blocks and a live cutscene reproduction):

- Translation is a **delta on bind**: `local_trans = bind_trans + anim_trans`. Rotation composes `anim_quat ⊗ bind_quat` (Hamilton, xyzw). Scale = animation scale only.
- Per-bone record: 20 u32 slots — s0–s3 rotation key offsets, s8–s10 translation key offsets, s11–s13 translation statics, s14–s16 scale offsets, s17–s19 scale statics. **Offset 0 means the channel is constant** — use the paired static value. A negative offset parks the whole joint.
- Per-channel constants are taken **modulo 10000** (encoding quirk — miss this and limbs drift).
- **Root joint (index 0) is special:** ignores its own scale; its translation is not in skeleton space (rotated 270°).
- Timebase: `duration_s = (nFrames − 1)/(rate × 30)` — your `speedScale * 30` is right; promote from "implementation interpretation" to confirmed.
- Build **two** world-matrix chains per bone: full `S·R+T` for positions, rotation-only for normals (normals never see scale).

Applying indexed translation *absolutely* (instead of as a bind delta), or treating offset-0 as "read from array start," is exactly what collapses limbs.

**#9 "Decode `0x2F` environment records."** Full layout, byte-verified 4,021/4,021:

```
+0x08  u32   indoorFlag           0 = outdoor gradient, 1 = indoor flat
+0x14  LightConfig MODEL         (0x20 bytes, layout below)
+0x34  LightConfig TERRAIN       (same shape)
+0x54  u32   clearColor           packed; NOT fog (see §1.4)
+0x60  f32   drawDistance         distinct from fogFar
+0x66  u16   sphereSpokeCount
+0x70  f32   skyBoxRadius         ~2029.5, effectively fixed
+0x74  u32[8] skyDomeRingColors   horizon → zenith
+0x94  f32[8] skyDomeElevations   phi = 0.5·pi·elev
+0xB4  u32   terminator           (total 184; rare 136-byte variant truncates after ring 5)

LightConfig (0x20 bytes): packed colors sun, moon, ambient, fog;
                          f32 fogFar; f32 fogNear; f32 diffuseMult
packed color = u32, R = u&0xFF, G = (u>>8)&0xFF, B = (u>>16)&0xFF, top byte constant 0x80
               (it is a marker, not alpha, and these are NOT f32 triples)
```

There is **no sun-direction vector in the DAT** — directions are pre-baked into the runtime environment object (§1.5). Records are per-hour (tags 0000/0600/1200/1800 + half-hours), linearly interpolated between bracketing hours, fog-off sentinel held discrete. Unclassified residue we're still carrying: `+0x58/+0x5c` (read-and-discarded), a `+0x64` selector u16, a packed color at `+0x68`, and why exactly 2 corpus records use the 136-byte variant.

**#10 "Decode `0x21`/`0x25` effect systems."** We have deep coverage here — summary in §5.6, full catalogs on request.

**#11 "Identify true shadow resources."** xiclient now has the blob-shadow path decompiled in `ZoneRenderer.cpp:1624ff`: it uses the `SYSTEM_KAGE` texture, `ShadowZBias = 15`, disables depth writes, applies dedicated shadow-scale vectors, and routes through dedicated shadow render functions. Static/environment shadow semantics and the outer DAT `is_shadow` resource bit remain separate questions.

**#12 "Test for native multi-texture / env-map / DOT3."** See §5.5 (the "shiny" three-way) — this is now the largest genuinely open rendering question and the cleanest joint experiment.

### 3.1 The placement record and the `0x1C` tail (your #5, and xim item 7 completed)

Full 0x64-byte placement record, byte-verified 573/573 across all encrypted-placement zones:

```
+0x00  char[16]  id                 (XOR-0x55 obfuscated mesh id)
+0x10  f32[3]    position
+0x1C  f32[3]    rotation           (radians; R = Rz·Ry·Rx — see §1.3)
+0x28  f32[3]    scale
+0x34  4CC       effectLink         particle-effect id, 0 = none
+0x38  f32       highDefThreshold   \
+0x3C  f32       midDefThreshold     } per-object LOD distances
+0x40  f32       lowDefThreshold    /  (= draw distance)
+0x44  u8        flags0
+0x45  u8        flags1             bit1 = skip during decal rendering
+0x46  u8        flags2
+0x47  u8        flags3
+0x48  u32       cullingTableLink
+0x4C  4CC       environmentLink    per-object environment record, 0 = none
+0x50  u32       fileIdLink         ← not in either of your docs
+0x54  u32[4]    pointLightIndex0..3  (1-based into the point-light table)
```

This completes xim's item-7 partial decode (adds exact offsets, the flag-byte split, and `fileIdLink`) and answers DATura's `float4`/`int32[8]` unknowns.

**`_h`/`_m`/`_l` selection (DATura: "how retail chooses... Unknown"):** the LOD upgrade is gated on `highDefThreshold`. Corpus-wide, `highDefThreshold == 0` occurs *only* on `_m` objects that have no `_h` sibling — i.e. the threshold gate is byte-identical in effect to the legacy name-suffix swap, and the three thresholds are the retail distance selectors. DATura's `_m`→`_h` fallback is therefore the right behavior; you can now drive it from the real field.

**The placement-table tail** (your "Speculation: visibility, spatial partitioning..."): correct, and now located. The encrypted-placement header (0x20 bytes) carries four sub-section offsets:

```
+0x08  collisionMeshOffset
+0x10  spacePartitioningTreeOffset   (quadtree root)
+0x14  cullingTablesOffset           (visibility sets; per-object link at +0x48)
+0x18  pointLightOffset
```

All four validated in-bounds corpus-wide. The quadtree/culling-table *internals* are still undecoded (16-byte octree descriptor observed: `[u32 vtxOffset][u32 0][u32 octreeKey][u32 indexBase]`, key high byte ramping `0x05..0x0f`) — a pure file-format task, no disassembly needed, if either of you wants to race us to it.

**Point-light table:** 256 records, stride 0x4C: a `char[16]` `"pl<NN>"` name handle + 60 zero bytes. **No inline color/radius/intensity/position** — parameters are runtime-resolved by handle; in city zones the world position comes from a separate `obj_light` marker object (verified: South San d'Oria `pl38` ↔ (160.7, −5.4, 165.7)). Where the runtime photometric parameters come from is open (§6).

---

# 4. Answers keyed to the remaining xim/cexi items

**Item 1 (`0x91` bitCount / raw 32-bit):** independently confirmed. Our header split of DATura's "six unknown 32-bit fields" region: `+0x1D u16` constant (observed 1), `+0x1F u16 bitCount` (32 = raw BGRA, no palette; else 8bpp palettized), five zero u32s, `+0x35 u32` palette entry depth (0x10/0x20).

**Item 2 (config byte, two independent bits):** adopted as the cleaner model over our stride-selector framing; both describe the same storage. DATura's single-flag table (0 → 48-byte list / nonzero → 36-byte strip) should be replaced with the two-bit decomposition.

**Item 4 (`0x5D` BumpMap):** agreed and extended: 8-bit height field, Sobel-style gradient with wrap-around, encoded `r = nx·0.5+0.5, g = ny·0.5+0.5, b = nz`, and the resource is linked by the *local* half of the texture name (see below). Your TBN-sampling claim is the only runtime-consumption model any of us has; for the joint doc note that retail is D3D8-era, so the underlying mechanism is presumably DOT3/bump-env stages — unverified. DATura's "no confirmed native per-pixel normal-map binding" should be softened to "none in the common material path; `0x5D` is a dedicated native resource."

**Item 6 (collision masks):** adopted, with thanks — your byte-exact `p1/p2 & 0x3FFF`, `p0`/normal `& 0x7FFF` closes an open question of ours. In exchange, the material word decode (§5.2).

**Items 13/14/15/16:** see §5.1, §5.3, §5.5, §5.7 respectively.

**Item 17 (DXT5 never observed):** three-way agreement, now as strong as a negative gets: your two corpus scans + our 1,345-image-DAT scan (only `0xA1`/`0xB1` markers present) + the observation that neither AltanaView's decoder nor any native path has a DXT5 branch. Safe to state in the joint doc: *FFXI ships no DXT5; accepting it is tool tolerance.*

**Texture extras neither doc has:**
- **`0x01` vs `0x91`** (DATura: "distinction unknown"): `0x01` is a *different container block type* (type 27 / 0x1B) — an 8-bit palettized DIB with **no format dword** before its 256-entry BGRA palette, consumed by a different code path (xim's own in-model parser accepts only `0x91/0xA1/0xB1`, never `0x01` — evidence in your favor you may not have noticed). Observed type-27 tags: `tesc`, `flar`, `tex2`, `spec`, `cele`.
- **`0xB1` leading dword** (DATura: "purpose unknown"): a format code; observed values **9 and 10**; parsed identically either way.
- **`0x81` which-copy rule** (DATura: "likely DXT"): per decoder source, the DXT copy is used **iff a valid `3TXD` trailer is present**; the DDS half is not always there. (Viewer-source evidence; retail-side unproven, but it's a deterministic rule rather than a preference.)
- **Name resolution:** the 16-byte name is two 8-char halves — `nameSpace` + `localName`. Resolution tries the exact pair, then falls back to `localName` alone, then a global table. Bump maps key on `localName` only. This explains cross-DAT texture redirects that look like magic if you treat the name as opaque.
- **Palette formats:** your B8G8R8A8 (32-bit) / B5G5R5A1 (16-bit) decode matches the `+0x35` depth field values (0x20/0x10).

---

# 5. Material we hold that fills remaining gaps in both docs

## 5.1 The two "Unknown" `0x2A` draw commands — and two more you don't list

Confirms and extends xim item 13 (byte-verified, 37,880-block corpus):

- **`0x0043`** = untextured **vertex-colored triangle list**: 10-byte records `[u16 i0][u16 i1][u16 i2][u32 BGRA]`, no UVs.
- **`0x4353`** (ASCII "SC") = untextured **single-color triangle strip**: 10-byte head (first three indices + one shared BGRA), then `count−1` bare u16 indices; `verts = count+2`, `tris = count`, alternating winding.
- Plus two markers absent from both your docs: **`0x40`** (10-byte strip vertex `[u16 idx][f32 u][f32 v]`) and **`0x0000`** (vertex-colored tri list, byte-identical to `0x0043`).

Decoding these recovered 37 blocks our parser previously dropped entirely (fingernails, `wep3`-class weapons) with zero regressions — expect the same in DATura for any DAT that currently errors on `0x4353`.

## 5.2 Collision: the material/terrain word, transforms, and runtime behavior

Extends cexi's item 6 (clean-room from reimplementation source; treat as strong theory):

- Material word `= (f0<<12)|(f1<<8)|(f2<<4)|f3` (the four top nibbles). Bit `0x40` = **"hit wall"** (one-way / invisible collision).
- Terrain type assembles from the four `&0x8` bits into an 11-value enum (Object/Path/Grass/Sand/Snow/Stone/Metal/Wood/ShallowWater/DeepWater/…), each mapping to a footstep-effect DAT id of the form `0<hex>00`.
- Per-mesh transform records carry **both the to-world and the inverse 4×4**; a packed misc word yields the map id (`8*((misc>>26)&3) + ((misc>>3)&7)`); each collision mesh also carries an environment-link DAT id + four light indices (so collision cells know their lighting/environment context — relevant to your per-object environment observations).
- Runtime resolution behavior, if you ever need gameplay-accurate walking: 0.05-unit stepping, candidate sort by `abs(normal.y)`, wrong-side culling, step-up gating.

## 5.3 The `0x8010` draw-state record (DATura's #1 "most important gap")

Our field map for the 0x30-byte state block (corpus-verified layout; display-type semantics byte-confirmed against retail data files):

| Offset | Field | Values |
|---|---|---|
| byte +5 | blend mode | `0x80` soft-blend / `0x00` opaque |
| byte +15 | **displayType** | 0 ordinary; 1/2/3 hair; 4 face skin; 5/6/7 wrist/pants/shin — drives hair/skin occlusion culling (verified against `ROM/37/38`: `hh_h = [1,2,3,4,3,4,3,1,4]`) |
| byte +16 | env/reflection pass | float `1.0` → enable the camera-normal cubemap stage; don't alpha-test this section |
| f32 +18 | brightness multiplier | default 1.0 (likely your *ambientMultiplier*, xim item 14 — reconcile before duplicating) |
| f32 +38 including the command word (`+36` from state data) | reflection/specular intensity | `ModelRenderer.cpp:401` writes `value × 0.5` to `TEXTUREFACTOR` alpha; not an alpha-test reference |
| char[16] | texture name | |

Section variants: `0x8010` full state / `0x8000` texture-swap only (state inherited) / nameless `0x8010` = state-only. Alpha test is enabled regardless of the blend byte. xim's *tFactor BGRA*, *specularHighlightPower*, and *specular-enable == 1.0* fields (item 14) are the remaining unpinned candidates — if you can share the byte offsets within the record that `SkeletonMeshSection.kt` reads for those three, we can corpus-verify them same-day.

## 5.4 Character mirroring and skeleton extras

- The mirror flag is a **u16 at `body+0x0C`** in our header origin: `1` = HALF mesh (generate z-mirrored copy about the skeleton root plane), `0` = FULL. This is the same field DATura places at header byte `0x04` — a header-origin shift, not a disagreement; worth normalizing origins in the joint doc (this is also why §1.6 needs care).
- Per-vertex bone bitfield: bits 0–6 this-copy bone, bits 7–13 mirror-partner bone, bits 14–15 negate axis (0 none / 1 x / 2 y / 3 z; `0xC0` = z is ubiquitous). Authoritative over `hh_*`-style name heuristics.
- Skeleton extras beyond your `0x29` layout: an attachment-point table (`[u16 jointIndex][f32×3][f32×3 posOffset]`; standard indices: above-head 2, feet 8/9, hands 126/127), a bounding-box run terminated by `0xCDCDCDCD`, a 128-bone identity fallback for skeleton-less DATs, and runtime weapon-handle re-parenting to hand joints.
- Two-weight skinning reconstruction (matches your unconventional-position observation): `pos = R_a@p1 + w1·t_a + R_b@p2 + w2·t_b` — each influence transforms its *own* stored position; weights apply to translation via homogeneous w, exactly as DATura reconstructs. Verified 544/544 triangles on a controlled mesh.

## 5.5 The "shiny" mechanism — resolved

`ModelRenderer.cpp:398–415` closes the three-way question. The `0x8010` reflection state configures texture stage 1 with `D3DTSS_TCI_CAMERASPACENORMAL` plus a texture transform, `COLOROP = MODULATEALPHA_ADDCOLOR`, and `ALPHAOP = ADD`. Texture resolution falls back to `CubeTex`. Thus the formerly competing readings were complementary: this is a second fixed-function texture stage, and that stage performs cubemap environment mapping. The `cubemap*` DXT3 resources and the additive reflection-mask behavior are parts of the same retail path. DATura's generated flat normal/specular material remains only a viewer approximation and should be replaced by this stage.

## 5.6 Effects: what we can hand over when you get there

DATura's effect section ends at "requires an effect simulation runtime." When either of you builds one, we hold (and will share on request — it's too large for this document):

- The full type-`0x05` generator opcode catalog **with numbers**, per section: emission-frequency/velocity/spherical-position/culling (Sec1 `0x04–0x14`), initializers incl. particle setup, blend-func nibble, ring meshes, child generators, keyframe-value binding, point lights, specular, day/night color (Sec2 `0x01–0x9B`), updaters (Sec3), expiration incl. child-emit (Sec4). Crucially, **(section, opcode) routing is load-bearing** — the same opcode byte means different things per section.
- The per-node `render_mode` word bit map: billboard-type ladder (camera / movement / movement-horizontal / XZ / XYZ / none — **effect cards are not universally camera-facing**), camera-vs-world space gate, world-freeze bit, per-node depth-test disable, card euler order (`Rx·Ry·Rz`, rz innermost).
- A 5-way blend-mode decode (add / alpha / reverse-subtract-darken / zero-invsrc / opaque) with a 12,166-record corpus census, and the finding that card texture binding comes from the geometry block, not the node (mis-binding this corrected 61,250 of 195,372 nodes for us).
- Retail-side: the effect pipeline's sticky MODULATE2X/MODULATE4X inheritance (same state block as §2.2), 1/frame integration, and a D3D8 device-vtable map (SetRenderState `+0xC8`, SetTextureStageState `+0xFC`, SetTexture `+0xF4`, DrawIndexedPrimitive `+0x11C`, SetVertexShader `+0x130`; effect FVFs `0x152`/`0x142`) — useful to anyone hooking or tracing the client.
- Your `0x19` keyframe pairs: consumed as keyframe curves by the generator's curve-binding opcodes; the second float's universal semantics outside that context we haven't pinned either.

## 5.7 Section-type map (xim item 16): byte-level layouts behind the names

Your name list matches our census (closed at **38 distinct block types over 50,129 DATs** — beware a phantom "type 117" scan artifact if you census with naive alignment). Byte-verified layouts we can share per type: `0x54` weapon-trace = two-rail quad-strip ribbon (0x20 header; 4,857/4,857 blocks), `0x5E` blur = `{trail offset, RGBA}` afterimage taps (69/69), `0x30/0x31` UI menu/element records (2,579 + 1,759), `0x3E` point lists + `0x4A` path graphs (763 + 197), `0x45` info = 16-byte metadata records, `0x06` camera = keyframe/roll/normalized-time semantics. Also `0x49` spell-list and `0x53` ability-list share a third cipher (popcount→rotate) distinct from the two zone schemes — relevant if you extend decryption beyond `0x1C`/`0x2E`.

Encryption specifics beyond DATura's sketch, if useful: scheme gates (`mode <= 0x1A` plaintext for the `0x1C` family; `mode < 5` for MMB), keyed-run XOR with run length `((key>>4)&7)+16`, the per-record name XOR-0x55 you already have, and the fact that the two 256-byte key tables live in `FFXiMain.dll` and can be located in any build by content scan (first-dword anchors `0xE2E506A9` and `0xB8C5F784`).

---

# 6. Open items and proposed division of labor

In rough value order:

1. **Closed: MMB loader trace.** The post-texture dword is `VerticeCountAndFlags`; low 28 bits are count and high bits map directly to runtime flags (§2.4).
2. **Closed: the `0x8010` reflection path.** The cubemap second stage and its texture-factor intensity consumer are identified (§5.3, §5.5).
3. **The shiny/env-map three-way** (§5.5): joint experiment as proposed.
4. **Mip/sampler state**: retail `SetTextureStageState` sampler calls (MIPFILTER, LOD bias, auto-gen mips) — we'll trace; DATura's payload-size-vs-level-size corpus check remains the right file-side complement, plus the `0x05` MULTI container question.
5. **Transparent traversal order**: does retail sort blended sub-groups, and how? Untraced; our anchors are ready.
6. **Daylight curve writer**: the three u16 channels the daylight getter reads (env object `+0xf4/+0x2ec/+0x504`, scale 2.3/65535, modulation gated at base RGB < 0.8) are written by an unlocated per-tick function in the weather/time system. We'll continue.
7. **Quadtree + culling-table internals** (§3.1): pure DAT decoding, no disassembly needed — first mover wins, we'll happily consume your result.
8. **Point-light runtime parameters** for `pl<NN>` handles: not in the zone DAT; source unknown.
9. **Shadows**: unstarted everywhere; the `system/kage.tex` loader at zone-init is the only known foothold.
10. **Character LOD selection** and **DMB/character-creation compositing**: no located retail path yet; honest blank for all three efforts.

A note on captures: several of your research-plan items assumed a retail GPU capture would be needed. The disassembly has now settled the fragment equation, alpha test, blend, bias, and cull questions without one — but a single frame capture would still be the fastest adjudicator for #5 (draw order) and #3 (shiny), if anyone has a capture-capable setup.

---

*Corrections welcome — especially counterexamples. Every claim above states its evidence class; if your data contradicts a "confirmed (retail)" item, the build difference is itself a finding worth recording (our image is the Apr 2026 PC client). For anything marked "on request" (effect opcode catalogs, per-type layouts, the 924-site render-state scan, raw decompile excerpts), ask and we'll package it.*
