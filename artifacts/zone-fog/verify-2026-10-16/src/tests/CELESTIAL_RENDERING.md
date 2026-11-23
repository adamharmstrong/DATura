# Sun and moon rendering

Generator attachment modes 14 and 15 select celestial resources, without a zone
or mesh-name whitelist. The normal weather sky pass draws these alongside the
existing camera shells. Mesh scale remains authored; generator RGB/alpha curves,
blend modes, weather selection, and weather-transition opacity still apply.
Untextured celestial geometry now uses the existing texture shader's white-texel
path so it receives these controls too.

The local PS2 decompile provides the transform references:

- `src/main/zone/xiarea.cpp`, `XiArea::SetSunMoonTime`: secondary direction
  `(sin(time), cos(time), 0)` at 900 units from the camera; main direction offset
  by pi at 1000 units. Native DAT Y points downward.
- `src/main/miyagawa/ymattach.cpp`, `YmAttachments::MakeAttachMatrix`, cases
  14 and 15: attach to main and secondary light positions.
- `src/main/miyagawa/effect/ymelem.cpp`, `YmElem::CalcRotAhead`, mode 3:
  orient local +X toward the camera.

`SkyRenderingTests` reads the installed Konschtat DAT and checks real sun/moon
pixel output, camera following, zero-alpha suppression, and all six weather sky
groups at midnight and noon. Captures `celestial-600.bmp` and
`celestial-1200.bmp` aim toward the sun and moon. The sun resource is a large
authored halo (80x scale); the moon resource is a bounded disk/halo (20x scale).

This implements generator-owned MapGeo celestial meshes. It does not add a lunar
phase scheduler, lens flare sprite effects, or ship-path attachment rotation.
Other outdoor zones use the same draw path but have not all been visually audited.
