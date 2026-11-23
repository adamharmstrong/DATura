# Sky rendering regression

Build `tests/SkyRenderingTests.vcxproj` for Debug/x64, then run the resulting executable with the FFXI installation root as its argument. It creates a hidden D3D9 device and captures `ROM/0/90.DAT` (Konschtat) at the reported camera direction, at midnight and noon. Captures are written to `tests/bin/sky-rendering/captures`.

The test checks both fine cloud generators resolve `kcr1`, `kcg1`, and `kcb1`, checks the midnight red curve evaluates to 0.04, requires successful creation of the shared texture shader, and compares rendered pixels with a deliberately missing-curve control. Motion is frozen and keyframe values are sampled at the requested time.

On 2026-09-09 the Debug test passed with zero failures. Near-white pixels fell from 63,541 to zero at midnight and from 109,093 to 7,372 at noon. Captures were visually inspected. This validates the sky pass independently of the title UI and terrain.

The fixes address two failures: extended weather curve opcodes place their resource name after a flags word, and strict HLSL compilation rejects the legacy sampler2D syntax used by the ps_2_0 shaders.
