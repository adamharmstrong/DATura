#!/usr/bin/env python3
import argparse
import csv
import re
import struct
from collections import Counter
from pathlib import Path


ZONE_ROW_RE = re.compile(
    r'\{\s*(\d+)\s*,\s*"([^"]*)"\s*,\s*"([^"]*)"\s*,',
    re.ASCII,
)


def cstr(raw):
    return raw.split(b"\0", 1)[0].decode("latin-1", "replace").rstrip()


def read_u32(data, ofs):
    return struct.unpack_from("<I", data, ofs)[0]


def read_u16(data, ofs):
    return struct.unpack_from("<H", data, ofs)[0]


def read_f32(data, ofs):
    return struct.unpack_from("<f", data, ofs)[0]


def parse_zone_table(path):
    zones = []
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        match = ZONE_ROW_RE.search(line)
        if not match:
            continue
        zone_id = int(match.group(1))
        name = match.group(2)
        model_dat = match.group(3)
        if model_dat:
            zones.append((zone_id, name, model_dat))
    return zones


def iter_chunks(data):
    ofs = 0
    index = 0
    while ofs <= len(data) - 16:
        info = read_u32(data, ofs + 4)
        chunk_type = info & 0x7F
        size = (info >> 3) & 0x7FFFF0
        if size <= 0 or ofs + size > len(data):
            break
        yield index, ofs, cstr(data[ofs:ofs + 4]), chunk_type, data[ofs + 16:ofs + size], size
        ofs += size
        index += 1


def range_valid(ofs, size, data_size):
    return ofs >= 0 and size >= 0 and ofs <= data_size and size <= data_size - ofs


def bounds3(values):
    if not values:
        return ("", "", "", "", "", "")
    xs, ys, zs = zip(*values)
    return (
        f"{min(xs):.6g}", f"{max(xs):.6g}",
        f"{min(ys):.6g}", f"{max(ys):.6g}",
        f"{min(zs):.6g}", f"{max(zs):.6g}",
    )


def bounds2(values):
    if not values:
        return ("", "", "", "")
    us, vs = zip(*values)
    return (
        f"{min(us):.6g}", f"{max(us):.6g}",
        f"{min(vs):.6g}", f"{max(vs):.6g}",
    )


def parse_type25(payload):
    if len(payload) < 0x20:
        return {"valid": False, "reason": "short_payload"}

    header_words = [read_u16(payload, i * 2) for i in range(8)]
    vertex_count = header_words[3]
    index_ofs = header_words[4]
    pos_ofs = header_words[5]
    attr_ofs = header_words[6]
    uv_ofs = header_words[7]
    material = cstr(payload[0x10:0x20])

    info = {
        "valid": False,
        "reason": "",
        "material": material,
        "header_words": header_words,
        "vertex_count": vertex_count,
        "index_ofs": index_ofs,
        "pos_ofs": pos_ofs,
        "attr_ofs": attr_ofs,
        "uv_ofs": uv_ofs,
        "triangles": 0,
        "bad_triangles": 0,
    }

    if vertex_count <= 0:
        info["reason"] = "zero_vertices"
        return info
    if not range_valid(pos_ofs, vertex_count * 16, len(payload)):
        info["reason"] = "bad_position_range"
        return info
    if not range_valid(index_ofs, vertex_count * 6, len(payload)):
        info["reason"] = "bad_index_range"
        return info

    positions = []
    finite_positions = True
    for i in range(vertex_count):
        base = pos_ofs + i * 16
        xyz = (
            read_f32(payload, base + 0),
            read_f32(payload, base + 4),
            read_f32(payload, base + 8),
        )
        if any(abs(v) > 1000000.0 or v != v for v in xyz):
            finite_positions = False
        positions.append(xyz)

    uvs = []
    if uv_ofs > 0 and range_valid(uv_ofs, vertex_count * 12, len(payload)):
        for i in range(vertex_count):
            base = uv_ofs + i * 12
            uvs.append((read_f32(payload, base + 0), read_f32(payload, base + 4)))

    for i in range(vertex_count):
        base = index_ofs + i * 6
        tri = (
            read_u16(payload, base + 0),
            read_u16(payload, base + 2),
            read_u16(payload, base + 4),
        )
        if max(tri) < vertex_count and len(set(tri)) == 3:
            info["triangles"] += 1
        else:
            info["bad_triangles"] += 1

    info["valid"] = finite_positions and info["triangles"] > 0
    info["reason"] = "" if info["valid"] else "invalid_positions_or_indices"
    info["pos_bounds"] = bounds3(positions)
    info["uv_bounds"] = bounds2(uvs)
    return info


def parse_type21(payload):
    material = cstr(payload[0x08:0x18]) if len(payload) >= 0x18 else ""
    header_words = [read_u16(payload, i * 2) for i in range(min(12, len(payload) // 2))]
    return {
        "material": material,
        "header_words": header_words,
        "first_ascii": cstr(payload[:64]),
    }


def main():
    ap = argparse.ArgumentParser(description="Catalog FFXI effect/environment chunks in known zone model DATs.")
    ap.add_argument("--ffxi-root", default=r"C:\Program Files (x86)\PlayOnline\SquareEnix\FINAL FANTASY XI")
    ap.add_argument("--zone-table", default="DATura/zone_dat_table.h")
    ap.add_argument("--out", default="tools/effect_chunk_scan.csv")
    args = ap.parse_args()

    ffxi_root = Path(args.ffxi_root)
    zone_table = Path(args.zone_table)
    rows = []
    type_counts = Counter()
    valid25 = 0

    for zone_id, zone_name, model_dat in parse_zone_table(zone_table):
        dat_path = ffxi_root / model_dat
        if not dat_path.is_file():
            continue
        data = dat_path.read_bytes()
        for chunk_index, chunk_ofs, chunk_name, chunk_type, payload, chunk_size in iter_chunks(data):
            if chunk_type not in (0x21, 0x25):
                continue
            type_counts[chunk_type] += 1
            common = {
                "zone_id": zone_id,
                "zone_name": zone_name,
                "model_dat": model_dat,
                "chunk_index": chunk_index,
                "chunk_offset": f"0x{chunk_ofs:X}",
                "chunk_name": chunk_name,
                "chunk_type": f"0x{chunk_type:02X}",
                "chunk_size": chunk_size,
                "payload_size": len(payload),
                "material": "",
                "valid_type25": "",
                "parse_reason": "",
                "header_words": "",
                "vertex_count": "",
                "triangles": "",
                "bad_triangles": "",
                "pos_ofs": "",
                "attr_ofs": "",
                "uv_ofs": "",
                "index_ofs": "",
                "pos_min_x": "",
                "pos_max_x": "",
                "pos_min_y": "",
                "pos_max_y": "",
                "pos_min_z": "",
                "pos_max_z": "",
                "uv_min_u": "",
                "uv_max_u": "",
                "uv_min_v": "",
                "uv_max_v": "",
                "first_ascii": "",
            }
            if chunk_type == 0x25:
                parsed = parse_type25(payload)
                if parsed["valid"]:
                    valid25 += 1
                common.update({
                    "material": parsed.get("material", ""),
                    "valid_type25": "yes" if parsed["valid"] else "no",
                    "parse_reason": parsed.get("reason", ""),
                    "header_words": " ".join(f"{w:04X}" for w in parsed.get("header_words", [])),
                    "vertex_count": parsed.get("vertex_count", ""),
                    "triangles": parsed.get("triangles", ""),
                    "bad_triangles": parsed.get("bad_triangles", ""),
                    "pos_ofs": parsed.get("pos_ofs", ""),
                    "attr_ofs": parsed.get("attr_ofs", ""),
                    "uv_ofs": parsed.get("uv_ofs", ""),
                    "index_ofs": parsed.get("index_ofs", ""),
                })
                pos_bounds = parsed.get("pos_bounds")
                if pos_bounds:
                    common.update(dict(zip(
                        ("pos_min_x", "pos_max_x", "pos_min_y", "pos_max_y", "pos_min_z", "pos_max_z"),
                        pos_bounds,
                    )))
                uv_bounds = parsed.get("uv_bounds")
                if uv_bounds:
                    common.update(dict(zip(
                        ("uv_min_u", "uv_max_u", "uv_min_v", "uv_max_v"),
                        uv_bounds,
                    )))
            else:
                parsed = parse_type21(payload)
                common.update({
                    "material": parsed["material"],
                    "header_words": " ".join(f"{w:04X}" for w in parsed["header_words"]),
                    "first_ascii": parsed["first_ascii"],
                })
            rows.append(common)

    out_path = Path(args.out)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    fieldnames = list(rows[0].keys()) if rows else [
        "zone_id", "zone_name", "model_dat", "chunk_index", "chunk_offset", "chunk_name", "chunk_type",
    ]
    with out_path.open("w", encoding="utf-8", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)

    print(f"scanned={len(rows)} chunks; type21={type_counts[0x21]} type25={type_counts[0x25]} valid25={valid25}")
    print(f"wrote {out_path}")


if __name__ == "__main__":
    main()
