"""Compare the runtime audit with independent counts from installed MZB records.

No MMB object names, texture names, or rendering flags participate.
Usage: python tools/audit_authored_collision.py GAME_ROOT RUNTIME_AUDIT
"""
import re
import struct
import sys
from collections import Counter
from pathlib import Path

from inspect_zone_collision import decrypt_object_map, parse_chunks


def native_counts(data):
    meshes = triangles = 0
    dimensions = Counter()
    for chunk in parse_chunks(data):
        if chunk["type"] != 0x1C:
            continue
        b = decrypt_object_map(chunk["payload"])
        u32 = lambda offset: struct.unpack_from("<I", b, offset)[0]
        table = u32(8)
        if not table:
            continue
        width, height = b[12] * b[14] // 4, b[13] * b[15] // 4
        dimensions[(b[14], b[15])] += 1
        grid = u32(table + 16)
        if not grid:
            continue
        assert grid + width * height * 4 <= len(b), "grid outside MZB"
        mesh_sizes = {}
        for cell in range(width * height):
            bucket = u32(grid + cell * 4)
            if not bucket:
                continue
            count = u32(bucket) & 0x3FFF
            assert bucket + 4 + count * 8 <= len(b), "bucket outside MZB"
            for pair in range(count):
                transform, geometry = struct.unpack_from("<II", b, bucket + 4 + pair * 8)
                if not transform or not geometry:
                    continue
                assert transform + 64 <= len(b), "transform outside MZB"
                if geometry not in mesh_sizes:
                    vertices, normals, indices, faces = struct.unpack_from("<IIIH", b, geometry)
                    assert vertices <= normals <= indices, "unordered mesh blocks"
                    assert indices + faces * 8 <= len(b), "triangles outside MZB"
                    vertex_count = (normals - vertices) // 12
                    assert 0 < vertex_count <= 0x3FFF, "unsupported vertex count"
                    # Check the declared faces instead of accepting the runtime's
                    # exported count as proof that nothing was silently skipped.
                    for face in range(faces):
                        raw = struct.unpack_from("<3H", b, indices + face * 8)
                        assert all((index & 0x3FFF) < vertex_count for index in raw), "invalid vertex index"
                    mesh_sizes[geometry] = faces
                faces = mesh_sizes[geometry]
                if faces:
                    meshes += 1
                    triangles += faces
    return meshes, triangles, dimensions


def main():
    root, audit_path = Path(sys.argv[1]), Path(sys.argv[2])
    runtime = {}
    for line in audit_path.read_text(encoding="utf-8-sig").splitlines():
        if re.fullmatch(r"\d+,\d+,\d+,\d+", line):
            zone, placements, meshes, triangles = map(int, line.split(","))
            runtime[zone] = meshes, triangles
    table = (Path(__file__).resolve().parents[1] / "DATura/zone_dat_table.h").read_text()
    zones = re.findall(r'\{\s*(\d+),\s*"[^"]*",\s*"([^"]+\.DAT)"', table)
    failures = checked = 0
    dimensions = Counter()
    for zone, path in zones:
        zone = int(zone)
        if zone not in runtime:
            continue
        try:
            meshes, triangles, sizes = native_counts((root / path).read_bytes())
            dimensions.update(sizes)
            if runtime[zone] != (meshes, triangles):
                raise AssertionError(f"runtime={runtime[zone]} native={(meshes, triangles)}")
            checked += 1
        except (AssertionError, struct.error) as error:
            print(f"FAIL zone={zone}: {error}")
            failures += 1
    assert checked + failures == len(runtime), "incomplete zone comparison"
    print(f"Independent MZB audit: checked={checked} failures={failures} bucket-sizes={dict(dimensions)}")
    return bool(failures or not checked)


if __name__ == "__main__":
    sys.exit(main())
