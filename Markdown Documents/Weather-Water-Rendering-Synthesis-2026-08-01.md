# FFXI Zone Weather & Water Rendering — KB Synthesis (2026-08-01)

A handoff document for collaborating developers, synthesized from our FFXI reverse-engineering
knowledgebase. It covers **how the retail client decides, styles, and renders weather in a zone**,
and **what "water" actually is in FFXI zones and how to render it**. Everything below carries an
honest evidence grade; where we do not know, the document says so instead of guessing, and the final
section proposes experiments to close each gap.

All implementation suggestions are **language-agnostic pseudocode** — adapt to whatever your
project uses.

---

## 0. How to read the evidence grades

Every claim is tagged with the KB's confidence tier:

| Tag | Meaning |
|---|---|
| **[confirmed]** | Byte-read from retail machine code / corpus-verified across thousands of records, or interop-proven on the wire. |
| **[tested]** | First-hand reproduction (working renderer / parser / decompile read), not yet fully cross-checked at the byte level everywhere. |
| **[theory]** | Re-expressed from a reimplementation (Xim/xiclient/cexi lineage) or community docs; plausible, NOT independently reproduced. |
| **[unverified]** | A pointer/label only — nobody has parsed the bytes. |
| **[OPEN]** | Genuinely unknown. Do not build load-bearing code on it. |
| **[DISPUTED]** | Two credible readings disagree; both are given. |

Two global caveats:

1. **Function/global addresses quoted here (`FUN_...`, `DAT_...`, `ctx+0x...`) are single-build
   virtual addresses** from one unpacked `FFXiMain.dll` image (base `0x10000000`, snapshot
   2026-07-22). They relocate on every client patch and are **not portable signatures** — treat them
   as evidence citations, not constants to hardcode.
2. **Provenance/licensing:** some facts were behaviorally cross-checked against GPLv3/AGPLv3
   projects (Xim, LandSandBoat, XiPackets, cexi) and are re-expressed clean-room — facts only, no
   code. The retail-confirmed items rest on our own first-hand decompiles. If your project has
   licensing constraints, note that the **LSB `zone_weather` table data** carries GPLv3 provenance
   if you vendor it.

---

## 1. The big picture

**Weather is three separate layers.** Keeping them distinct is the single most useful mental model
we have:

| Layer | What it is | Where it lives | Grade |
|---|---|---|---|
| 1. Weather **id / schedule** | Which weather id (0x00–0x13) is active in a zone right now | Server packet 0x057 + zone-in seed; deterministic per-zone schedule table | confirmed (wire) / tested (schedule) |
| 2. Weather **environment appearance** | Fog, sun/moon/ambient colours, sky dome, clear colour, draw distance — per weather, per in-game hour | DAT block **type 47 (0x2F)** records inside the zone's own DATs | tested (byte-pinned layout) |
| 3. Weather **particles** | Rain/snow/etc. falling-particle visuals | Type-5 particle generators, special "batched" path | theory |

**Water is NOT a subsystem.** We found no dedicated water renderer, no water-surface file format,
and no water shader in the retail client. What players see as water is a combination of:

- ordinary **placed zone geometry** (e.g. a mesh literally named `water4` in a zone's placement
  table), drawn through the normal zone pipeline with the alpha-blend material class; and
- **ambient effects embedded in the zone DAT** (type-5 generators) that place otherwise-unplaced
  "orphan" meshes — ocean, sea, waterfalls, fountain spray — and animate them (UV scroll, curves,
  particles).

One community theory that a dedicated water-surface block existed (block type 74 "RAB") was
**byte-refuted**: it is an NPC path/route graph (details in §5.4).

---

## 2. Weather layer 1 — which weather is active

### 2.1 The weather id enum **[confirmed]**

Ids 0x00–0x13 are the primary set, each with a 4-char resource tag used to key environment records:

```
0x00 fine   0x01 suny   0x02 clod   0x03 mist
0x04 dryw(Fire)   0x05 heat(Fire x2)   0x06 rain(Water)  0x07 squl(Water x2)
0x08 dust(Earth)  0x09 sand(Earth x2)  0x0A wind(Wind)   0x0B stom(Wind x2)
0x0C snow(Ice)    0x0D bliz(Ice x2)    0x0E thdr(Ltng)   0x0F bolt(Ltng x2)
0x10 aura(Light)  0x11 ligt(Light x2)  0x12 fogd(Dark)   0x13 dark(Dark x2)
```

Ids 0x14–0x27 are a parallel "variant" tag set (`fin1`, `rai1`, …) with no weather icons. Their
purpose is **[OPEN]** — never observed live. Don't build on them.

### 2.2 Server sets weather: packet 0x057 **[confirmed — interop-proven]**

`GP_SERV_COMMAND_WEATHER`, fixed 12 bytes (4-byte header + 8-byte payload):

| Payload offset | Type | Field | Client-side scaling |
|---|---|---|---|
| +0x00 | u32 | StartTime | × 60 (then the clock store multiplies by 25 internally) |
| +0x04 | u16 | WeatherNumber | the id above |
| +0x06 | u16 | WeatherOffsetTime | × 3600 |

**Load-bearing correction:** `WeatherOffsetTime` is a **fade/transition offset, NOT a
"seconds until next weather change" countdown** — a common documentation error. LandSandBoat sends
a small random 4–28 for it (a transition window). Do not treat it as a scheduler.

Capture gotcha if you test this: forcing a weather that equals the zone's *current* weather sends
**no packet** (the server early-returns). Always force a *different* weather.

### 2.3 Zone-in seeds initial weather **[tested]**

The zone-login packet (0x00A) carries both an **immediate weather** (applied on entry) and a
separate **transition-target weather** with a transition window (start/end). They are distinct
fields — model them separately.

### 2.4 Offline/deterministic selection **[theory — algorithm reproduced, data equivalence unproven]**

Retail weather is server-authoritative but **deterministic and predictable**: one weather per
Vana'diel day per zone, from a fixed table, on a **2160-Vana'diel-day cycle** (= 6 Vana'diel years
= 86.4 real days).

```
VANA_EPOCH  = 1009810800          # unix seconds
VANA_DAY    = 3456                # real seconds per Vana'diel day

function weather_for(zone_id, unix_now):
    day   = ((unix_now - VANA_EPOCH) / VANA_DAY) mod 2160
    # IMPORTANT: zero-day carry-forward — a day whose table value is 0 is a
    # "no entry" sentinel, NOT weather 0x00 (Fine). Scan backward to the last
    # non-zero day at or before `day`.
    while table[zone_id][day] == 0: day = day - 1   # wraps within the cycle
    packed = table[zone_id][day]                     # little-endian u16
    normal = packed >> 10                            # the deterministic pick
    common = (packed >> 5) & 0x1F                    # secondary candidate
    rare   =  packed       & 0x1F                    # rare candidate
    return normal
```

- Table source: LandSandBoat's `zone_weather` SQL (~300 zones × 2160 u16). **The zone id needs no
  remap** — the canonical FFXI zone id indexes the table directly (spot-verified for zone 230).
- **Honest limits:** the bit-unpack, carry-forward, cycle length, and zone-id identity were
  reproduced first-hand in tests against the vendored LSB blob. **Whether LSB's table data exactly
  matches retail's is unvalidated** — that's the open half. LSB's own *runtime* rolls RNG
  (15% rare / 35% common / 50% normal, reschedule every 3–30 real min) and is explicitly **not**
  retail-faithful; use the deterministic model, not LSB's roll, if you want retail behavior.
- Regression fixture: zone **230** (Southern San d'Oria): day0 = Sunshine, day7 = Clouds, day1
  carries day0 forward.

### 2.5 The client also ships the schedule **[tested]**

The client itself carries a per-zone weather schedule resource (consulted by event-VM opcode 0x72):
DAT file id **0x1B79** (zones 0–99) / **0x1B7D** (zones 100+), laid out as **2160 days × 3 bytes
per zone-row** (stride 0x1950 per zone-row; the 3 bytes mirror normal/common/rare). Byte-recovered
from the client decompile.

Two adjacent file ids **0x1B78 / 0x1B7C** are labelled "weather base" by a community toolkit but
are **[unverified]** — nobody has parsed them. A community claim that they hold "weather appearance
parameters" is unsupported; the appearance data we can actually point at bytes for is the type-47
record (§3). Additionally, at zone-in the client binds a per-zone weather table by 4CC **`'weat'`**
from inside the zone's main DAT — that in-DAT carrier block is identified but **not yet parsed**
**[OPEN]**.

---

## 3. Weather layer 2 — environment appearance (the part that changes how the zone LOOKS)

This is the highest-value, best-proven data for a renderer.

### 3.1 The type-47 (0x2F) per-hour environment record **[tested — byte-confirmed 4021/4021 records]**

Zone DATs carry, per **sub-area**, per **weather type**, a set of **per-in-game-hour** environment
snapshots (record tag = the hour: `0000`, `0600`, `1200`, `1800`, plus half-hours). Addressing is
a tree: `area / <environmentId> / <weatherType> / <hourRecords>`, where `weatherType` is one of the
**20 4-char tags** matching the enum in §2.1 (fall back to sunny/`0000` when a specific one is
absent). The `environmentId` (e.g. `ev01`) is selected by the collision surface you stand on — each
collision object carries an environment-link DatId; `0000` means "use the zone's main weather
environment" (grade for the addressing model: **[theory]**, Xim-lineage; the record layout itself is
byte-pinned).

**Record layout — 184 bytes, little-endian, offsets relative to record body:**

| Offset | Type | Field |
|---|---|---|
| +0x08 | u32 | `indoorFlag` — 0 = outdoor (real sky gradient), 1 = indoor (flat sky) |
| +0x14 | LightConfig | **MODEL** lighting (for actors/objects) |
| +0x34 | LightConfig | **TERRAIN** lighting (for zone geometry) |
| +0x54 | packed colour | `clearColor` |
| +0x60 | f32 | `drawDistance` (far view distance; distinct from fog far) |
| +0x66 | u16 | `sphereSpokeCount` (sky-dome radial spokes, 0–21) |
| +0x70 | f32 | `skyBoxRadius` (~2029.5 in practice) |
| +0x74 | 8 × packed colour | sky-dome ring colours, horizon → zenith |
| +0x94 | 8 × f32 | sky-dome ring elevation stops, monotonic 0→1 |

LightConfig (0x20 bytes): `sun`, `moon`, `ambient`, `fog` (packed colours), then `fogFar` (f32),
`fogNear` (f32), `diffuseMult` (f32). **Note the order: far BEFORE near. Do not assume
near ≤ far.**

**Packed colour decode** — a u32 whose low three bytes are **R, G, B** (low→high) and whose top
byte is a constant `0x80` marker:

```
R = u & 0xFF;  G = (u >> 8) & 0xFF;  B = (u >> 16) & 0xFF
```

We shipped a real R/B-swap bug reading this the other way (noon skies rendered orange). Sanity
check: an outdoor noon zenith ring should decode blue-dominant; a dusk horizon ring warm.

**Fog-off sentinel:** `fogFar == 0.0` means **fog disabled** — a discrete state, possibly with a
nonzero `fogNear` beside it.

A rare 136-byte truncated variant exists (2 of 11,109 corpus blocks; no elevation table).
Unclassified slots remain at +0x58/+0x5c/+0x64/+0x68 **[OPEN]**.

### 3.2 How the client consumes it — hour interpolation **[tested, GPU-QC'd]**

The active environment at a given Vana'diel minute is a **linear blend of the two hour records
bracketing that minute**, wrapped across midnight:

```
function active_environment(records, minute_of_day):
    (before, after, t) = bracketing_records(records, minute_of_day)  # wraps at 0000
    env = lerp_fields(before, after, t)
    # Blend linearly: model+terrain sun/moon/ambient/fog COLOURS, fog planes
    # (fogFar/fogNear), clearColor, drawDistance.
    # EXCEPTION: treat fogFar == 0.0 (fog off) as DISCRETE — never lerp a real
    # fog plane through ~0, or you get a momentary near-camera fog wall.
    return env
```

On a **weather change**, the client cross-fades environments over roughly **3.33 s**
**[theory — Xim-lineage figure]**.

### 3.3 Lighting: pre-baked directions, NOT a computed sun arc **[tested — key correction]**

This corrects a widespread assumption (and Xim's reimplementation): the retail client does **not**
compute the sun direction from the clock. The directional-light builder reads **pre-baked sun and
moon direction vectors from the active environment record** and merely **scales the colours** by a
per-channel "daylight factor". No trig, no time-angle, no 06:00/18:00 sun↔moon swap exists in the
builder.

- Daylight factor **[tested]**: three per-channel u16s scaled by `2.3/65535` (range 0..2.3 —
  overbright is possible), applied only when an enable flag is set AND the base colour channels are
  < 0.8, then clamped to 1.0. **What writes those three u16s per tick (the actual daylight curve)
  is [OPEN]** — the single remaining unknown in the daylight chain.
- Indoor records: flat sky; the moon-colour slot doubles as a fixed diffuse *direction*
  (channels signed /128) **[theory]**.

```
function build_directional_lights(env, daylight):
    sun.dir    = env.baked_sun_dir          # straight from the record
    sun.color  = env.model.sun * clamp01(daylight.rgb)
    moon.dir   = env.baked_moon_dir
    moon.color = env.model.moon * clamp01(daylight.rgb)
    ambient    = env.model.ambient (per-channel daylight-scaled, same gate)
```

If your renderer synthesizes an analytic sun arc, it will diverge from retail.

### 3.4 Fog **[tested/confirmed, one dispute]**

- **In-game world fog is LINEAR only.** In the whole client image, `FOGDENSITY` has exactly one
  call site — the title screen. Everything in-game is a linear start/end pair:
  `f = (far - d) / (far - near)`.
- Values flow from the type-47 getters to the device fog colour/start/end. Defaults if absent:
  colour 0x808080, start 0, end 400.
- With an opaque sky dome behind everything, **distant terrain fades into the sky via this fog**,
  not via the clear colour.
- **[DISPUTED]** Which LightConfig supplies the terrain fog **colour**: our first repro used
  **MODEL**.fog; a later repro re-routed terrain fog to **TERRAIN**.fog (the byte-pinned
  model/terrain role split favours TERRAIN for terrain draws), but **no retail byte-read proves
  either side**. Pragmatic advice: use TERRAIN.fog for terrain and MODEL.fog for actors; expect a
  correction once the retail `D3DRS_FOGCOLOR` call site is traced.

### 3.5 The sky dome **[tested, GPU-QC'd draw model]**

Geometry: a radial dome — `sphereSpokeCount` columns × 8 elevation rings; ring angle
`phi = 0.5 · π · elevation`, colours horizon→zenith.

Draw model (reproduced to match retail behavior):

```
# once per frame, FIRST after clearing colour/depth:
disable depth-write                 # dome never writes Z
view' = view with translation removed   # camera-attached: rotation only
draw dome opaque at fixed skyBoxRadius (scale to fit inside far plane, e.g. far*0.9)
re-enable depth-write; draw the world on top
```

- The radius is a **fixed ~2029.5-unit constant** — it does NOT scale with zone size. Because the
  dome is camera-attached and drawn depth-less, that's fine in-engine; only a free-camera zone
  viewer needs to rescale it (a viz choice, not engine behavior).
- **Axis gotcha:** FFXI zone geometry is **Y-down** (gravity +Y, "up" is −Y). The dome data is
  authored +Y-up, so composite with a `diag(1, −1, −1)` flip or it renders as a bowl under the
  world.
- Celestial/sky billboards draw with a large depth bias (retail ZBIAS 15) **[confirmed]**.
- Sun/moon/star/cloud/sky meshes are **absent from the zone placement table** — they exist as
  orphan meshes driven by effects, or camera-attached environment; a renderer that only walks
  placements will produce a zone with no sky. **[tested/theory]**

---

## 4. Weather layer 3 — particles (rain/snow etc.) **[theory — weakest layer]**

What we have is Xim-lineage, not first-hand:

- Weather precipitation rides the type-5 particle-generator system's **"batched" path**: a
  generator flagged *batched* (header flags byte `+0x7B` bit 5) emits **one batch particle per
  cycle** that internally holds the sub-particle array (sub-count = the normal per-emission count).
- Weather batches get **×2 emission-radius variance**; several ops/flags are ignored for batched
  particles; batch orientation is somewhat view-aligned even without a billboard flag.
- A client oddity (suspected bug): batched vertex data is pre-multiplied by model×view on the CPU,
  with only projection handed to the GPU.
- Per-particle animated channels (position/rotation/scale/RGBA/UV) are `(time 0..1) → value`
  keyframe curves, linearly interpolated, terminated by a `time == 1.0` entry.

Two honest flags:

1. **Which retail draw pass renders these is [OPEN].** We byte-read a particle-class pass with
   alpha test `ALPHAREF 0x7f (127) GREATER`, but whether it serves effect cards, point-sprite
   weather, or another particle subsystem is unproven — two independent reimplementations draw
   effect cards with **no** alpha test at all, so don't blindly adopt the 127 discard for all
   particles (we did; it visibly broke soft gradients, and we reverted).
2. **Zone gating:** not every zone plays the full visual for a given weather id — some zones show
   no effect. The gating mechanism (which resource must exist for the effect to play) is untraced
   **[OPEN]**.

Suggested approach: render precipitation as your own camera-local particle emitter driven by the
weather id, and treat byte-faithful batched emulation as a later refinement.

---

## 5. Water

### 5.1 What water actually is **[tested/theory mix]**

There is **no water system**. Observed composition, from strongest evidence to weakest:

1. **Placed zone geometry.** Zone placement tables (the encrypted ZoneDef / MZB scene graph,
   byte-verified 573/573) place ordinary meshes with water names (e.g. `water4`) using the normal
   `world = Rz·Ry·Rx · (S · v) + T` transform, LOD via `_l/_m/_h` suffixes. These draw through the
   standard zone pipeline (§5.2) — typically in the alpha-blend material class. **[confirmed
   placement mechanics / observed naming]**
2. **Zone-embedded ambient effects.** Type-5 (`0x05`) generator sections inside the zone DAT
   itself place and animate meshes — including **orphan meshes with no placement record** (one
   zone audited: 317 effect sections, 64 orphan meshes including sky spheres, clouds, and
   **ocean**). Fountains decompose into independent stacked layers (jets + bubbles + puddle
   quad). Ocean/sea surfaces on ships (Manaclipper) come through this path too. **[theory,
   with several externally-reproduced in-game edits]**
3. **UV-scrolled textures.** The effect system's constant-rate UV scroll op (Sec3 `0x27`/`0x28`,
   an 8-byte record, f32 rate; `0x27` = U, `0x28` = V) is **byte-confirmed across 3,191 nodes**
   and is the mechanism for flowing-water/waterfall texture motion on effect-driven surfaces.
   **[confirmed]**
4. **A particle-opacity data point:** alpha-blended particle meshes snap fully opaque at
   alpha ≥ 127/255 — a figure verified by a third party specifically against the **Bibiki Bay
   ocean**, which tells us that at least some large water surfaces ride the particle-mesh path.
   **[theory for the mechanism; the 127 constant matches our byte-read pass]**

We have **no evidence of**: screen-space reflection, cubemap water reflection, refraction, or any
water-specific shader. The "shiny/env-map" mechanism (relevant to specular glints on water) is a
known three-way open dispute (additive pass vs cubemap vs flat fake) on our joint target list
**[OPEN]**.

### 5.2 The zone render pipeline water rides on **[confirmed — first-hand retail byte-read]**

This is the strongest material in this document — an exhaustive classification of every
`SetRenderState` call site in the client (924 sites).

**Zone pass state block** (applied once per zone pass, inherited by all fixed-function zone draws):

| State | Value |
|---|---|
| Stage0 COLOROP | MODULATE**2X** → `color = 2 · tex.rgb · vertex.rgb` |
| Stage0 ALPHAOP | MODULATE**4X** → `alpha = 4 · tex.a · vertex.a` |
| SRCBLEND / DESTBLEND | SRCALPHA / INVSRCALPHA — **never changes anywhere in the zone pipeline** (no additive blending in world geometry; additive exists only in particle/UI systems) |
| ALPHAREF / ALPHAFUNC | 0x60 (96/255 ≈ 0.376) / GREATER |
| FOGENABLE / LIGHTING | 1 / 1 |
| Cull | CCW default per pass (not globally-off) |

**Per material sub-group** (this is your water blend pass):

```
blend-on  group:  ALPHABLENDENABLE=1, ZWRITE off, ZBIAS 8
blend-off group:  ZWRITE on, ZBIAS 0
ZFUNC: untouched — device default LESSEQUAL
```

**Per-mesh runtime toggles:** alpha-test enable per mesh (`0` off+WRAP, `1` on+WRAP,
`2` off+CLAMP), a per-geometry winding flip, and a per-group cull-disable.

**The coplanar gotcha (why your water/overlay pass silently no-ops):** the alpha-blend overlays
(water edges, snow/dirt feathering, clouds/mist class) are **coplanar with the opaque base they
sit on**. If your blend pass runs with a strict `LESS` depth test, every equal-depth overlay
fragment is rejected and the pass draws nothing — pixel-identical to "blending is broken". Fix:

```
# pass 1 — opaque + cutout
depth_func LESS;  depth_write ON
draw opaque meshes; draw cutout meshes (alpha-test)

# pass 2 — alpha-blend overlays (water surfaces, feather overlays)
enable blend (SRC_ALPHA, ONE_MINUS_SRC_ALPHA)
depth_func LEQUAL          # the key: coplanar with the base
depth_write OFF
(optionally small depth bias toward viewer — retail uses ZBIAS 8)
draw blend-class meshes with NO alpha test   # blend pass draws test-free [tested repro]
# fragment alpha = tex.a * vertex.a (times the engine's x4 — see PS2 note below)

# pass 3 — restore
depth_func LESS; depth_write ON; disable blend
```

Back-to-front sorting was **not** needed for these coplanar overlays in our reproduction (they
rarely overlap); add it only if an artifact appears. Whether every overlay class (clouds, mist,
aurora) is reliably coplanar is **[OPEN]** — confirmed for ground overlays.

**Which meshes are in the blend class** **[confirmed for the rule; one bit disputed]**: from the
mesh header's `blending` u16 — `useAlpha = (blending & 0x8000) OR mesh-name starts with '_'`;
true blending fires when `useAlpha AND ((blending >> 12) & 0x08)`. A `#` name prefix selecting
opaque+CLAMP is **[theory]**. **[DISPUTED/OPEN]:** whether bit `0x2000` is a cull-disable bit or
part of a modulate-multiplier nibble — a per-batch cull-disable mechanism provably exists at
runtime, but the loader mapping from DAT bits to the runtime toggles is untraced. If your water
looks correct only two-sided, this is why; cull off for water quads is a safe practical choice.

### 5.3 Vertex colour and the PS2 alpha convention **[tested/confirmed]**

Zone vertices carry a BGRA colour: **RGB = baked lighting/AO** (terrain has no dynamic lighting —
authored ≈ 0.5 so the ×2 lands neutral), **A = the blend/clip factor** ("softblend"). Water
transparency gradients (shore feathering) are authored in **vertex alpha**, not texture alpha.

Alpha is authored on the PS2 half scale: **0x80 = fully opaque** on both textures and vertex
colours. The engine's ×4 alpha stage is exactly two ×2 compensations. If your loader normalizes
alpha to 0..1 with 0x80 → 1.0 on both sources, use a plain `tex.a * vertex.a`; if you keep raw
0..255 values, you need the ×2-per-source. **Do not copy the ×4 blindly into a renderer that
already normalizes — it double-counts** (we made this mistake; it saturates mid-alpha content).

Two texture-decode warnings that masquerade as render bugs:
- FFXI textures pack real imagery in zero-alpha regions (because blend comes from vertex alpha).
  Alpha-keyed cleanup/dilation destroys real art.
- There is **no palette-index-0 / colour-key transparency**; transparency is a real alpha channel
  (palette alpha byte or DXT alpha), PS2-half-scaled.

### 5.4 Water-adjacent data that is NOT rendering (don't confuse these)

- **Collision terrain types** **[confirmed]**: collision triangles carry a 4-bit terrain code;
  `8 = ShallowWater`, `9 = DeepWater` (drives footstep/splash selection, not visuals). Useful if
  you need to *detect* water for gameplay/audio.
- **RAB path graphs (block type 74)** **[confirmed format / theory purpose]**: tube-radius
  polyline graphs, often hosted under water-named ids (`umi`/sea, `kawa`/river, `mizu`/water,
  `taki`/waterfall). **Byte-refuted as water surfaces** — they are NPC steering/current path
  graphs, and per a third party also drive **shoreline positional audio** (surf volume from
  distance to nearest path point).
- **Environment link on collision objects** **[theory]**: standing on a collision object with an
  environment-link id switches which type-47 environment set applies (e.g. under-a-roof areas) —
  relevant to water only in that grottos/waterways often carry their own environment.

### 5.5 Ambient-effect mechanics you need if you render effect-driven water **[theory unless noted]**

- `0x05` sections in zone DATs are **unencrypted** (unlike the scene graph and mesh containers) —
  in-place experimentation is practical (externally reproduced on a live client).
- An effect's local position: the 3×f32 immediately after the first referenced resource in the
  section. **−Y is up** (a positive Y buries the effect under the floor).
- Spawn-at-load: generators with an autoRun flag register at zone load with no server involvement
  (this is how ocean/fountains/torches come up). Exact flag predicate (equality vs mask on the
  `+0x79` byte) is **[OPEN]** — 884 corpus nodes diverge between the two readings.
- Pop-in/draw distance = the generator-cull op's first f32 (externally verified in-game).
- Emission cadence: the header period field stores **frames-per-emission MINUS ONE**
  **[confirmed, three agreeing sources]**.
- Transplanting an effect to another zone works iff the **full dependency closure** travels with
  it (textures, sprite-sheets, particle meshes, keyframe curves, meshes).
- Playback lifecycle (from our latest work, pending final KB ingest): an effect's visual end ≠ its
  schedule length — emission windows end, then each emitted segment lives out its own lifetime;
  render streamed segments with per-segment ages, and re-arm loops when the last segment visibly
  drains **[confirmed in our player]**.

---

## 6. Suggested implementation plan

Phased so each stage is independently verifiable. Grades tell you where to expect rework.

### Phase 1 — Weather state (solid ground)

Implement the weather id state machine: server-driven if you have a server (0x057 semantics in
§2.2, zone-in seed §2.3), deterministic table otherwise (§2.4 pseudocode). Regression-test with
zone 230.

### Phase 2 — Environment records (highest visual payoff per effort)

Parse type-47 records (§3.1 — the 184-byte layout is byte-pinned; mind the colour decode and the
far/near order). Then:

```
each frame:
    minute  = vanadiel_minute_of_day(clock)
    envs    = records[current_subarea][weather_tag(active_weather)]
              or fallback records[...]["suny" / default]
    env     = active_environment(envs, minute)         # §3.2 blend, sentinel discrete
    if weather_changed_recently:
        env = crossfade(prev_env, env, elapsed / 3.33s)    # [theory] duration
    clear(env.clearColor)
    draw_sky_dome(env)                                  # §3.5: first, no depth write,
                                                        # camera-attached, fixed radius
    set_fog(linear, env.terrain.fogNear, env.terrain.fogFar, env.<terrain|model>.fog)
                                                        # colour source DISPUTED §3.4
    set_lights(build_directional_lights(env, daylight)) # §3.3: baked dirs, scaled colours
    set_far_plane(env.drawDistance)
    draw_world()                                        # §6 phase 3 ordering
```

For the daylight factor, until the real curve is found **[OPEN]**, a defensible approximation is
to derive brightness from the blended hour records themselves (they already carry the day/night
colour variance) and keep the factor at 1.0 — i.e., don't invent a second curve.

### Phase 3 — Zone geometry passes (water surfaces live here)

Implement the §5.2 pass structure exactly: opaque+cutout with depth LESS/write-on, then the blend
class with **LEQUAL + depth-write off (+ small bias)**, blend always SRCALPHA/INVSRCALPHA, no
alpha test on the blend pass, fragment `= (2·tex.rgb·vtx.rgb, tex.a·vtx.a on normalized scales)`.
This single ordering decision is the difference between "water/overlays render" and "the blend
pass silently does nothing".

### Phase 4 — Effect-driven water & weather particles (expect iteration; weakest evidence)

- Start with placed water geometry only; many zones' water is visible without effects.
- Add zone ambient effects next (ocean/waterfall orphan meshes + UV scroll §5.1.3). The streamed
  segment model (§5.5 last bullet) matters if you animate them faithfully.
- Add precipitation last, as your own emitter keyed on weather id (§4) — byte-faithful batched
  emulation is not currently possible from public knowledge.

---

## 7. What we do NOT know — flagged honestly, with proposed closure experiments

| # | Unknown | Impact | How to close |
|---|---|---|---|
| 1 | LSB `zone_weather` data ≡ retail schedule data? | Offline prediction may mismatch retail per-zone | Diff predicted vs observed weather on retail across zones/days; or byte-compare the client's own 0x1B79/0x1B7D resource against the LSB blob (highest value, cheap) |
| 2 | Fog colour source: MODEL vs TERRAIN LightConfig | Wrong fog tint on terrain at some hours | Trace the retail `D3DRS_FOGCOLOR` call site downstream of the zone pass and read which struct field feeds it |
| 3 | Daylight-factor writer (the actual day curve) | Global brightness curve approximated | Memory-watch the three u16 channels across an in-game day; correlate with clock |
| 4 | Weather → environment resource resolver semantics (the id+offset adds) | Which env/sky resource a weather id loads | Decompile the resolver chain; corpus-diff loaded resources across forced weathers |
| 5 | Weather particle batched path (everything in §4) | Precipitation fidelity | Trace the retail batch draw path; RenderDoc/PIX-style capture on a rainy zone would settle the pass states |
| 6 | Which particle subsystem owns the ALPHAREF 0x7f pass | Whether any particle class alpha-tests at 127 | Trace the card-draw call chain to the pass prelude |
| 7 | Zone gating of weather visuals ("some zones don't play the effect") | Per-zone correctness | Compare a zone that renders rain vs one that doesn't; diff their weather-keyed resources |
| 8 | The `'weat'` in-DAT carrier block layout | Unknown per-zone weather data | Dump and characterize the block (we know where it binds) |
| 9 | File ids 0x1B78 / 0x1B7C contents | Possibly nothing weather-related | Resolve via ROM indexing, dump, characterize |
| 10 | DAT `blending` bits → runtime toggles (the `0x2000` cull dispute) | Two-sided rendering of water/foliage | Trace the mesh-loader path from header bits to the runtime shorts (already on our joint target list) |
| 11 | Shiny/env-map mechanism (specular glints) | Water sparkle fidelity | Already agreed as a joint target: additive pass vs cubemap vs flat fake |
| 12 | Are clouds/mist/aurora overlays coplanar like ground overlays? | Depth artifacts on those classes | Inspect their meshes' depths vs base geometry in a few zones |
| 13 | The 0x14–0x27 variant weather ids | Unknown | Force them server-side and observe |
| 14 | Whether baked sun/moon *directions* are hour-blended or held per record | Subtle light-direction popping | Compare directions across bracketing records in zones where they differ |

---

## 8. Source map (for auditing this document)

Claims trace to these KB entries (available on request), listed with their confidence:

| KB entry | Grade | Contributed |
|---|---|---|
| `packet-in-057-weather` | confirmed | §2.1–2.2 wire layout, enum, scaling, capture gotcha |
| `ffxitool-weather-packet-apply-and-state-globals` | tested | §2.3, §3.4 fog value flow, weather globals |
| `ffxitool-weather-generation-deterministic` | theory | §2.4 deterministic model, carry-forward, fixtures |
| `ffximain-client-core-architecture` | tested | §2.5 client schedule resource (0x1B79/0x1B7D) |
| `ffxi-weather-appearance-dat-regions` | unverified | §2.5 stub ids 0x1B78/0x1B7C |
| `ffxitool-dat-type47-zone-lighting` | tested | §3.1 record layout (byte-confirmed 4021/4021) |
| `ffxitool-tod-daylight-render-pipeline` | tested (needs-review on fog-colour sub-claim) | §3.2–3.5 |
| `ffximain-render-state-pipeline` | confirmed | §5.2 all retail render states |
| `ffxi-vertex-alpha-softblend` | tested | §5.3 vertex colour roles |
| `ffxitool-alpha-test-opaque-blend-driver` | confirmed (needs-review banner) | §5.2 material classes, PS2 alpha |
| `ffxitool-zone-blend-overlay-coplanar-leqal` | confirmed | §5.2 coplanar/LEQUAL gotcha |
| `ffxi-zone-dat-ambient-effects` | theory | §5.1.2, §5.5 ambient effect mechanics |
| `ffxitool-dat-effect-particle-keyframe-batch` | theory | §4 batched weather particles, curves |
| `ffxitool-dat-effect-particle-generator` | theory/tested rows | §5.1.3 UV scroll (confirmed), generator ops |
| `ffxitool-dat-zone-type28-geometry` | confirmed | §5.1.1 placements, `water4`, sky absent from placements |
| `ffxi-zone-collision-format` | tested | §5.4 terrain types, environment link |
| `ffxi-zone-paths-routes` | tested | §5.4 RAB refutation, shoreline audio |
| `ffximain-zone-resource` | tested | §2.5 `'weat'` zone-in binding |

Compiled 2026-08-01 from the FFXI-RE knowledgebase. Corrections welcome — especially on anything
tagged [theory] or [DISPUTED]; several of the open items above are already on our shared
experiment list.
