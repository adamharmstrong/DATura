# Shop-sign depth regression

Build `SignRenderingTests.vcxproj` for Debug/x64 and run from the repository root:

```powershell
& tests/bin/sign-rendering/Debug/SignRenderingTests.exe 'C:/Program Files (x86)/PlayOnline/SquareEnix/FINAL FANTASY XI/ROM/1/35.DAT'
```

Requires installed retail Bastok Markets data and a D3D9-capable desktop session.
The test creates a hidden window and isolates `zakka_bord_p`, the mug shop sign.
It renders the same oblique camera with near planes 0.01 (old), 0.1 (production),
and 1.0 (high-precision reference). BMP captures are written under
`tests/bin/sign-rendering/`, numbered 0 through 2 respectively.

It requires the original setting to reproduce the artifact, then checks that
the corrected setting reduces differing pixels by at least 75%. Differences
below a summed RGB error of 24 are ignored. On the development GPU, the counts
were 29,190 old versus 0 corrected. No sign triangles or materials are removed.

The zone camera now clips at 0.1 world units; standalone model viewing retains
its 0.01 near plane. Terrain and sky use the same near plane.
