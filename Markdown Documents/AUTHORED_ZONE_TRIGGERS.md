# Authored zone triggers

DATura reads RID resources (DAT type 0x36) from the loaded zone model DAT.
The PS2 reference is `FFXI-PS2/src/ohno/ko_rid.cpp`, specifically
`YmRidRes::Open`, `KO_RectData::RotClamp`, `KO_RectData::HitCheck`, and
`KO_Rid::HitCheck`. `include/KO_RectData.h` supplies the 64-byte record layout.

RID payload offset 0x10 points to a table containing a count, 12 reserved bytes,
and 64-byte records. The relevant fields are position at 0x00, Y rotation at
0x10, full dimensions at 0x18, token at 0x24, target token at 0x28, and zone at
0x2c. ID letters are folded to lowercase; dimensions are made positive.
The ordinary retail zone check selects `z` records with a nonzero target.
The player is translated by the negative center, rotated by negative yaw, and
scaled by reciprocal dimensions. All three local coordinates must lie strictly
between -0.5 and +0.5. This is containment, with no outward-only requirement.
Record order determines the first hit. The actor update requests zoning only
outside an active event or existing zone-change request (`xiatelnet.cpp:8151`).

The installed PC Bastok Markets ROM/1/35.DAT has these residential records:

| Token | Center | Y rotation | Full dimensions |
| --- | --- | --- | --- |
| zmra | (-146.16008, -6.76603, 29.92008) | pi/2 | (10, 8, 2) |
| zmrc | (-146.16814, -6.78117, -30.32883) | pi/2 | (10, 8, 2) |

These are current installed DAT bytes interpreted using the PS2 layout, not a
claim that the 2003 map bytes were identical. Both records specify zone 254;
DATura routes them to its local Bastok Mog House model (301). Destination
selection, interior instances, and arrival positions remain local route policy,
not a recreation of the retail server's zone-change protocol.

When a zone supplies usable RID triggers, approximation-only routes cannot fire.
RID tokens without a local destination route remain unsupported. Synthetic
interiors lacking RID records retain the older plane fallback.
Config > Debug provides independent displays for active triggers (red boxes)
and zero-target records (blue boxes). Both use the authored dimensions and
rotation, respect scene depth, and persist independently. Zero-target records
are retained for visualization but remain excluded from movement hit checks.
Markers use the near face and width of the same authored volume, with the
requested 1.5-unit approach offset, player-relative height, and depth testing.

Compatibility difference: the PS2 tests discrete interpolated positions using
`DAT_0054c2ac`, whose value is not recovered in the available source. DATura
uses a continuous segment/box intersection with identical strict bounds to
avoid tunneling on long frames. Exact PS2 sampling equivalence is unverified.

Tests accept the installed Bastok Markets model DAT as an optional argument and
check decoding, both mirror modes, both residential routes, stationary
containment, wrong-floor rejection, passage-width rejection, and truncation.
