# Third-party notices

DATura is licensed under the GNU General Public License, version 3 or later. Material identified below remains subject to its own copyright and license terms.

## Individual upstream authors and contacts

The following roster covers the identifiable individuals whose source code or
algorithms are directly present in, ported into, or adapted by DATura. It does
not include organizations, data-only contributors, AI tools, or authors of
dependencies contained only in standalone research-reference trees.

| Individual or alias | Code lineage | Source-published contact information |
| --- | --- | --- |
| Rich Whitehouse | Noesis FF11 support and the Noesis R3000A core | [Official website](https://richwhitehouse.com/); no public email was identified in the supplied source. |
| vulture | Vulture DAT Extractor | No contact information was published in the supplied source. |
| RZN | MapLib/FFXIEncryption map lookup and decryption algorithms | No direct contact information was published with the preserved source; see the [public source-release and permission notice](https://ffximodding.blogspot.com/2018/12/). |
| GalkaReeve | FFXI DAT/map-viewer parsing and rendering research | [GitHub profile and upstream repository](https://github.com/galkareeve/ffxi); no public email was identified. |
| Tomato | FFXI Tool; identified as its original creator and apparent project lead | Historical, reportedly inactive address: `tomato1989@lycos.jp`. |
| Fo | FFXI Tool; probable team member/contact | Historical, reportedly inactive address: `fo@cosmosa.jp`. The later site contact `fon2@cosmosa.jp` may be a newer address or alias for Fo. |
| Akake | FFXI Tool; probable team member/contact | Historical, reportedly inactive address: `akake@platon.co.jp`. |
| Salsa877 | FFXI Tool; probable team member/contact | Historical, reportedly inactive address: `salsa877@hotmail.com`. |
| Tim Van Holder | Windower/POLUtils resource readers | [Windower/POLUtils repository](https://github.com/Windower/POLUtils); no individual contact address was identified in the cited POLUtils source. |
| Nevin Stepan | Windower/POLUtils resource readers | [Windower/POLUtils repository](https://github.com/Windower/POLUtils); no individual contact address was identified in the cited POLUtils source. |

Contact details are reproduced only when published by an upstream source. The
historical FFXI Tool addresses are retained for attribution and should not be
assumed to remain operational or to belong to the same people today.

## Vulture DAT Extractor

`DATura/Vulture_DAT_Extractor.cs` and `DATura/Vulture_DAT_Extractor_Readme.txt` identify their author as `vulture` and their license as public domain.

## Noesis-derived source

The following source carries attribution to Rich Whitehouse or identifies itself as Noesis FF11 support:

- `DATura/CNoeR3000A.cpp`
- `DATura/CNoeR3000A.h`
- `DATura/model_ff11.cpp`
- `DATura/model_ff11_creation.cpp`
- `DATura/model_ff11.h` and the associated private `model_ff11_*_handler*.h` files

No explicit redistribution license was found alongside this material. Its existing attribution notices have been preserved. Redistribution authorization for these files should be confirmed before DATura is publicly released.

## RZN MapLib and FFXIEncryption

`DATura/model_ff11_decrypt.h` adapts map lookup and DAT chunk-decryption
algorithms from RZN's MapLib/FFXIEncryption work. RZN authorized the preserved
MapLib and MapViewer source to be shared publicly in 2018, as recorded in the
[source-release notice](https://ffximodding.blogspot.com/2018/12/). The source
is identified there as copyright RZN; its precise redistribution and
derivative-work license terms should be confirmed before DATura is publicly
released.

## GalkaReeve FFXI projects

DATura's map parsing, material interpretation, and rendering research was
informed by GalkaReeve's FFXI DAT reverse-engineering projects. The upstream
project and its public contact route are available at
[galkareeve/ffxi](https://github.com/galkareeve/ffxi). No explicit license was
found in the preserved project snapshot, so redistribution authorization for
any directly derived portions should be confirmed before public release.

## Reference projects

The independent project under `tools/kuluu-ffxi-reference/` and its vendored dependencies are governed by the license files contained in that directory tree. Development references under `.agents/`, when present, likewise retain their upstream licenses and are not relicensed as DATura code. These standalone reference trees are not included in the individual roster above merely because they are present in the workspace.

## FFXI Tool

DATura's FFXI format research was informed in part by the legacy FFXI Tool
source. The historical website note identifies the following people or contact
identities associated with that project:

- Tomato — `tomato1989@lycos.jp`; identified as the original creator and
  apparent project lead.
- Fo — `fo@cosmosa.jp`; identified as a probable team member/contact.
- Akake — `akake@platon.co.jp`; identified as a probable team member/contact.
- Salsa877 — `salsa877@hotmail.com`; identified as a probable team
  member/contact.

The later website lists `fon2@cosmosa.jp` as its current contact. This may be a
newer address or alias for Fo, so `fon2` is not counted here as a confirmed
fifth individual. The association of Fo, Akake, and Salsa877 with the project
team is an inference made by the author of the historical note from the listed
contact addresses, rather than a conclusive author roster. The FFXI Tool
material retains its original terms; no explicit redistribution license has
been identified.

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

## LandSandBoat NPC placement data

`DATura/npc_placements.csv` is generated from LandSandBoat's
`sql/npc_list.sql` at commit `cc243c661a1c30c1e7fc4e0612601a7a3f2410ac`.
LandSandBoat is licensed under the GNU General Public License, version 3. The
upstream project and corresponding source are available from
[LandSandBoat/server](https://github.com/LandSandBoat/server). The generated
catalog contains zone/entity identifiers, transforms, names, and appearance
selectors; it does not contain Square Enix DAT assets.

## FINAL FANTASY XI

FINAL FANTASY XI and its game data, artwork, audio, names, and other assets are not licensed under DATura's GPL license. DATura should operate on files supplied by a user's own installation rather than redistribute those assets.
