# Zone fog verification run, 2026-10-16

Source: `src/` is a copy of `DATura/` and `tests/` taken at the start of the
run (the shared checkout was being edited by other sessions). `source-manifest.txt`
records HEAD (`aab1bc6`), the uncommitted files at snapshot time
(`DATura/directional_shadow_map.inl` and `tests/material_rendering_tests.cpp`, both
from concurrent shadow work), and SHA-256 prefixes of every snapshot source.
After the weather-blend color fix, the snapshot's `DATura/zone_environment_state.cpp`
and the new `tests/zone_fog_transition_tests.cpp` / `ZoneFogTransitionTests.vcxproj`
were refreshed from the working tree and rebuilt (the manifest hashes predate that).

Environment: Windows 11 Home 10.0.26300; VS 18 Community MSBuild, toolset v145,
MSVC 14.51.36231, WindowsTargetPlatformVersion 10.0 (latest installed SDK).
GPUs: Intel Iris Xe (driver 31.0.101.4502) and NVIDIA RTX 4070 Laptop (driver
32.0.16.1091). The tests create the device on `D3DADAPTER_DEFAULT`, HAL; which
physical GPU that maps to was not logged. FFXI installation:
`C:/Program Files (x86)/PlayOnline/SquareEnix/FINAL FANTASY XI`.

Builds (Release x64 with PDBs, own OutDir/IntDir under `bin/` and `obj/`):

| Binary | Log |
| --- | --- |
| `bin/Release/MaterialRenderingTests.exe` | `build-tests-release.log` |
| `bin/AppRelease/DATura.exe` | `build-app-release.log` |
| `bin/TransitionRelease`, `bin/TransitionDebug` `ZoneFogTransitionTests.exe` | console |
| `bin/Release/SkyRenderingTests.exe`, `WeatherRenderingTests.exe` | console |

Results (native exit codes checked; all 0):

| Run | Log | Result |
| --- | --- | --- |
| `--shadow-temporal` | `shadow-temporal.log` | 854 assertions, 0 failures; 1920² grazing frames 0-4 darkest=255, 0 false pixels |
| `--materials-only` | `materials.log` | 230 assertions, 0 failures |
| `--zone-fog` | `zones.log` | 277 assertions, 0 failures |
| full suite (pre-fix) | `full-suite.log` | 2,661 assertions, 0 failures |
| ZoneFogTransitionTests (post-fix) | `transitions.log` | 60 assertions, 0 failures (3 consecutive runs) |
| full suite (post-fix) | `full-suite-postfix.log` | 2,661 assertions, 0 failures |
| `--zone-fog` (post-fix) | `zones-postfix.log` | 277 assertions, 0 failures |
| SkyRenderingTests (post-fix) | `sky.log` | 0 failures |
| WeatherRenderingTests (post-fix) | `weather.log` | 0 failures |
| Debug breakpoints | `debugger-breakpoints.log` | 2 of 2 bound and hit |

Before the fix, ZoneFogTransitionTests failed 9 assertions (red/blue swapped during
weather blends) plus the empty-records case.

Debugger check: no cdb/WinDbg is installed, so a small Win32 Debug API + DbgHelp
harness (kept outside the repository) loaded the Debug PDB, resolved
`zone_environment_state.cpp:143` (weather-blend fog color) and `:360` (time-of-day
fog color) to code addresses, set int3 breakpoints, and recorded hits with
source-line stack frames. The Visual Studio IDE debugger itself was not driven.
