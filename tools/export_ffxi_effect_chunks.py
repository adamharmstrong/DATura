#!/usr/bin/env python3
import argparse
import struct
from pathlib import Path


def cstr(raw):
    return raw.split(b"\0", 1)[0].decode("latin-1", "replace").rstrip()


def read_u32(data, ofs):
    return struct.unpack_from("<I", data, ofs)[0]


def read_u16(data, ofs):
    return struct.unpack_from("<H", data, ofs)[0]


def read_f32(data, ofs):
    return struct.unpack_from("<f", data, ofs)[0]


def safe_name(name):
    out = []
    for ch in name.strip():
        out.append(ch if ch.isalnum() or ch in ("_", "-") else "_")
    return "".join(out).strip("_") or "chunk"


def iter_chunks(data):
    ofs = 0
    index = 0
    while ofs <= len(data) - 16:
        info = read_u32(data, ofs + 4)
        chunk_type = info & 0x7F
        size = (info >> 3) & 0x7FFFF0
        if size <= 0 or ofs + size > len(data):
            break
        yield index, ofs, cstr(data[ofs:ofs + 4]), chunk_type, data[ofs + 16:ofs + size]
        ofs += size
        index += 1


def parse_type25(payload):
    if len(payload) < 0x20:
        return None

    header_words = [read_u16(payload, i * 2) for i in range(8)]
    vertex_count = header_words[3]
    index_ofs = header_words[4]
    pos_ofs = header_words[5]
    attr_ofs = header_words[6]
    uv_ofs = header_words[7]
    material = cstr(payload[0x10:0x20])

    if vertex_count <= 0:
        return None
    if pos_ofs <= 0 or index_ofs <= 0:
        return None
    if pos_ofs + vertex_count * 16 > len(payload):
        return None
    if index_ofs + vertex_count * 6 > len(payload):
        return None

    positions = []
    for i in range(vertex_count):
        base = pos_ofs + i * 16
        positions.append((read_f32(payload, base + 0), read_f32(payload, base + 4), read_f32(payload, base + 8)))

    uvs = []
    if uv_ofs > 0 and uv_ofs + vertex_count * 12 <= len(payload):
        for i in range(vertex_count):
            base = uv_ofs + i * 12
            uvs.append((read_f32(payload, base + 0), 1.0 - read_f32(payload, base + 4)))

    triangles = []
    # Observed 0x25 chunks use vertex_count * 3 u16 indices, yielding vertex_count triangles.
    for i in range(vertex_count):
        base = index_ofs + i * 6
        tri = (read_u16(payload, base + 0), read_u16(payload, base + 2), read_u16(payload, base + 4))
        if max(tri) < vertex_count and len(set(tri)) == 3:
            triangles.append(tri)

    return {
        "header_words": header_words,
        "material": material,
        "vertex_count": vertex_count,
        "index_ofs": index_ofs,
        "pos_ofs": pos_ofs,
        "attr_ofs": attr_ofs,
        "uv_ofs": uv_ofs,
        "positions": positions,
        "uvs": uvs,
        "triangles": triangles,
    }


def write_obj(path, chunk_name, parsed):
    with path.open("w", encoding="utf-8", newline="\n") as f:
        f.write(f"# FFXI effect/environment chunk export\n")
        f.write(f"# chunk={chunk_name} material={parsed['material']}\n")
        f.write("# header_words=" + ",".join(f"0x{x:04X}" for x in parsed["header_words"]) + "\n")
        f.write(f"o {safe_name(chunk_name)}_{safe_name(parsed['material'])}\n")
        for x, y, z in parsed["positions"]:
            f.write(f"v {x:.6f} {y:.6f} {z:.6f}\n")
        for u, v in parsed["uvs"]:
            f.write(f"vt {u:.6f} {v:.6f}\n")
        has_uv = len(parsed["uvs"]) == len(parsed["positions"])
        for a, b, c in parsed["triangles"]:
            if has_uv:
                f.write(f"f {a + 1}/{a + 1} {b + 1}/{b + 1} {c + 1}/{c + 1}\n")
            else:
                f.write(f"f {a + 1} {b + 1} {c + 1}\n")


def main():
    ap = argparse.ArgumentParser(description="Export currently understood FFXI 0x25 effect/environment chunks as OBJ previews.")
    ap.add_argument("dat", help="Path to a FFXI DAT file")
    ap.add_argument("--out-dir", default="tools/effect_chunk_exports")
    args = ap.parse_args()

    dat_path = Path(args.dat)
    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    data = dat_path.read_bytes()
    exported = 0
    for index, ofs, name, chunk_type, payload in iter_chunks(data):
        if chunk_type != 0x25:
            continue
        parsed = parse_type25(payload)
        if not parsed:
            print(f"skip chunk {index} {name} at 0x{ofs:X}: layout did not match current 0x25 hypothesis")
            continue
        out_path = out_dir / f"{safe_name(dat_path.stem)}_{index:03d}_{safe_name(name)}_{safe_name(parsed['material'])}.obj"
        write_obj(out_path, name, parsed)
        exported += 1
        print(
            f"exported {out_path} vertices={parsed['vertex_count']} "
            f"triangles={len(parsed['triangles'])} material={parsed['material']!r}"
        )

    if exported == 0:
        print("no exportable 0x25 chunks found")


if __name__ == "__main__":
    main()
