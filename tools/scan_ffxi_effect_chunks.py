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
    vertex_count = header_words[2]
    secondary_position_count = header_words[3]
    stored_index_ofs = header_words[4]
    triangle_count = header_words[5]
    pos_ofs = 0x20
    stored_attr_ofs = header_words[6]
    stored_uv_ofs = header_words[7]
    attr_ofs = stored_attr_ofs
    while attr_ofs < pos_ofs + vertex_count * 16:
        attr_ofs += 0x10000
    uv_ofs = stored_uv_ofs
    while uv_ofs < attr_ofs + triangle_count * 12:
        uv_ofs += 0x10000
    index_ofs = stored_index_ofs
    while index_ofs < uv_ofs + triangle_count * 24:
        index_ofs += 0x10000
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
        "secondary_position_count": secondary_position_count,
        "triangles": triangle_count,
        "bad_triangles": 0,
    }

    if vertex_count <= 0:
        info["reason"] = "zero_vertices"
        return info
    if not range_valid(pos_ofs, vertex_count * 16, len(payload)):
        info["reason"] = "bad_position_range"
        return info
    if triangle_count <= 0:
        info["reason"] = "zero_triangles"
        return info
    if not range_valid(attr_ofs, triangle_count * 12, len(payload)):
        info["reason"] = "bad_color_range"
        return info
    if not range_valid(uv_ofs, triangle_count * 24, len(payload)):
        info["reason"] = "bad_uv_range"
        return info
    if not range_valid(index_ofs, triangle_count * 12, len(payload)):
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
    if uv_ofs > 0 and range_valid(uv_ofs, triangle_count * 24, len(payload)):
        for i in range(triangle_count * 3):
            base = uv_ofs + i * 8
            uvs.append((read_f32(payload, base + 0), read_f32(payload, base + 4)))

    valid_triangles = 0
    for i in range(triangle_count):
        base = index_ofs + i * 6
        tri = (
            read_u16(payload, base + 0),
            read_u16(payload, base + 2),
            read_u16(payload, base + 4),
        )
        if max(tri) < vertex_count and len(set(tri)) == 3:
            valid_triangles += 1
        else:
            info["bad_triangles"] += 1

    info["valid"] = finite_positions and valid_triangles > 0 and info["bad_triangles"] == 0
    info["reason"] = "" if info["valid"] else "invalid_positions_or_indices"
    info["pos_bounds"] = bounds3(positions)
    info["uv_bounds"] = bounds2(uvs)
    return info


def parse_type21(payload):
    material = cstr(payload[0x08:0x18]) if len(payload) >= 0x18 else ""
    header_words = [read_u16(payload, i * 2) for i in range(min(12, len(payload) // 2))]
    if len(payload) < 0x1C:
        return {
            "material": material, "header_words": header_words, "first_ascii": cstr(payload[:64]),
            "valid": False, "reason": "short_payload", "card_count": 0, "layout": "",
        }

    extended_count = payload[2]
    card_count = payload[6]
    card_offsets = []
    if card_count <= 0:
        reason = "zero_cards"
    elif extended_count <= 1:
        card_offsets = [0x1C + card * 144 for card in range(card_count)]
        reason = ""
    elif card_count < extended_count:
        reason = "extended_count_exceeds_total"
    else:
        reason = ""
        cursor = 0x18
        for _ in range(extended_count):
            card_offsets.append(cursor + 20)
            cursor += 0xA4
        trailing_count = card_count - extended_count
        if trailing_count:
            trailing_bytes = trailing_count * 144
            extra_bytes = len(payload) - cursor - trailing_bytes
            if extra_bytes >= 4 and payload[cursor:cursor + 4] == b"\x01\x00\x01\x00":
                cursor += 4
            card_offsets.extend(cursor + card * 144 for card in range(trailing_count))

    valid = not reason and len(card_offsets) == card_count
    if valid:
        for card_ofs in card_offsets:
            if not range_valid(card_ofs, 144, len(payload)):
                valid = False
                reason = "bad_card_range"
                break
            for corner in range(6):
                xyz = struct.unpack_from("<3f", payload, card_ofs + corner * 24)
                if any(v != v or abs(v) > 1000000.0 for v in xyz):
                    valid = False
                    reason = "invalid_card_positions"
                    break
            if not valid:
                break

    layout = "flat" if extended_count <= 1 else "extended"
    if not valid:
        # Generator layout: byte 2 is a group count. Each four-byte group header
        # contains kind=1 and the number of contiguous 144-byte cards that follow.
        # The sum is authoritative; byte 6 is not a total in the large tam3 record.
        group_offsets = []
        cursor = 0x18
        grouped_valid = extended_count > 0
        for _ in range(extended_count):
            if not grouped_valid or not range_valid(cursor, 4, len(payload)):
                grouped_valid = False
                break
            group_kind = read_u16(payload, cursor)
            group_card_count = read_u16(payload, cursor + 2)
            cursor += 4
            if group_kind != 1 or not range_valid(cursor, group_card_count * 144, len(payload)):
                grouped_valid = False
                break
            for card in range(group_card_count):
                card_ofs = cursor + card * 144
                for corner in range(6):
                    xyz = struct.unpack_from("<3f", payload, card_ofs + corner * 24)
                    if any(v != v or abs(v) > 1000000.0 for v in xyz):
                        grouped_valid = False
                        break
                if not grouped_valid:
                    break
                group_offsets.append(card_ofs)
            cursor += group_card_count * 144
        grouped_valid = (grouped_valid and bool(group_offsets) and
                         (cursor + 15) & ~15 == len(payload))
        if grouped_valid:
            card_offsets = group_offsets
            card_count = len(card_offsets)
            valid = True
            reason = ""
            layout = "grouped"

    return {
        "material": material,
        "header_words": header_words,
        "first_ascii": cstr(payload[:64]),
        "valid": valid,
        "reason": reason,
        "card_count": card_count,
        "layout": layout,
    }


def parse_type1f(payload):
    marker = payload[0] if len(payload) >= 4 and payload[1:4] == b"\0\0\0" else -1
    material_ofs = 0x10 if marker == 3 else 0x0E
    material = cstr(payload[material_ofs:material_ofs + 16]) if len(payload) >= material_ofs + 16 else ""
    header_words = [read_u16(payload, i * 2) for i in range(min(8, len(payload) // 2))]
    triangle_count = read_u16(payload, 6) if len(payload) >= 8 else 0
    image_count = payload[4] if len(payload) >= 5 else 0
    vertex_ofs = 0x50 if marker == 3 else (0x0E + image_count * 16 + 15) & ~15
    valid = (marker in (3, 6) and
             triangle_count > 0 and range_valid(vertex_ofs, triangle_count * 3 * 36, len(payload)))
    return {
        "material": material,
        "header_words": header_words,
        "triangles": triangle_count,
        "valid": valid,
        "reason": "" if valid else "invalid_type1f_layout",
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
    valid1f = 0
    valid21 = 0

    for zone_id, zone_name, model_dat in parse_zone_table(zone_table):
        dat_path = ffxi_root / model_dat
        if not dat_path.is_file():
            continue
        data = dat_path.read_bytes()
        for chunk_index, chunk_ofs, chunk_name, chunk_type, payload, chunk_size in iter_chunks(data):
            if chunk_type not in (0x1F, 0x21, 0x25):
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
            elif chunk_type == 0x21:
                parsed = parse_type21(payload)
                if parsed["valid"]:
                    valid21 += 1
                common.update({
                    "material": parsed["material"],
                    "header_words": " ".join(f"{w:04X}" for w in parsed["header_words"]),
                    "first_ascii": parsed["first_ascii"],
                    "vertex_count": parsed["card_count"] * 6 if parsed["valid"] else "",
                    "triangles": parsed["card_count"] * 2 if parsed["valid"] else "",
                    "parse_reason": parsed["reason"],
                })
            else:
                parsed = parse_type1f(payload)
                if parsed["valid"]:
                    valid1f += 1
                common.update({
                    "material": parsed["material"],
                    "header_words": " ".join(f"{w:04X}" for w in parsed["header_words"]),
                    "triangles": parsed["triangles"],
                    "parse_reason": parsed["reason"],
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

    print(f"scanned={len(rows)} chunks; type1f={type_counts[0x1F]} valid1f={valid1f} "
          f"type21={type_counts[0x21]} valid21={valid21} "
          f"type25={type_counts[0x25]} valid25={valid25}")
    print(f"wrote {out_path}")


if __name__ == "__main__":
    main()
