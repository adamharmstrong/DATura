# Authored foliage and zone collision

Static zone collision now comes only from the MZB collision grid. Rendered MMB
triangles never imply collision, regardless of object name, material name,
transparency, sidedness, topology, or wind animation. This replaces the earlier
`kusa`/`grass`/flower/atlas-name filter and covers unknown foliage without having
to identify the plant. A tree trunk, hedge, branch, or flowerbed remains solid
where its authored collision mesh says it is solid.

## Source evidence and platform boundary

The local PS2 version 972 decompilation supplies the behavioral reference:
`C:/Users/nanob/source/repos/FFXI-PS2/src/ohno/ko_chara.cpp`,
`KO_MapLocation::CheckChips` (line 19) and `CheckParts` (line 160).
They skip disabled placements, select groups using placement flags, and submit
selected polygons to `CollisionDetect`. They do not classify plants by names,
texture alpha, or visual sidedness. `KO_Vum` has distinct triangle, transparent,
and collision groups at offsets 0x08, 0x0c, and 0x10 respectively.

The PS2 source uses a different resource layout from PC MZB/MMB data. DATura
therefore consumes the PC assets' already-authored collision records rather than
copying PS2 bit positions into PC placements. This establishes data-driven static
collision; it does not certify bit-for-bit equivalence with PS2 assets or the
PS2 movement solver. Paired PS2 zone assets and in-client comparison would be
needed for that stronger claim.

## Decoder corrections

The collision reader now observes the actual MZB structure:

- A zero collision-table or grid pointer means no collision table/grid.
- Grid dimensions use both header dimensions and bucket sizes:
  width = header[0x0c] * header[0x0e] / 4; height similarly uses 0x0d/0x0f.
  Bucket sizes in installed data include 20, 40, and 80; a fixed multiplier of
  ten missed authored collision, including Port Bastok and Port Jeuno.
- A bucket's low 14 bits give its pair count. Pairs are not zero-terminated.
  The previous scan could read subsequent data as extra collision instances.
- The mesh triangle count is unsigned 16-bit.

The independent reference implementation in
`tmp/xi-test-client-review/renderer/ffxi/mzb.cpp` also describes those grid and
counted-list layouts. `tools/audit_authored_collision.py` separately counts and
validates raw MZB records to check the runtime export, without inspecting MMB
names or rendering state.

## Moving objects

Static rendering never generates collision. Existing moving-solid systems opt
in explicitly through `Model_FF11_CollectMovingObjectCollision`: door registration
and the two existing Metalworks elevator bindings. Only the highest-detail
available LOD is collected, and scene reflection is undone for both positions
and winding before runtime conversion. This preserves the existing dynamic
object behavior; it is not a newly reconstructed PS2 dynamic collision system.

## Validation

- Synthetic fixtures vary names (including unknown and collision-like names),
  texture names, sidedness, list/strip topology, and static/wind vertex layouts.
  Authored collision is preserved; un-authored visible geometry adds none.
- Fixtures place valid-looking records outside the declared grid/pair count to
  prove those records are not consumed. Explicit moving registration is
  idempotent and round-trips reflected coordinates and winding.
- The installed-zone runtime audit covers 298 entries, 1,023,541 static
  placements, and 97,282,749 authored triangle references, with zero failures.
  Repeated grid references are counted; these are not unique triangles.
  Two entries (278 and 279) contain no scene/collision data.
- The independent raw-record audit agrees for all 298 entries and validates
  index bounds and declared counts. Results are in
  `artifacts/authored-collision-audit.csv` and
  `artifacts/authored-collision-independent.txt`.
- Door interaction: 101 checks pass, including 30 Bastok Markets leaves and
  traversal of all 15 opened paired doorways. Room-loading checks pass across
  the existing city fixtures. All 28 zone-transition movement cases pass in
  both scene orientations. Coordinate checks pass with companion-room collision
  and 111 of 112 catalog NPCs finding floors in Bastok Markets.
- Both actual Metalworks elevator platforms register moving collision in native
  and reflected scenes. Synthetic checks also verify winding restoration.

A full application build was attempted, but existing unrelated renderer
interface changes leave `D3D9Graphics::Context` abstract. No new executable is
claimed until that workspace build issue is resolved.
