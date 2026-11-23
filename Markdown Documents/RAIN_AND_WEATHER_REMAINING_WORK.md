# Rain and weather rendering: remaining work

## Purpose

This document records the work still needed after the October 2026 rain-rendering repair and PS2-source investigation. It is intentionally broader than the original orange-line defect because correct rain depends on four connected systems:

1. selecting the weather that is allowed in the current zone;
2. selecting and activating the correct authored resources for that weather;
3. rendering the environment, precipitation, and transition correctly;
4. playing the matching ambient weather audio.

The current implementation is usable and its focused regression suite passes. The remaining tasks are fidelity, coverage, scheduling, audio, and production-hardening work. Items below distinguish confirmed missing behavior from questions that still require evidence.

## Current verified baseline

The following behavior is already implemented and should be preserved:

- Weather resources are read from each zone model's `/weat` branches.
- Active particle generators require the serialized weather/active flags used by the inspected PS2 activation path.
- A generator is associated with the selected weather by its authored weather root rather than only by a short resource name.
- An authored texture in the active weather directory takes precedence over a texture inherited from a shared shape resource. This fixes the case where a shared orange texture was applied to a rain or dust card.
- Pre-instantiated precipitation meshes are rendered as authored batches.
- Sprite weather uses authored card geometry, texture atlases, spawn position, position spread, scale, velocity, velocity variance, acceleration, lifetime opacity, color tracks, distance fading, and atlas animation where those fields were decoded.
- Weather particles depth-test against the completed terrain and actor scene.
- The old and new weather particle sets cross-fade during a weather change.
- Renderer state is captured and restored around the weather pass.
- La Theine Plateau, Pashhow Marshlands, and Rolanberry Fields have distinct rain regression fixtures. Their active rain-generator counts are currently 8, 2, and 4.
- The weather regression executable passes for Konschtat dust, Beaucedine snow, Garlaige blizzard, and the three rain fixtures.
- The full Debug x64 DATura application builds successfully.
- The generated database covers 300 zone IDs and 1,326 authored weather branches. Its SQLite integrity check passes.
- Weather follows the zone's natural LandSandBoat schedule on Vana'diel time (`DATura/zone_weather_schedule.{h,cpp}`, runtime data `DATura/zone_weather_schedule.bin`). Cycle Zone Weather is a manual override that steps through every authored branch and then returns to the schedule. The camera-debug overlay shows `[natural: <type>]` or `[manual override]`.
- Authored weather loops play through XAudio2 with a 3-second cross-fade (`DATura/zone_weather_audio.{h,cpp}`).
- Every rain and squall branch has a recorded classification (`zone_rain_branch_audit` and `tools/zone_rain_branch_audit.csv`).
- Weather emitters whose stored count byte is zero are no longer skipped.
- `tests/WeatherScheduleTests.vcxproj` covers schedule, resolver, sound selection, and the audio controller.

Relevant implementation and data files:

- `DATura/zone_weather_particles.cpp`
- `DATura/zone_environment_state.cpp`
- `DATura/main.cpp`
- `tests/weather_rendering_tests.cpp`
- `tests/WEATHER_RENDERING.md`
- `tools/generate_zone_weather_database.py`
- `tools/zone_weather_database.sqlite`
- `tools/ZONE_WEATHER_DATABASE.md`
- `tools/zone_rain_branch_audit.csv`
- `DATura/zone_weather_schedule.cpp`
- `DATura/zone_weather_audio.cpp`
- `tests/weather_schedule_tests.cpp`

## Priority 0: validate the repaired image against the retail client

The automated tests prove that authored resources render, animate, respect depth, and respond to activation flags. They do not prove pixel-level or perceptual parity with the original client.

### Capture comparable scenes

Record retail and DATura footage with matching:

- zone;
- weather type and intensity;
- Vana'diel time;
- camera position, height, yaw, pitch, and field of view;
- output resolution and aspect ratio;
- character position;
- transition phase, where possible.

At minimum, capture:

- La Theine Plateau rain;
- Pashhow Marshlands rain;
- Rolanberry Fields rain;
- a squall or other stronger-rain branch;
- rain while facing bright sky;
- rain while facing dark terrain;
- rain with an actor directly in front of the camera;
- rain while walking, rotating the camera, and standing still;
- the beginning and end of a clear-to-rain transition.

### Compare the properties that tests do not currently measure

Review the captures for:

- card size and aspect ratio;
- apparent drop length;
- number and density of visible cards;
- direction and speed;
- whether the rain volume follows the camera or remains in world space;
- camera-facing orientation;
- texture-atlas frame order and rate;
- brightness and tint at several times of day;
- alpha and blend behavior over sky, terrain, water, characters, and particle effects;
- near-camera clipping or sudden appearance;
- far-distance fade;
- sorting artifacts between overlapping translucent cards;
- transition duration and fade curve;
- whether camera motion causes swimming, popping, or a repeating grid.

### Acceptance criteria

- The orange-line failure cannot be reproduced in any rain-enabled fixture.
- Rain uses the correct local rain texture when a shared card shape is used.
- Card density, velocity, scale, orientation, and alpha are close enough to retail that side-by-side footage does not expose a systematic difference.
- Rain does not draw over actors or solid terrain that should occlude it.
- A camera rotation does not reveal a fixed flat wall of precipitation.
- Any accepted deviation is recorded with a capture and a short explanation.

## Priority 0: confirm the PS2 generator-selection rules completely

The database currently defines a `rain_activatable_generator` from the serialized flags corresponding to the inspected PS2 `ActivateRain` path. This is a strong match, but it is not yet a complete reimplementation of the PS2 object lifecycle.

### Trace the remaining activation state

Use the decompiled PS2 source to document, field by field:

- the object type selected by `XiWeather::ActivateRain`;
- the meaning of every tested flag and status value;
- the order in which matching generators are visited;
- whether activation stops after a quota or activates every match;
- how the requested rain count or intensity reaches `ActivateRain`;
- whether a generator can already be active for another reason;
- how `DeactivateRain` chooses the subset to stop;
- whether generators are reset or resume their prior age/phase;
- whether transitions overlap active sets or replace them immediately.

Primary source locations already identified:

- `src/main/zone/xiweather.cpp`
- `src/main/zone/xiarea.cpp`
- `src/main/zone/xizone.cpp`
- `src/main/actor/xiatelnet.cpp`
- `src/main/zone/ymresfileprocess.cpp`

### Findings recorded on 2026-10-16

- `YmCombineWeather::YmCombineWeather` resolves the requested `/weat/<id>` directory and falls back to `suny` when it is missing. It then activates every generator whose runtime flag byte (`+0x99`, serialized `0x69`) has bit `0x10`, unless the zone's indoor flag (`zone+0x178`) is set. In that case it skips generators whose element status marks them as rain, so a covered area starts without rain.
- `ActivateRain` re-activates the rain-marked subset (flag `0x10` set, `0x08` clear, not "never"). `DeactivateRain` stops rain-marked generators that have `0x08` set. Outdoors, therefore, all autorun generators run, and rain activation only toggles the roof-sensitive subset. DATura's `0x6B & 0x20` filter is still an inferred stand-in for the element-status bit, read from a different structure (`GetElemStatus` reads the generator's element header, not `+0x9B`).
- `YmGenerater::GetGenerateNum` emits `(count & 0x1FF) + 1` per emission. A stored count byte of zero therefore means one particle. DATura's `Active()` wrongly rejected these emitters (117 weather emitters, none of them rain) and now accepts them.
- The same `zone+0x178` flag selects a branch's `indo` sound set in `FindPrevSound`/`FindNextSound`. DATura approximates it with the active 0x2F record's indoor flag; the flag's real source is still unknown.

### Replace inferred names with explicit code concepts

Add a small, testable classifier rather than leaving the activation predicate as an anonymous helper in the renderer. The classifier should report why a record is included or excluded:

- wrong weather root;
- serialized inactive flag;
- not eligible for weather activation;
- no emission count;
- unsupported linked resource type;
- missing linked resource.

This will make failures diagnosable and keep the database generator and runtime renderer aligned.

### Acceptance criteria

- Every condition used by DATura's activation predicate is tied to a PS2 source condition or is explicitly labeled as a DATura safety check.
- Unit tests cover each exclusion reason.
- Database counts and runtime counts are produced by equivalent rules.
- La Theine, Pashhow, and Rolanberry still report 8, 2, and 4 active rain generators.

## Priority 1: classify all rain and squall branches

**Status (2026-10-16): classification done; per-branch render captures remain.**

The database contains 73 authored rain or squall branches (58 rain, 15 squall). Thirty-one have no precipitation emitter: 23 rain branches and 8 of the 15 squall branches. The generator now classifies every branch in `zone_rain_branch_audit` and `tools/zone_rain_branch_audit.csv`:

| Classification | Rain | Squalls | Cause |
| --- | ---: | ---: | --- |
| `precipitation` | 35 | 7 | Autorun, weather-batched sprite or mesh emitters. DATura renders these. |
| `environment_only` | 19 | 4 | No generators. These are cave or indoor zones where weather changes only lighting/fog and the ambient loop (for example the Temple of Uggalepih, Labyrinth of Onzozo, and Qulun Dome branches). Correct as rendered. |
| `positional_sound_only` | 2 | 2 | Alzadaal Undersea Ruins `squl`/`squ1`, Arrapago Remnants `rai1`, Silver Sea Remnants `rai1`. Their only generators emit `0x3D` positional sounds. DATura does not play generator-driven sounds yet. |
| `weather_scenery_unsupported` | 2 | 2 | Sealion's Den `rain`/`squl` (sea, coral, and weather meshes), Cloister of Tides `squl` (`tenk` mesh and two `lens` records), Hall of the Gods `rain` (cloud meshes). Their generators link `0x0B` meshes or `0x39` lens records, which the PS2 constructor activates outdoors. DATura's weather pass does not draw non-batched weather generators. |

None of the 31 is a rain decoder or activation bug: no branch hides a weather-batched precipitation emitter. Sound: 72 of 73 branches have a `0x3D` record. Reisenjima Sanctorium `d_hi/weat/rain` has none, and Sealion's Den `rain`/`squl` author only an indoor loop.

Phomiuna Aqueducts (zone 27, `ROM3/0/26.DAT`, root `d_re`) schedules rain on 347 cycle days but authors only `clod`, `fogd`, `mist`, `suny`, `thdr`, and `wind`. The model mapping is correct. The retail client falls back to `suny` for a missing branch, so Phomiuna rain shows the sunshine environment and loop. DATura now resolves it the same way.

Remaining from the original audit list: render results at two times of day per branch, the generator-to-texture selection reason, and implementation of the two unsupported classes.

### Build a branch audit report

For every authored rain and squall branch, record:

- zone ID and zone name;
- model DAT path;
- resource directory;
- environment-record count and names;
- all particle generators, including inactive ones;
- generator flags and status fields;
- linked resource type and name;
- whether a matching mesh exists;
- whether a matching sprite shape exists;
- which texture is selected and why;
- sound-resource names rather than only a count;
- render result at two times of day;
- classification: working precipitation, environment-only, shared-resource, unsupported, or malformed.

Generate this report from the same parser used to build the database so it can be reproduced after parser changes.

### Investigate the scheduled/authored mismatch

Phomiuna Aqueducts is currently the one zone that schedules rain or squalls but has no same-family authored rain/squall branch in its mapped model DAT. Determine whether it:

- intentionally uses only fog/lighting changes;
- inherits weather assets from another DAT;
- has an incorrect model-DAT mapping;
- uses an alias not recognized by `WEATHER_NAMES`;
- stores weather in a secondary resource file.

### Acceptance criteria

- [x] Every one of the 73 authored rain/squall branches has a documented classification.
- [x] Zero-generator branches no longer appear as an unexplained aggregate.
- [x] Phomiuna Aqueducts has a confirmed explanation or a tracked parser/mapping defect.
- [x] The audit can be regenerated with one documented command (`python tools\generate_zone_weather_database.py`).

## Priority 1: connect natural weather scheduling to the runtime

**Status (2026-10-16): offline mode and the manual override are done; server-driven mode remains.**

Implemented:

- `ZoneWeatherSchedule::Table` loads the 2,160-day LandSandBoat cycle and reproduces `entryForDay`. `Select(zone, unixTime)` mirrors LSB's `UpdateWeather`: it re-rolls every 3-30 real minutes and picks rare/common/normal with 15/35/50 percent odds. The interval and roll come from a hash of (zone, Vana'diel day, segment), so a fixed time is repeatable. Segments restart at Vana'diel midnight. LSB's 02:00-07:00 forced-fog rule is not reproduced; LSB's own comment calls that rule inaccurate.
- `ResolveBranch` maps weather IDs to authored branches through an explicit alias table. Missing branches fall back to `suny`, then `fine`, then the first branch, matching the client constructor. The client's own `WeatherResId` table is not recovered, so the primary names are inferred from DAT directory names. 739 of 753 scheduled pairs resolve to an exact branch.
- `main.cpp` applies the selection inside `UpdateZoneEnvironmentState`. It re-evaluates only when the current segment ends, snaps on zone entry, and cross-fades later changes. The title screen keeps its forced sunny sky. Cycle Zone Weather enters a manual override and returns to the schedule after the last branch. The camera-debug overlay marks the mode.

Remaining: server-driven mode (`RecvWeather` number, start time, and offset), and a UI marker outside the camera-debug overlay.

Original analysis: DATura previously built its selectable weather list from authored environment branches and cycles that list manually. The generated schedule database is not yet the authority used during gameplay. Consequently, a user can select a weather branch that is authored in the DAT even when that weather does not naturally occur in the zone, and DATura does not advance through the zone's natural cycle.

### Define a runtime weather service

Create a component that owns:

- current weather ID;
- previous weather ID;
- weather start time;
- scheduled end time or next transition;
- zone ID;
- schedule source;
- manual override state;
- mapping from server weather ID to an authored `/weat` branch;
- transition progress.

Keep schedule selection out of the renderer. The renderer should receive a resolved current branch, previous branch, and blend amount.

### Support two operating modes

1. **Offline simulation:** read the 2,160 Vana'diel-day schedule and choose weather from the current Vana'diel date/time.
2. **Server-driven mode:** accept the weather number, start time, and offset supplied by a compatible server, matching the responsibility observed in `RecvWeather` and `RecWeatherTask`.

Manual weather selection should remain available as an explicit debugging override. The UI should indicate when an override is active.

### Resolve schedule names to resource aliases

The schedule uses semantic weather IDs while retail DAT directories use names such as `rain`, `rai1`, `squa`, `squl`, and `squ1`. Add an explicit, zone-aware resolution table. Do not choose a branch merely because it contains a similar substring.

The resolver must handle:

- multiple authored branches for one semantic weather type;
- stronger-weather variants;
- zones with an environment change but no precipitation generator;
- missing same-family branches;
- clear/sunshine aliases;
- inherited or shared weather resources.

### Acceptance criteria

- [x] A zone never enters a naturally impossible weather type unless a manual override is active. Tested across all 300 zones.
- [x] The offline mode selects repeatable weather for a fixed Vana'diel date/time.
- [ ] Server-driven mode follows the server's weather number and timing.
- [x] Manual selection remains deterministic and visibly marked (camera-debug overlay).
- [x] Schedule-to-resource resolution has tests for aliases and missing branches.

## Priority 1: implement authored weather audio

**Status (2026-10-16): branch loops play; positional generator sounds and authored volume remain.**

Implemented:

- Weather sounds are `0x3D` SeSep records named by HHMM inside each branch (and its `indo` set). The SeWave ID at payload `+8` maps to `sound*/win/se/seNNN/seNNNNNN.spw`. The database records them in `zone_authored_weather_sounds`.
- `ZoneWeatherAudio::SelectSound` follows PS2 `FindPrevSound`: it plays the latest record at or before the current minute. Before the day's first record, it uses the last eligible record in file order. The `indo` set is used only when the zone counts as indoor.
- `ZoneWeatherAudio::Controller` decodes through the existing SeWave path and loops from the authored loop start. It cross-fades over the same 3 seconds as the visuals, never stacks a loop twice, and resumes a fading loop instead of restarting it. It stops immediately when sounds or weather effects are disabled, or when the app is in the background without background audio. Zone unload resets it, and window destruction shuts it down. A missing SeWave leaves visuals running silently. Playback is gated like the other ambient sounds (game mode only); the manual audio browser is separate.

Remaining: decode the SeSep volume and envelope fields (DATura uses a fixed 0.55 gain), play `0x3D`-linked generator sounds (Alzadaal, Arrapago, Silver Sea), identify the real indoor flag (`zone+0x178`), and compare loudness with retail.

Original analysis: the database previously recorded only the number of sound resources in each branch. It does not preserve enough information to play them, and the active weather renderer does not start, stop, or blend weather audio.

Seventy-two of the 73 authored rain/squall branches contain at least one detected sound resource. This makes audio part of the normal authored effect rather than a rare special case.

### Extend resource extraction

Record for each sound resource:

- resource name and directory;
- chunk type and source offset;
- referenced sound bank/file identifier;
- loop settings;
- authored volume;
- distance or positional settings, if present;
- any keyframe or time-of-day control;
- relationship to a generator or environment record.

### Add a weather-audio controller

The controller should:

- start the current branch's loop when the weather becomes active;
- cross-fade the old and new loops during transitions;
- honor the application's sound-enable and background-audio settings;
- stop weather audio on zone unload;
- avoid duplicate playback when several records reference the same loop;
- recover cleanly if a sound resource cannot be decoded;
- keep visual weather running when audio is unavailable.

Reuse the existing DATura audio decoding/playback facilities where practical, but keep weather-loop state separate from the manual audio browser.

### Acceptance criteria

- [x] Entering rain starts the correct authored loop (La Theine 1042 checked against the installed DAT).
- [x] Leaving rain fades it out without an abrupt cut (linear 3-second fade; no retail comparison yet).
- [x] Switching between rain strengths does not stack duplicate loops.
- [x] Disabling sounds immediately silences weather and re-enabling sounds restores only the currently valid loop.
- [x] Zone unload and application shutdown release every weather-audio resource.

## Priority 1: verify transition timing and semantics

DATura currently uses a fixed three-second linear transition in `ZoneEnvironmentState::Update`. That value is a DATura choice. It has not been established as the retail duration or curve.

### Questions to answer

- Does the PS2 client derive transition duration from the server's weather start time and offset?
- Does `YmCombineWeather` use a fixed duration, a resource value, or caller-supplied timing?
- Are lighting, fog, sky shells, precipitation, and audio blended over the same interval?
- Are particle generators cross-faded, or are old generators deactivated while existing particles finish their lifetimes?
- Is the curve linear, eased, stepped, or subsystem-specific?
- What happens when a second weather change arrives before the first transition completes?

### Required tests

- clear to rain;
- rain to clear;
- rain to stronger rain/squalls;
- interrupted transition;
- transition across midnight;
- transition while weather effects are disabled and then re-enabled;
- transition during a zone load.

### Acceptance criteria

- The duration and curve are supported by source tracing or retail measurement.
- Environment, particle, and audio transitions remain synchronized.
- An interrupted transition starts from the currently blended state without popping.
- No stale previous-weather path survives after completion or zone unload.

## Priority 2: complete the particle virtual-machine coverage

The current sprite renderer implements only the generator commands exercised by the existing fixtures. `tests/WEATHER_RENDERING.md` explicitly says it is not a complete general-purpose FFXI particle VM.

### Inventory unsupported commands

During DAT parsing, retain every generator/updater opcode and report whether it is:

- decoded and rendered;
- decoded but ignored;
- partially decoded;
- unknown.

Prioritize opcodes present in rain and squall branches. Likely areas requiring confirmation include:

- rotation velocity and acceleration;
- non-Z rotation axes;
- scale-over-lifetime tracks;
- per-particle color evolution;
- alternate spawn volumes;
- local-space versus world-space velocity;
- parent transform inheritance;
- collision or kill planes;
- turbulence or wind fields;
- emitter delay and finite emission count;
- texture-frame wrapping rules;
- generator-to-generator links.

Do not implement an opcode from its apparent data shape alone. Tie it to decompiled behavior, multiple DAT examples, or controlled retail observation.

### Acceptance criteria

- Every opcode used by a rain or squall branch has a known support status.
- Unsupported opcodes emit one concise diagnostic per resource, not one message per frame.
- Adding an opcode includes a focused fixture that fails before the implementation and passes after it.

## Priority 2: reproduce retail randomization where it matters

DATura deliberately uses a deterministic hash for particle placement. This makes tests stable but does not reproduce the retail random sequence.

Investigate:

- the PS2 random-number generator used by weather particles;
- seed source and reset behavior;
- whether particles are generated on the CPU, VU, or both;
- whether zone load, weather activation, or frame count reseeds the sequence;
- whether multiple emitters share a generator state.

Retain a deterministic test mode even if runtime behavior switches to the retail sequence. A fixed seed should be injectable into regression tests.

### Acceptance criteria

- Runtime particle distribution matches retail well enough to avoid visible repeating patterns.
- Tests remain exactly repeatable with a fixed seed and clock.
- Long sessions do not produce hash-cycle artifacts or numerical drift.

## Priority 2: strengthen rendering correctness tests

The current tests count changed pixels and compare deterministic frames. Add tests that catch more subtle regressions.

### Texture-selection tests

- Assert the exact scoped texture name selected for each fixture.
- Add a fixture where the shared sprite and local weather atlas deliberately have very different colors.
- Fail if the fallback texture is selected while the authored local atlas exists.

### Geometry and motion tests

- Assert card bounds and aspect ratio.
- Measure mean motion direction between frames.
- Confirm acceleration changes displacement by the expected amount.
- Confirm camera-relative and world-relative emitters respond differently to camera translation.
- Test atlas frame boundaries, including the last frame and wrap/reset behavior.

### Blend and depth tests

- Test over bright, dark, and alpha-bearing backgrounds.
- Put an opaque actor-shaped occluder in front of only half the rain field.
- Test overlapping transparent weather cards for stable back-to-front ordering.
- Verify every pipeline state restored by the weather pass, not only the final image.

### Transition tests

- Capture multiple points in a transition rather than only stable weather.
- Assert that old-weather contribution decreases monotonically and new-weather contribution increases monotonically.
- Ensure the sum does not cause an unintended brightness spike.

### Acceptance criteria

- A wrong local texture, reversed velocity, broken depth state, wrong atlas frame, or transition brightness spike produces a specific test failure.
- Capture directories remain unique per zone and weather resource.
- Reference images include metadata describing DAT path, resource path, time, camera, and renderer version.

## Priority 2: performance and resource-lifetime work

The current implementation builds temporary vectors, gathers matching batches, sorts particles, and submits immediate geometry each frame. This is acceptable for validation but should be profiled before the weather system is considered complete.

### Measure first

Collect CPU and GPU timings for:

- a light-rain branch;
- the densest rain/squall branch;
- rain combined with many actors;
- 1080p, 1440p, and 4K;
- uncapped frame rate and a long-running session;
- weather transitions with both old and new sets active.

Record:

- particles generated and visible;
- vertices submitted;
- draw calls;
- transparent-sort time;
- transient allocations;
- texture and buffer changes;
- time spent capturing/restoring renderer state.

### Optimize only demonstrated bottlenecks

Potential options include:

- cache generator-to-mesh and generator-to-texture resolution at zone load;
- reuse particle and vertex buffers;
- replace `DrawImmediate` with a dynamic vertex buffer;
- avoid recapturing invariant state inside a pass;
- cull whole emitters before particle generation;
- cap work using a rule supported by retail behavior rather than an arbitrary visual reduction.

### Acceptance criteria

- No per-frame resource lookup scans the full material table after zone initialization.
- Weather does not create an unbounded number of particles or vertices.
- The densest supported weather stays within an agreed frame-time budget at the target resolution.
- Device loss, zone unload, and renderer shutdown release cached resources.

## Priority 2: database robustness and provenance

The database is generated from two external inputs: installed retail DATs and LandSandBoat's `zone_weather.sql`. Reproducibility needs stronger provenance.

### Record source identity

Add metadata for:

- installed FFXI file version or manifest date;
- hash of every inspected zone DAT, or a separate hash manifest;
- LandSandBoat repository URL and exact commit hash;
- hash of `zone_weather.sql`;
- generator version/schema version;
- generation timestamp;
- parser warnings and failures.

### Improve failure behavior

- Treat a missing schedule file as an explicit incomplete build rather than silently producing an apparently complete database.
- Make unknown weather aliases visible in a report.
- Detect duplicate zone mappings.
- Detect malformed chunk boundaries and include the DAT path and offset in the error.
- Write outputs atomically so an interrupted generation does not leave a partial SQLite/CSV/JSON set.
- Add a schema migration/version policy.

### Add database consistency checks

Automate checks for:

- exactly 300 known zone IDs for the targeted game version;
- valid weather IDs;
- referential integrity;
- unique authored resource paths per zone;
- scheduled weather without a resolved authored representation;
- active generators without matching meshes or sprites;
- sound resources without resolvable audio references;
- unexpected changes in representative-zone generator counts.

### Acceptance criteria

- Two runs against identical inputs produce byte-identical logical content.
- Every output identifies its exact source versions.
- Partial or stale output cannot be mistaken for a successful generation.
- Consistency-check failures return a nonzero process exit code.

## Priority 3: user-facing diagnostics and controls

Add a compact weather diagnostics view for development builds. It should show:

- zone ID and name;
- semantic weather ID/name;
- current and previous resource paths;
- transition progress;
- whether weather is natural, server-driven, or manually overridden;
- active generator count;
- rendered mesh-batch and sprite-emitter counts;
- selected texture names;
- active weather sound resources;
- skipped resources and reasons.

Useful controls include:

- select only weather types valid for the zone;
- enable an explicit “show all authored branches” debug mode;
- restart the current weather effect;
- freeze weather animation time;
- freeze transition progress;
- set a deterministic particle seed;
- export the current diagnostic snapshot.

Diagnostics must be generated by the runtime's actual selection logic. A separate approximation will drift and become misleading.

## Recommended implementation order

Done on 2026-10-16: the 73-branch audit, the Phomiuna explanation, runtime schedule resolution with a manual override, and weather-loop audio.

1. Capture retail comparison footage and settle card, motion, blend, and transition questions.
2. Finish the PS2 activation trace (element status bit, `zone+0x178`) and introduce a shared, testable activation classifier.
3. Render the `weather_scenery_unsupported` generators (`0x0B` meshes, `0x39` lens) and play `0x3D` generator sounds.
4. Add server-driven weather mode.
5. Expand opcode coverage only where the audit proves it is needed (for example Mount Zhayolm dust's `0x02`/`0x1F`).
6. Add texture, geometry, motion, blend, transition, and state-restoration tests.
7. Profile dense weather and remove measured bottlenecks.
8. Add the remaining provenance, atomic generation, consistency checks, and developer diagnostics.

## Definition of done

Rain and weather work can be considered complete when all of the following are true:

- Every naturally scheduled rain and squall case resolves to a deliberately classified visual representation.
- Every authored rain and squall branch has a documented rendering status.
- Weather selection follows the offline schedule or server state, with manual override clearly separated.
- Generator activation and deactivation match the PS2 client behavior documented from source.
- Rain visuals, transitions, and audio have been compared with retail captures.
- Weather audio starts, transitions, stops, and respects application settings.
- Unsupported particle commands used by rain/squalls have been implemented or explicitly accepted with evidence.
- Regression tests cover every distinct rain/squall resource pattern, not merely three representative zones.
- Performance meets the selected frame-time budget in the densest supported case.
- The database records exact source provenance and fails clearly when inputs are incomplete.
- A full DATura build and the complete weather test suite pass from a clean checkout.

## Commands used for the current baseline

Regenerate the weather database:

```powershell
python tools\generate_zone_weather_database.py
```

Build the weather regression executable from a Visual Studio developer command prompt:

```powershell
msbuild tests\WeatherRenderingTests.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1
```

Run the weather regression suite, supplying the installed FFXI root:

```powershell
tests\bin\weather-rendering\Debug\WeatherRenderingTests.exe "C:\Program Files (x86)\PlayOnline\SquareEnix\FINAL FANTASY XI"
```

Build DATura:

```powershell
msbuild DATura\DATura.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1
```

Build and run the schedule/audio suite:

```powershell
msbuild tests\WeatherScheduleTests.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1
tests\bin\weather-schedule\Debug\WeatherScheduleTests.exe "C:\Program Files (x86)\PlayOnline\SquareEnix\FINAL FANTASY XI"
```

The last verified result (2026-10-16) was zero weather-rendering failures, zero weather-schedule failures, and a successful Debug x64 application build.
