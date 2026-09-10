# Authored weather particles

Build `tests/WeatherRenderingTests.vcxproj` for Debug/x64, then run `tests/bin/weather-rendering/Debug/WeatherRenderingTests.exe` with the FFXI installation root as its sole argument. The hidden D3D9 fixture writes captures below `tests/bin/weather-rendering/captures`.

The 2026-09-09 implementation replaces the generic snow cards and the missing dust pass with model-owned sprite geometry/textures resolved from the zone and `ROM/0/0.DAT`. It retains the existing mesh-batch path for rain resources. All active emitters are considered. Camera-following dust and world-positioned snow use their authored placement, scale, spawn spread, velocity/variance, lifetime alpha, clock color/alpha, atlas progression, and distance fade. Missing sprite resources do not generate substitute geometry. Render state is restored after the weather pass.

Regression fixtures: Konschtat dust (`ROM/0/90.DAT`), Beaucedine snow (`ROM/0/122.DAT`), Garlaige blizzard (`ROM/1/19.DAT`), and La Theine rain (`ROM/0/115.DAT`). Checks cover parsed emitter settings, visible and changing pixels, deterministic fixed-time frames, opaque-depth occlusion, inactive emitter suppression, lifetime opacity, sprite-frame animation, camera-following dust, and world-positioned snow. The four fixtures passed with zero failures; captures were visually inspected.

The deterministic spawn distribution is not intended to reproduce the retail random sequence. This implements the weather sprite commands exercised by these fixtures, not the complete general-purpose FFXI particle VM.

Format references were checked against the installed DAT bytes, [Kuluu particle decoder](https://github.com/jondwillis/kuluu-ffxi/blob/main/ffxi-dat/src/particle_gen.rs), and [Xim's published source](https://xim.pages.dev/source.zip), particularly ParticleInitializers.kt / ParticleUpdaters.kt. Source references were used to interpret fields; game textures are read from the user's installation and are not bundled.

The obsolete Konschtat cloud-smoothing exemption was also removed after confirming the earlier white-band bug was caused by missing color curves and shader compilation. The sky suite passed for all six Konschtat weather groups at midnight and noon; cloudy/mist captures were inspected for blending and stipple. Debug DATura built successfully.
