# Third-party notices

DATura is licensed under the GNU General Public License, version 3 or later. Material identified below remains subject to its own copyright and license terms.

## Vulture DAT Extractor

`DATura/Vulture_DAT_Extractor.cs` and `DATura/Vulture_DAT_Extractor_Readme.txt` identify their author as `vulture` and their license as public domain.

## Noesis-derived source

The following source carries attribution to Rich Whitehouse or identifies itself as Noesis FF11 support:

- `DATura/CNoeR3000A.cpp`
- `DATura/CNoeR3000A.h`
- `DATura/model_ff11_fixed.cpp`
- `DATura/model_ff11.h` and the associated `model_ff11_*.inl` files

No explicit redistribution license was found alongside this material. Its existing attribution notices have been preserved. Redistribution authorization for these files should be confirmed before DATura is publicly released.

## Reference projects

The independent project under `tools/kuluu-ffxi-reference/` and its vendored dependencies are governed by the license files contained in that directory tree. Development references under `.agents/`, when present, likewise retain their upstream licenses and are not relicensed as DATura code.

## Windower/POLUtils

`DATura/ffxi_resource.cpp` ports and modifies resource-format behavior from
[Windower/POLUtils](https://github.com/Windower/POLUtils) at commit
`b28ae7007e9c8fa2e20721351c0a1dabd3a6a803`.

POLUtils copyright notices identify Tim Van Holder, Nevin Stepan, and the
Windower Team. POLUtils is licensed under the Apache License, Version 2.0. A
copy of that license is provided in
[`LICENSE-POLUTILS-APACHE-2.0.txt`](LICENSE-POLUTILS-APACHE-2.0.txt). DATura's
port includes additional bounds checks, unified format detection, modern
`d_msg` layout disambiguation, and a native Win32 presentation layer.

## FINAL FANTASY XI

FINAL FANTASY XI and its game data, artwork, audio, names, and other assets are not licensed under DATura's GPL license. DATura should operate on files supplied by a user's own installation rather than redistribute those assets.
