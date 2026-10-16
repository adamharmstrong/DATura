# Heavens Tower portal resource trace

Read-only investigation of installed PC DATs, checked against the local PS2
decompilation at C:/Users/nanob/source/repos/FFXI-PS2. No renderer changes or
in-game visual/audio validation were performed in this investigation.

## Persistent circle

Windurst Walls: ROM/0/79.DAT, directory t_wi/effe/hoto. Generator placements
cluster around native (-0.103,-16.803,140.865), matching the Tower entrance.

| Generator | Chunk offset (decimal) | Linked model |
| --- | ---: | --- |
| rig1 | 9356016 | eis2 |
| rig2 | 9357152 | hi18 |
| rig3 | 9357856 | ei30 |

Additional particles/lights at the entrance include pt01/eisy, pt02/komo,
hg01/ligh and hg03/ligh. The ring generators reference alpha curve rngs.

Tower ground-floor exit: ROM/2/62.DAT, an interior replacement resource rather
than the base ROM/1/38.DAT. The three ring layers sit near native
(0.075,-0.059,-33.813), matching the exit region.

| Generator | Chunk offset (decimal) | Linked model |
| --- | ---: | --- |
| rig3 | 700384 | ei30 |
| rig2 | 700768 | hi18 |
| rig1 | 701152 | eis2 |

Tower particle layers Lpt1 and Lpt2 reference hit1 at the same position.
The ring generators reference curve kho1. Similar generators exist in
ROM/2/63.DAT around the upper-floor portal, near (7.1,-45,-29.7).

These are layered textured effect models, not a single zone terrain mesh.
Their shared type-0x1f geometry is in ROM/0/0.DAT: eis2 at 20560, ei30 at
28432 and hi18 at 51232. Material references include eisyou2/eisyou23,
eisyou3/ei30 and hit24/hit241. Texture selection must use the payload material
identity and directory/type, not merely the four-byte chunk name: the common
DAT has duplicate names across textures, meshes and schedulers.

The ring spawn instructions author X rotation approximately pi/2, laying
the textured layers horizontally. The generators encode scale, tint, lifetime
60, rotation-rate slots, scale-rate slots and alpha-curve references. Idle
commands 0x05, 0x08 and 0x1b respectively update rotation, scale and alpha;
the PS2 YmGenerater implementation confirms these operations. Layers rotate
at different rates, grow, and fade according to the authored curves. Generator
control stream 0x0a carries 25 at Walls and 50 at the Tower; exact scheduling
interpretation should be validated before translating those values to seconds.
Idle 0x2e contains paired timing values 20/25 and 40/50 respectively.

### Ring-axis correction (2026-10-11)

DATura applies the `rig1`/`rig2`/`rig3` animated spin in mesh-local space,
before their fixed authored rotation lays the rings on the floor. Adding the
spin directly to the post-tilt Euler angles made the transporter tumble.
This correction is limited to Tower portal idle rings; Home Point crystal
and activation-effect transforms retain their existing behavior. Regression
tests use all three installed portal animation DATs and check that both ring
plane basis vectors stay horizontal at multiple animation ages, alongside
the existing rendering, animation, occlusion, and sound tests.

## Activation and sound

The zone-script reference starts event 86 at Walls and event 41 at Tower.
Their installed event DATs are ROM/21/48.DAT and ROM/21/51.DAT. The system
actor is 0x7ffffff0; entry offsets are 262 and 492 respectively. Both begin
with blocking setup/fade and opcode 0x62 starting effect ID 1, scheduler main,
attached to the player. Constant references are 0x8046 (Walls) and 0x8067
(Tower), and both resolve to value 1. Do not confuse the subsequent opcode
0x45 fade resource with the portal activation effect.

PS2 main/actor/xievent.cpp around line 4616 maps this effect family to file ID
0x1394 + effect ID. File ID 5013 resolves through installed FTABLE/VTABLE to
ROM/16/75.DAT. Its main scheduler is at chunk offset 3008. It starts multiple
generators and schedules opcode 0x0a at frame 25 with sound-resource tag 6001.
The type-0x3d SeSep chunk 6001 at offset 25008 has ID 16001 (0x3e81) at
payload offset 8. The installed sound file is:

sound/win/se/se016/se016001.spw

PS2 main/miyagawa/effect/ymschdecript.cpp resolves scheduler sound resources
as type 0x3d; YmSepRes::Play is the sound playback path. This confirms the
activation sound reference, but the clip was not listened to in this pass.

Distinct location-associated sound generators also exist: Walls warp links
SeSep 2063 (ID 2063), and Tower se04 links SeSep 2184 (ID 2184). These are
separate from activation sound 16001; their loop/attenuation behavior has not
yet been fully decoded.

## Original DATura rendering gap (before the portal implementation)

The current common-resource import in model_ff11_weather.h is restricted to
weather-directory sprites and type-0x21 meshes. The portal rings are outside
that weather directory and depend on common type-0x1f resources. Thus this
import does not satisfy their dependencies. Locating the generator records
alone is insufficient: rendering needs the shared geometry/materials,
generator placement, repeat/lifetime evaluation, transforms and alpha curves.
The Tower side additionally requires its replacement room resource to remain
available to the effect renderer. This is a concrete missing dependency path,
not proof that it is the only rendering issue.

For activation, the current direct local zoning adapter skips event 86/41,
so it also needs an explicit equivalent of the effect-1 main sequence and its
sound timing if cutscene events are not executed.

## Implemented adapter and verification — 2026-10-11

The implementation now loads the three common ring meshes with textures selected
by their embedded material identity, and evaluates the authored rig1/rig2/rig3
placements, rotation, scale, alpha curves, lifetime, and fade slots. It reads
Walls placements from ROM/0/79.DAT and Tower ground/upper placements from
ROM/2/62.DAT and ROM/2/63.DAT. Secondary parsing preserves terrain diagnostics.
The circles depth-test against completed terrain/actor depth without writing
depth, and restore render state. Their ordinary floating-dot rows are suppressed
when their ring asset is available.

All three event regions now open the shared query UI. Movement is frozen during
the query and activation. No/escape cancels; a latch prevents reopening until
the player leaves the circle. Unloading the zone or leaving game mode clears
pending interaction state.

The upper-floor query reads message 80 from ROM/25/51.DAT:

    Do you want to warp to the first floor?
      Yes.
      Not now.

The Walls and ground-floor events inspected here do not contain a confirmation
query. Their new prompts, "Enter Heaven's Tower?" and "Return to Windurst Walls?",
are explicitly DATura enhancements requested by the user, not claimed original
menu text.

The upper-floor return adapter hret uses the script-reference event region
(6,-46,-30) through (8,-44,-28). Installed Tower event 83 branches to event 84
or 85. Event 84 calls system-actor local block index 21, whose opcode 0x47
supplies X/Z/Y values (-20902,-8665,500) in thousandths and heading 3792 in
4096ths of a turn. The adapter chooses that first safe return position:
native (-20.902,0.5,-8.665). It does not reproduce the random alternate branch
to event 85. The shared local zoning path reloads the Tower for this same-zone
return.

Confirmation starts the installed effect-1 main sequence on the player.
Sound 16001 is dispatched once at scheduler frame 25 using independent XAudio2
playback; sound settings and foreground policy are respected. Location-associated
ambient sound generators remain unimplemented. The adapter waits 1.5 seconds
before entering DATura's ordinary fade/load path. This delay, 60-Hz effect
evaluation, and translation of generator repeat intervals are local timing
approximations, not a complete emulation of retail event/scheduler behavior.

HomePointTests verifies all three ring assets have three mesh layers, produce
visible changing pixels, are hidden by foreground depth, render the activation
effect, and load the activation sound. It also checks sound decoding, original
query text, the installed upper-floor arrival operands, cancel state, and
one-shot frame-25 sound dispatch. Existing Home Point rendering/audio tests
remain passing. ZoneTransitionTests covers all three event-region crossings,
inclusive floor contact, wrong-floor/outside-width rejection, both coordinate
modes, and non-bouncing arrivals.

Release x64 DATura builds successfully. Offscreen rendering tests pass; a manual
in-game walkthrough and listening comparison against retail remain outstanding.
