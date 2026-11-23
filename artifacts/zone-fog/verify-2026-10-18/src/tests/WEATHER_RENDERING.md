# Authored weather particles

Build `tests/WeatherRenderingTests.vcxproj` for Debug/x64, then run `tests/bin/weather-rendering/Debug/WeatherRenderingTests.exe` with the FFXI installation root as its sole argument. The hidden D3D9 fixture writes captures below `tests/bin/weather-rendering/captures`.

The 2026-09-09 implementation replaces the generic snow cards and the missing dust pass with model-owned sprite geometry/textures resolved from the zone and `ROM/0/0.DAT`. It retains the existing mesh-batch path for rain resources. All active emitters are considered. Camera-following dust and world-positioned snow use their authored placement, scale, spawn spread, velocity/variance, lifetime alpha, clock color/alpha, atlas progression, and distance fade. Missing sprite resources do not generate substitute geometry. Render state is restored after the weather pass.

Regression fixtures: Konschtat dust (`ROM/0/90.DAT`), Beaucedine snow (`ROM/0/122.DAT`), Garlaige blizzard (`ROM/1/19.DAT`), and rain in La Theine (`ROM/0/115.DAT`), Pashhow (`ROM/0/125.DAT`), and Rolanberry (`ROM/0/126.DAT`). The rain fixtures also lock in their distinct PS2-style active generator sets (8, 2, and 4 respectively), preventing the renderer from collapsing every zone onto one generic rain setup. Checks cover parsed emitter settings, visible and changing pixels, deterministic fixed-time frames, opaque-depth occlusion, inactive emitter suppression, lifetime opacity, sprite-frame animation, camera-following dust, and world-positioned snow.

The same suite checks authored weather audio against the installed DATs: La Theine rain selects SeWave 1042 outdoors and 1045 indoors, La Theine sunshine selects 1001 from 06:00 and 1003 from 18:00, and every rain fixture has an outdoor loop. A Mount Zhayolm dust fixture (`ROM4/0/14.DAT`, `f_ma/weat/dust`) covers emitters whose stored count byte is zero. PS2 `YmGenerater::GetGenerateNum` emits count + 1, so these emitters must still draw. Before the 2026-10-16 fix, DATura skipped 117 such weather emitters: Mount Zhayolm dust, Riverne Sites #A01/#B01 gales, and 38 of 39 snow emitters in Arrapago Reef, Ilrusi Atoll, and Talacca Cove. That fixture checks only that the emitter draws. Its spread and velocity opcodes (`0x02`, `0x1F`) are not decoded yet, so the full motion checks do not apply.

## Natural weather schedule and audio logic

Build `tests/WeatherScheduleTests.vcxproj` for Debug/x64 and run `tests/bin/weather-schedule/Debug/WeatherScheduleTests.exe "<FFXI root>"`. It reads `DATura/zone_weather_schedule.bin` from the source tree; a second argument overrides that path. The suite covers:

- LSB day semantics: packed normal/common/rare values, carry-forward entries, the 2,160-day wrap, and the Vana'diel day boundary (3,456 real seconds);
- offline selection: repeatable results for a fixed time, contiguous 3-30 real-minute segments cut at Vana'diel midnight, roughly 15/35/50 percent rare/common/normal odds, independent zones, and no naturally impossible weather in any of the 300 zones;
- branch resolution: exact families and the `rai1`/`squ1`/`fog1` aliases, the client's `suny` fallback for missing branches (Phomiuna Aqueducts rain), then `fine`, then the first branch;
- sound selection: the PS2 `FindPrevSound` rules, including the `indo` set and the file-order fallback before the day's first record;
- the XAudio2 loop controller at zero volume, when the FFXI root is given. It checks start, no stacked voices, cross-fade, resuming a fading loop, immediate silence when disabled, restore on re-enable, fade-out to clear weather, zone-unload reset, and a missing SeWave. The controller check is skipped when no audio device is available.

The deterministic spawn distribution is not intended to reproduce the retail random sequence. This implements the weather sprite commands exercised by these fixtures, not the complete general-purpose FFXI particle VM.

Format references were checked against the installed DAT bytes, [Kuluu particle decoder](https://github.com/jondwillis/kuluu-ffxi/blob/main/ffxi-dat/src/particle_gen.rs), and [Xim's published source](https://xim.pages.dev/source.zip), particularly ParticleInitializers.kt / ParticleUpdaters.kt. Source references were used to interpret fields; game textures are read from the user's installation and are not bundled.

The obsolete Konschtat cloud-smoothing exemption was also removed after confirming the earlier white-band bug was caused by missing color curves and shader compilation. The sky suite passed for all six Konschtat weather groups at midnight and noon; cloudy/mist captures were inspected for blending and stipple. Debug DATura built successfully.
