# FFXI KB <-> cexi-docs crosscheck - 2026-08-01

Follow-up to the 2026-07-22 crosscheck. We reviewed **cexi-docs @ f330441** (published
2026-07-28) end-to-end against our KB and ran per-claim comparisons in the areas where we
hold first-hand byte/decompile evidence. This doc is the two-way result: corrections we can
back with bytes, places where your docs beat our KB (we're adopting them), apparent
conflicts that dissolve on inspection, small doc bugs, and a shortlist of cheap decisive
tests - several of which line up with the joint targets we already agreed.

**Evidence-class legend used below:**
- `byte` - first-hand hex/corpus verification on retail files by us
- `decomp` - first-hand FFXiMain.dll decompile by us
- `docs` - what cexi-docs states (with its own cited trail)
- `xim` - claims whose ancestry is the xim parser lineage (shared upstream with parts of
  our KB, so agreement there is corroboration of the same source, not independent proof)

**Circularity note, so neither side double-counts:** where cexi-docs cites "the client
decompile" for findings from our July joint session (zone `_`-prefix alpha test, 0x2000
cull bit, the ~69/255 entity cutout, event file-id bases), we treat agreement as shared
work, not independent confirmation - and we'd suggest the docs mark those the same way.

---

## 1. Corrections we can back with bytes/decompile

### 1.1 FTABLE/VTABLE combine model (the big one)
Your docs and `external_source/dump_event.py` still model the Xim **OR-merge**
(`FileTableManager.combine`) with a version==0 fallback. We byte-disproved OR-merge on
2026-06-24: the client is **volume-direct + update-shadows-base** - the ROM volume named
by VTABLE is read directly, updates shadow the base, and the ~33 file-ids present in more
than one volume decide the question (OR-merge produces wrong paths for them). Your
*practical* rule ("write both tables when patching") stays valid either way - it's the
stated mechanism that's off. Happy to send the id list.

### 1.2 Event-VM instruction sizes (5 deltas)
Your `events/opcodes.md` size tables agree with our catalog for every opcode we compared
except:

| op | cexi-docs | ours (atom0s + corpus) | note |
|---|---|---|---|
| `0x71` sub `0x20` | 16 | 10 | size-set {2,4,6,8,10}, not {...,16} |
| `0x72` | {4,6} | {4,6,10} | 10-byte case missing |
| `0xAB` | {2,4} | {2,4,6} | 6-byte case missing |
| `0x5B`/`0x66` | fixed 15 | {15,17} | 17-byte case missing |
| `0x2A` | prose says 7 | 6 | your own code says 6 - prose typo |

Our disassembler runs a zero-overrun harness over the full retail event corpus, so these
adjudicate cheaply; we'll run both tables and share the diff. Two semantic deltas also
flagged, not yet adjudicated: `0x81` ("blinking" vs your "warp data"), and your `0xD8`
dual-handler claim (sound-effect flag + `ExtData[1]->EventDir`) which no other source has.

### 1.3 Auto-translate wire form
`dats/ROM_168_25.md` claims the client emits a 4-byte `02 02 <cat> <idx>`. The on-wire
tag is **6 bytes, 0xFD-framed** (`byte`-verified three independent ways on our side).
Likely reconciliation: the 4-byte form is the DAT-internal record prefix, conflated with
the chat wire form.

### 1.4 `comm` block count (ROM/118/114)
"1792 (0x700) records" is the FFXiMain consumer's **loop bound**; on disk the block holds
**2816 x 0x30** records (`byte`, sequential index 0..2815). Suggest rewording to "client
iterates the first 0x700".

### 1.5 Zone model file-id expansion branch
Your xim-cited branch is `zoneId >= 0x100 -> 0x147B3 + (zoneId - 0x100)`. Our decompile of
the resolver (`FUN_10178d00`) gives threshold **>= 600** with **+0x144F7**. The two
disagree on both threshold and base for zones 256-599+; your own corpus only exercises
ids < 256, so the branch is untested on your side. Worth swapping in the decompiled values
(or re-deriving from your build).

### 1.6 POL1 OEP nuance
`reference/ffximain.md`'s OEP (RVA `0xBB17B0`) is the **packer stub** entry. The real
post-unpack OEP (what you want after dumping `.text`) is **`0x3162AF`** (`decomp`).
Everything else in that doc matched our confirmed POL1 analysis independently - layout,
packed length, `.text` VirtualSize `0x32716E`, and the 12-bit offset / 4-bit length,
MSB-first LZSS. Nice convergence.

### 1.7 EventMessage stored offsets
`events/dialogue.md` says stored = absolute - 4. Our full-sweep byte check (289/289 files)
reads the demasked offsets as **file-absolute** (count = (offset[0] - 4) / 4). Same
geometry either way, but exactly one description matches the literal bytes - one hex read
settles it, and ours says file-absolute.

### 1.8 Event DAT sentinels + zone-block id
- `events/format.md` prose lists eventId `0xFFFF` *and* `0xFFFE` as sentinels; our corpus
  (and your own `dump_event.py`) support **0xFFFE = wildcard** only. 0xFFFF-as-sentinel
  looks unbacked.
- `dump_event.py` uses `0x7FFFFFFF` as the zone/master block actor id; real files carry
  **`0x7FFFFFF0`** (`byte`). (Operand-space GetActorIndex magic ids are a separate
  namespace and your table for those matches ours.)

### 1.9 Type-6 `mode` is not an interp selector
`events/camera_scene_ids.md` / `scene_dat_writer.md` label the route header byte a
"smoothing/interp mode". Our client-side RE of the camera player (VA `0x1003D150`) shows
interpolation is chosen by **key count** (2 keys = LINEAR, >2 = CUBIC); `mode` never feeds
it - and your docs are internally split anyway (retail multi-point = mode 4 in one place,
mode 1 in another). Your "mode 0 on multi-point crashes" observation is interesting though
- if it reproduces, it's the first behavioral semantic anyone has pinned on that byte.

---

## 2. Where cexi-docs beats our KB (we're adopting / re-flagging)

### 2.1 Camera FOV: K = 192 [docs, A/B-verified]
`FOV = 2*atan2(192, focal)` (focal 350 -> 57.5 deg vfov) resolves a field we only had an
uncalibrated K~145 guess for. We're adopting K=192 pending one render-match against a
retail screenshot. (Note `events/cutscenes.md` still carries the retracted "decidegrees"
reading - see 4.2.)

### 2.2 Audio .bgw+0x2F / .spw+0x2B: you're probably right
Our entry (derived from RE of AltanaView's parseHeader, not FFXiMain) reads the byte as
bitsPerSample in {8,16} with fixed 9-byte/16-sample ADPCM frames. Your reading -
**block_size**, with variable frame geometry derived from the body - is backed by
byte-exact vgmstream comparison and by files a fixed-9 decoder cannot decode
(music025.bgw: value 64, 33-byte frames; se032037.spw: header lies, real frame 3 bytes).
Value 64 is impossible for a {8,16} field, so we've re-flagged our entry; our suspicion is
AltanaView itself misreads that byte. Also adopting: `index >= 5 -> silent block, history
untouched` (vs our "filter 0"), and the 5-bit shift wrap. One request: our label for
.bgw+0x28 ("start offset") vs your `unknown1` + hardcoded 0x30 is still open - if you ever
see a file where data doesn't start at 0x30, that decides it.

### 2.3 Effect emission field +0x76 = spawn interval [docs, in-game verified]
Your in-game A/B (write 240 -> ~1.3 s gaps; write 2 -> continuous gush) beats our
AltanaView-derived "frame count x 1/60 = duration" reading for the same u16. Re-flagged on
our side.

### 2.4 Per-subcode size tables
Your `_FIXED_SIZES`/`_SUB_TABLES` resolve the 34 multi-size opcodes our disassembler left
unsized. Queued for validation over our corpus (see 6.2), then adoption.

### 2.5 Skeletal modelid -> file-id bands (partial)
Your VA `0x100C513D` table agrees with our tested bands except two numbers: band-3 upper
bound (**3499** vs our **3193/0xC79**) and band-4 base (**98239/0x17FBF** vs our
speculative **98546/0x180F2**). Since our band 4 was flagged speculative with a ~90%
corpus miss, your pair is the best candidate fix we've seen - but band 3's boundary is
tested on our side, so we'd love the decompile snippet around `0x100C513D` to reconcile
both at once.

### 2.6 Smaller adoptions queued
The 13,655-op work-selector proof (2-byte `0x8000|refIndex`, 17% with index > 127); the
full `0x45` 17-byte operand layout; sceneSize-is-unpadded + 0xFF tail padding; the
spell -> animation resolution chain (incl. `fileIndex = 0xAF0 + animIndex` and the 0x49
SpellList cipher); mount model resolution `0x019131 + mountId` and the 64-menu/255-model
split; footstep DatId `0<terrain><movementChar><shakeFactor+1>` with footwear selecting
the timbre (assigns a purpose to an "unconfirmed" byte in our info-section entry);
FTABLE id-space bound 109,701 from the table file sizes. Good material - thank you.

---

## 3. Apparent conflicts that dissolve (don't chase these)

1. **Dialog offset convention.** Your "position = value + 4" / `startOffset/4 - 1` and our
   "file-absolute + 4-byte record header" / `(offset[0]-4)/4` are the same arithmetic with
   different bookkeeping - we verified both decode zone 230 entry 0 identically. (Your
   model doesn't represent the `0x0700` record-header kind tag; its low word is undecoded
   on both sides.)
2. **Audio sample rate.** Your signed-int32 sum equals our `u32 sum & 0x7FFFFFFF` for all
   sane rates.
3. **0x36 sub-area records.** Your entry base = our record base - 8; your "three zero u32
   pad" = our reserved head fields. Layouts reconcile exactly under that shift.
4. **ADPCM filter table.** Your x4 / >>8 table is bit-identical to the standard PSX >>6
   form. One caution from our comparison: the predictor term needs **arithmetic >> 8**;
   pol-utils' C# `/256` truncates toward zero and drifts after the first negative-history
   sample.
5. **Container size mask.** Your 20-bit read vs our byte-validated 19-bit is the known
   xim-lineage discrepancy we already track; no new evidence on either side.

---

## 4. Small doc bugs / internal inconsistencies in cexi-docs

1. README intro links "xim" to `github.com/atom0s/XiEvents` - different project.
2. `events/cutscenes.md` still says the FOV field is decidegrees; `camera_scene_ids.md`
   correctly retracts that ("older docs saying decidegrees are wrong").
3. `events/opcodes.md` prose gives `0x2A` len 7; your code (and our corpus) say 6.
4. "xiclient" is described as a fan reimplementation in the README trust tiers but as
   "the xiclient decomp" in `events/event_mode_bits.md` - which is it? It changes the
   weight anyone should give xiclient-derived claims (e.g. the camera-task self-delete,
   LOD struct guesses).
5. The 0x05 attach-group anchor is inconsistent across `dats/fx.md` (data-start +0x10),
   `fx/effects.md` (+0x00), and `fx/effect_system.md` (which admits the split). The
   emission group (+0x74..) is demonstrably section-start relative; worth normalizing all
   tables to one frame.
6. `zone/collision.md` calls the +0x80 tail an "identity normal matrix";
   `zone/format.md` calls it "mesh-local cull bounds". (Our corpus says: 3 x vec3,
   purpose unresolved - so both are candidates, but the docs shouldn't disagree with
   themselves.)
7. `dump_event.py`: zone-block constant `0x7FFFFFFF` vs on-disk `0x7FFFFFF0` (see 1.8).

---

## 5. Environment caveat worth labeling

The traced install is a CatsEyeXI client with Ashita/XIPivot overlays. Load-order and
"never loaded" claims (e.g. ROM/0/1.DAT zero reads at boot) are environment-specific and
may not hold on clean retail; a one-line label on those sections would keep them honest.
Same for zones.md rows that reflect CatsEyeXI-remapped slots (e.g. 133) and custom
ROM/36x-37x ranges - clearly marked as client-variant would help downstream readers.

---

## 6. Proposed cheap decisive tests (joint queue)

1. **0x2A body+0x1A** - our tested inline `u16 nSingle, u16 nDual` vs your
   `u32 vertexCountsOffset/2` (+ count=2 @ +0x1E). Both sides claim retail validation, so
   one of us is misreading a numeric coincidence. One DAT, one hex dump, settled.
2. **Event size tables** - we run your `_FIXED_SIZES`/`_SUB_TABLES` and the atom0s table
   through our zero-overrun corpus harness; the 0x71/0x72/0xAB/0x5B/0x66 deltas fall out
   mechanically.
3. **.bgw/.spw +0x2F** - histogram the byte across the corpus against derived frame
   geometry; confirms block_size and quantifies the 32 liar files.
4. **K=192** - render-match one known retail cutscene frame.
5. **sec2 0x16 color order** - red-channel edit in-game (your green test can't
   discriminate BGRA vs RGBA; R/B is the disputed pair).
6. **0x07 sec2 delay attribution** - one routine with asymmetric delays decides
   before-own-command (xim/UE5) vs after-own-command (our confirmed reading).
7. **EventMessage offset base** - single hex read (see 1.7).
8. **Modelid band 3/4 seam** - resolve modelids 3194-3499 and 3500+ against both formula
   sets; also directly tests your 3499/98239 vs our 3193/98546.

Items 1, 4, 5, 6 slot straight into the joint-target list from July (MMB blending trace,
daylight writer, shiny three-way, mip/samplers still open on our side).

---

## 7. Convergences worth celebrating

Independent agreement (not shared-lineage): the POL1/LZSS analysis end-to-end; the event
DAT container layout incl. your 11-zone writer round-trip vs our 613-block sweep; the 0x07
sec2 framing and op-0x04-plays-camera; the 0x2B animation header/84-byte bone record at
identical offsets; per-zone file-id algebra (your catalog bases are algebraic identities
of our byte-verified deltas). And one gift back: your "non-obvious `0xAB` size field" in
the animation DAT decodes with our container meta formula - `0x0002A5AB` = type `0x2B` +
19-bit size, 21,680 bytes. It was never a mystery byte; it's the section header doing its
job.

---

*Shared-safe: this document contains no personal data, no account/machine identifiers,
and no verbatim excerpts from either side's non-public tooling - offsets, opcodes, VAs,
and formulas only. Questions, disagreements, and hex dumps welcome as always.*
