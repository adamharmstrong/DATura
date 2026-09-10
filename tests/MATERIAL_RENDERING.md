# Material, depth and fog regression

Build `MaterialRenderingTests.vcxproj` with Visual Studio v145, x64, Debug or Release.
Run `tests/bin/material-rendering/Release/MaterialRenderingTests.exe [capture-directory]`
from the repository root. No retail DATs are required. The test creates a hidden native
D3D9 HAL device and reads actual pixels; it requires desktop GPU access and ps_2_0.

The 157 assertions cover opaque alpha suppression, hard-cutout thresholds, combined
texture/vertex alpha, real compressed DXT3 alpha expansion, RGB saturation, textured
to untextured transitions, transparent sorting and depth writes, fog endpoints and
interpolation, invalid fog records, midnight interpolation, standalone-model reset,
sky fog exclusion, and pass-state restoration. Pixel expectations are analytic with
small channel tolerances for GPU rounding. BMP captures are written locally.

On 2026-09-09 the initial run reproduced four failures: both material paths retained
the previous textured pixel shader when the next surface had no texture. The opaque
surface became black and the transparent surface disappeared. Explicitly clearing
that shader restores vertex color. The unchanged tests then passed 157/157.

These tests establish DATura's material and depth contract; they are not a retail
screenshot comparison or a claim of universal FFXI lighting fidelity.
