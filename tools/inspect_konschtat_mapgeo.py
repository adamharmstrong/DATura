import argparse
import csv
import math
import re
import struct
from pathlib import Path
from PIL import Image


CHUNK_TEXTURE = 0x20
CHUNK_MAP = 0x1C
CHUNK_MAPGEO = 0x2E
CHUNK_HEADER_SIZE = 16
NAME_LEN = 16


def parse_key_table(header_text, table_name):
    match = re.search(rf"{table_name}\[256\]\s*=\s*\{{(.*?)\}};", header_text, re.S)
    if not match:
        raise RuntimeError(f"could not find {table_name}")
    return [int(x, 16) for x in re.findall(r"0x[0-9A-Fa-f]{2}", match.group(1))]


SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parent
DECRYPT_HEADER = REPO_ROOT / "DATura" / "model_ff11_decrypt.h"
_header = DECRYPT_HEADER.read_text(encoding="utf-8", errors="ignore")
KEY_TABLE = parse_key_table(_header, "skKeyTable")
KEY_TABLE2 = parse_key_table(_header, "skKeyTable2")


def c_name(raw):
    raw = raw.split(b"\0", 1)[0]
    return "".join(chr(b) if 32 <= b < 127 else "?" for b in raw)


def u24le(data, off):
    return data[off] | (data[off + 1] << 8) | (data[off + 2] << 16)


def decrypt_object_map(buf):
    if len(buf) < 8 or buf[3] < 0x1B:
        return buf
    out = bytearray(buf)
    decode_len = min(u24le(out, 0), len(out))
    key = KEY_TABLE[out[7] ^ 0xFF]
    key_counter = 0
    pos = 8
    while pos < decode_len:
        xor_len = ((key >> 4) & 7) + 16
        if (key & 1) and (pos + xor_len < decode_len):
            for i in range(xor_len):
                out[pos + i] ^= 0xFF
        key_counter += 1
        key += key_counter
        pos += xor_len

    node_count = u24le(out, 4)
    node_stride = 100
    max_nodes = max(0, (len(out) - 32) // node_stride)
    for i in range(min(node_count, max_nodes)):
        off = 32 + i * node_stride
        for j in range(16):
            out[off + j] ^= 0x55
    return bytes(out)


def decrypt_mmb2(out):
    if len(out) < 8 or out[6] != 0xFF or out[7] != 0xFF:
        return
    decode_len = min(u24le(out, 0), len(out))
    key1 = out[5] ^ 0xF0
    key2 = KEY_TABLE2[key1]
    decode_count = ((decode_len - 8) & ~0xF) // 2
    p1 = 8
    p2 = 8 + decode_count
    pos = 0
    while pos < decode_count:
        if key2 & 1:
            for word in range(2):
                a = p1 + word * 4
                b = p2 + word * 4
                out[a:a + 4], out[b:b + 4] = out[b:b + 4], out[a:a + 4]
        key1 += 9
        key2 += key1
        p1 += 8
        p2 += 8
        pos += 8


def decrypt_mmb(buf):
    out = bytearray(buf)
    if len(out) >= 8 and out[3] >= 5:
        decode_len = min(u24le(out, 0), len(out))
        key = KEY_TABLE[out[5] ^ 0xF0]
        key_counter = 0
        for pos in range(8, decode_len):
            x = ((key & 0xFF) << 8) | (key & 0xFF)
            key_counter += 1
            key += key_counter
            out[pos] ^= (x >> (key & 7)) & 0xFF
            key_counter += 1
            key += key_counter
    decrypt_mmb2(out)
    return bytes(out)


def decrypt_chunk(chunk_type, payload):
    if chunk_type == CHUNK_MAP:
        return decrypt_object_map(payload)
    if chunk_type in (CHUNK_MAPGEO,):
        return decrypt_mmb(payload)
    return payload


def parse_chunks(data):
    off = 0
    while off <= len(data) - CHUNK_HEADER_SIZE:
        name = data[off:off + 4]
        info = struct.unpack_from("<I", data, off + 4)[0]
        chunk_type = info & 0x7F
        chunk_size = (info >> 3) & 0x7FFFF0
        if chunk_size <= 0 or off + chunk_size > len(data):
            break
        yield {
            "offset": off,
            "name": c_name(name),
            "type": chunk_type,
            "size": chunk_size,
            "payload": data[off + CHUNK_HEADER_SIZE:off + chunk_size],
        }
        off += chunk_size


def iter_mapgeo_segments(payload):
    if len(payload) < 32:
        return
    object_name = c_name(payload[16:32])
    draw_off = 32
    end_draw = len(payload) - 32
    super_index = 0
    while draw_off <= end_draw:
        if draw_off + 32 > len(payload):
            break
        super_count = struct.unpack_from("<i", payload, draw_off)[0]
        draw_off += 32
        if super_count < 0 or super_count > 4096:
            break
        for si in range(super_count):
            if draw_off + 32 > len(payload):
                return
            sub_count = struct.unpack_from("<i", payload, draw_off)[0]
            sub_flag = struct.unpack_from("<i", payload, draw_off + 28)[0]
            draw_off += 32
            if sub_count < 0 or sub_count > 4096:
                return
            for sub_i in range(sub_count):
                if draw_off % 4:
                    return
                if draw_off + NAME_LEN + 4 > len(payload):
                    return
                mat_raw = payload[draw_off:draw_off + NAME_LEN]
                mat_name = c_name(mat_raw)
                draw_off += NAME_LEN
                vert_count, blend_flags = struct.unpack_from("<HH", payload, draw_off)
                draw_off += 4
                vert_stride = 48 if sub_flag == 0 else 36
                vert_data_off = draw_off
                vert_data_len = vert_stride * vert_count
                draw_off += vert_data_len
                if draw_off + 4 > len(payload):
                    return
                index_count, flags2 = struct.unpack_from("<HH", payload, draw_off)
                draw_off += 4
                index_data_off = draw_off
                indices = list(struct.unpack_from(f"<{index_count}H", payload, index_data_off)) if index_count else []
                draw_off += index_count * 2
                draw_off = (draw_off + 3) & ~3

                alphas = []
                colors = []
                us = []
                vs = []
                xs = []
                ys = []
                zs = []
                radial = []
                elevations = []
                clr_off = 36 if vert_stride == 48 else 24
                uv_off = 40 if vert_stride == 48 else 28
                for vi in range(vert_count):
                    vo = vert_data_off + vi * vert_stride
                    if vo + vert_stride > len(payload):
                        break
                    x, y, z = struct.unpack_from("<fff", payload, vo)
                    u, v = struct.unpack_from("<ff", payload, vo + uv_off)
                    color = payload[vo + clr_off:vo + clr_off + 4]
                    xs.append(x)
                    ys.append(y)
                    zs.append(z)
                    horizontal_radius = math.hypot(x, z)
                    radial.append(horizontal_radius)
                    # MapGeo weather caps use FFXI's native Y-down coordinates,
                    # so negative Y is physical elevation above the horizon.
                    elevations.append(math.degrees(math.atan2(-y, horizontal_radius)))
                    us.append(u)
                    vs.append(v)
                    colors.append(tuple(color))
                    alphas.append(color[3])

                triangle_count = 0
                local_valid_triangle_count = 0
                max_triangle_edge = 0.0
                coincident_uv_seam_count = 0
                coincident_uv_seam_samples = []
                coincident_vertices = {}
                for vertex_index, position in enumerate(zip(xs, ys, zs)):
                    position_key = tuple(round(component, 5) for component in position)
                    coincident_vertices.setdefault(position_key, []).append(vertex_index)
                for position_key, vertex_indices in coincident_vertices.items():
                    uv_values = {(round(us[index], 6), round(vs[index], 6)) for index in vertex_indices}
                    if len(uv_values) <= 1:
                        continue
                    coincident_uv_seam_count += 1
                    if len(coincident_uv_seam_samples) < 8:
                        coincident_uv_seam_samples.append((position_key, vertex_indices, sorted(uv_values)))
                u_wrap_triangle_count = 0
                max_triangle_u_span = 0.0
                u_wrap_samples = []
                v_wrap_triangle_count = 0
                max_triangle_v_span = 0.0
                v_wrap_samples = []
                geometric_triangles = {}
                geometric_edges = {}
                triangle_offsets = range(0, max(0, index_count - 2), 3) if sub_flag == 0 else range(max(0, index_count - 2))
                for triangle_offset in triangle_offsets:
                    tri = indices[triangle_offset:triangle_offset + 3]
                    if len(tri) < 3 or len(set(tri)) < 3 or max(tri) >= len(xs):
                        continue
                    triangle_count += 1
                    triangle_us = [us[index] for index in tri]
                    triangle_u_span = max(triangle_us) - min(triangle_us)
                    max_triangle_u_span = max(max_triangle_u_span, triangle_u_span)
                    if triangle_u_span > 0.5:
                        u_wrap_triangle_count += 1
                        if len(u_wrap_samples) < 8:
                            u_wrap_samples.append((triangle_offset, tri, triangle_us))
                    triangle_vs = [vs[index] for index in tri]
                    triangle_v_span = max(triangle_vs) - min(triangle_vs)
                    max_triangle_v_span = max(max_triangle_v_span, triangle_v_span)
                    if triangle_v_span > 0.5:
                        v_wrap_triangle_count += 1
                        if len(v_wrap_samples) < 8:
                            v_wrap_samples.append((triangle_offset, tri, triangle_vs))
                    edge_lengths = []
                    for edge in range(3):
                        a = tri[edge]
                        b = tri[(edge + 1) % 3]
                        edge_lengths.append(math.sqrt(
                            (xs[a] - xs[b]) ** 2 +
                            (ys[a] - ys[b]) ** 2 +
                            (zs[a] - zs[b]) ** 2
                        ))
                    triangle_max_edge = max(edge_lengths)
                    max_triangle_edge = max(max_triangle_edge, triangle_max_edge)
                    if triangle_max_edge <= 80.0:
                        local_valid_triangle_count += 1
                    position_keys = [
                        tuple(round(component, 5) for component in (xs[index], ys[index], zs[index]))
                        for index in tri
                    ]
                    triangle_key = tuple(sorted(position_keys))
                    geometric_triangles.setdefault(triangle_key, []).append(triangle_offset)
                    for edge in range(3):
                        edge_key = tuple(sorted((position_keys[edge], position_keys[(edge + 1) % 3])))
                        geometric_edges[edge_key] = geometric_edges.get(edge_key, 0) + 1

                duplicate_geometric_triangles = {
                    triangle: offsets for triangle, offsets in geometric_triangles.items()
                    if len(offsets) > 1
                }
                boundary_geometric_edges = [
                    edge for edge, count in geometric_edges.items() if count == 1
                ]

                yield {
                    "object_name": object_name,
                    "super_index": super_index,
                    "sub_index": sub_i,
                    "sub_flag": sub_flag,
                    "material": mat_name,
                    "vert_count": vert_count,
                    "index_count": index_count,
                    "blend_flags": blend_flags,
                    "flags2": flags2,
                    "vert_stride": vert_stride,
                    "alpha_min": min(alphas) if alphas else None,
                    "alpha_max": max(alphas) if alphas else None,
                    "alpha_unique": len(set(alphas)),
                    "color_samples": sorted(set(colors))[:8],
                    "uv_bounds": (
                        min(us) if us else None,
                        min(vs) if vs else None,
                        max(us) if us else None,
                        max(vs) if vs else None,
                    ),
                    "bounds": (
                        min(xs) if xs else None,
                        min(ys) if ys else None,
                        min(zs) if zs else None,
                        max(xs) if xs else None,
                        max(ys) if ys else None,
                        max(zs) if zs else None,
                    ),
                    "radial_bounds": (
                        min(radial) if radial else None,
                        max(radial) if radial else None,
                    ),
                    "elevation_bounds": (
                        min(elevations) if elevations else None,
                        max(elevations) if elevations else None,
                    ),
                    "triangle_count": triangle_count,
                    "local_valid_triangle_count": local_valid_triangle_count,
                    "max_triangle_edge": max_triangle_edge,
                    "coincident_uv_seam_count": coincident_uv_seam_count,
                    "coincident_uv_seam_samples": coincident_uv_seam_samples,
                    "u_wrap_triangle_count": u_wrap_triangle_count,
                    "max_triangle_u_span": max_triangle_u_span,
                    "u_wrap_samples": u_wrap_samples,
                    "v_wrap_triangle_count": v_wrap_triangle_count,
                    "max_triangle_v_span": max_triangle_v_span,
                    "v_wrap_samples": v_wrap_samples,
                    "duplicate_geometric_triangle_count": len(duplicate_geometric_triangles),
                    "duplicate_geometric_triangle_samples": list(duplicate_geometric_triangles.items())[:8],
                    "boundary_geometric_edge_count": len(boundary_geometric_edges),
                    "boundary_geometric_edge_samples": boundary_geometric_edges[:8],
                }
            super_index += 1


def parse_textures(chunks):
    rows = []
    for chunk in chunks:
        if chunk["type"] != CHUNK_TEXTURE:
            continue
        payload = chunk["payload"]
        if len(payload) < 56:
            continue
        tex_type = payload[0]
        name = c_name(payload[1:17])
        width, height = struct.unpack_from("<ii", payload, 21)
        bpc = struct.unpack_from("<I", payload, 53)[0]
        src = payload[57:]
        codec = ""
        alpha_stats = {}
        if tex_type in (0xA1, 0x81) and len(src) >= 4:
            if tex_type == 0x81:
                safe_bpc = bpc if bpc in (16, 32) else 32
                pal_size = 256 * (safe_bpc // 8)
                dxt_off = pal_size + max(0, width) * max(0, height)
                codec = c_name(src[dxt_off:dxt_off + 4]) if dxt_off + 4 <= len(src) else ""
                dxt_payload = src[dxt_off + 12:]
            else:
                codec = c_name(src[:4])
                dxt_payload = src[12:]
            alpha_stats = dxt_alpha_stats(codec, width, height, dxt_payload)
        rows.append({
            "chunk_offset": chunk["offset"],
            "name": name,
            "type": f"0x{tex_type:02X}",
            "width": width,
            "height": height,
            "bits_per_pal_color": bpc,
            "codec": codec,
            **alpha_stats,
        })
    return rows


def dxt_alpha_stats(codec, width, height, data):
    if width <= 0 or height <= 0 or not data:
        return {}
    codec_norm = codec[::-1]
    block_cols = (width + 3) // 4
    block_rows = (height + 3) // 4
    alphas = []
    transparent_codes = 0
    block_size = 8 if codec_norm == "DXT1" else 16
    needed = block_cols * block_rows * block_size
    if len(data) < needed:
        return {"alpha_note": f"short data {len(data)}/{needed}"}
    off = 0
    for by in range(block_rows):
        for bx in range(block_cols):
            block = data[off:off + block_size]
            off += block_size
            if codec_norm == "DXT1":
                c0, c1, bits = struct.unpack_from("<HHI", block, 0)
                for i in range(16):
                    code = (bits >> (i * 2)) & 3
                    a = 0 if c0 <= c1 and code == 3 else 255
                    alphas.append(a)
                    if a == 0:
                        transparent_codes += 1
            elif codec_norm == "DXT3":
                for row in range(4):
                    packed = struct.unpack_from("<H", block, row * 2)[0]
                    for px in range(4):
                        alphas.append(((packed >> (px * 4)) & 0xF) * 17)
            elif codec_norm == "DXT5":
                a0 = block[0]
                a1 = block[1]
                table = [a0, a1]
                if a0 > a1:
                    table += [(a0 * (8 - i) + a1 * (i - 1)) // 7 for i in range(2, 8)]
                else:
                    table += [(a0 * (6 - i) + a1 * (i - 1)) // 5 for i in range(2, 6)]
                    table += [0, 255]
                bits = 0
                for i in range(6):
                    bits |= block[2 + i] << (i * 8)
                for i in range(16):
                    alphas.append(table[(bits >> (i * 3)) & 7])
    if not alphas:
        return {}
    return {
        "codec_norm": codec_norm,
        "alpha_min": min(alphas),
        "alpha_max": max(alphas),
        "alpha_unique": len(set(alphas)),
        "alpha_zero": sum(1 for a in alphas if a == 0),
        "alpha_low_1_127": sum(1 for a in alphas if 0 < a < 128),
        "alpha_mid_128_254": sum(1 for a in alphas if 128 <= a < 255),
        "alpha_opaque": sum(1 for a in alphas if a == 255),
        "dxt1_transparent_codes": transparent_codes,
    }


def rgb565(raw):
    r = ((raw >> 11) & 0x1F) * 255 // 31
    g = ((raw >> 5) & 0x3F) * 255 // 63
    b = (raw & 0x1F) * 255 // 31
    return r, g, b


def decode_dxt(codec, width, height, data):
    codec_norm = codec[::-1]
    block_cols = (width + 3) // 4
    block_rows = (height + 3) // 4
    block_size = 8 if codec_norm == "DXT1" else 16
    pixels = [(0, 0, 0, 0)] * (width * height)
    off = 0
    for by in range(block_rows):
        for bx in range(block_cols):
            block = data[off:off + block_size]
            off += block_size
            if len(block) < block_size:
                break

            alpha = [255] * 16
            color_off = 0
            if codec_norm == "DXT3":
                for row in range(4):
                    packed = struct.unpack_from("<H", block, row * 2)[0]
                    for px in range(4):
                        alpha[row * 4 + px] = ((packed >> (px * 4)) & 0xF) * 17
                color_off = 8
            elif codec_norm == "DXT5":
                a0 = block[0]
                a1 = block[1]
                table = [a0, a1]
                if a0 > a1:
                    table += [(a0 * (8 - i) + a1 * (i - 1)) // 7 for i in range(2, 8)]
                else:
                    table += [(a0 * (6 - i) + a1 * (i - 1)) // 5 for i in range(2, 6)]
                    table += [0, 255]
                bits = 0
                for i in range(6):
                    bits |= block[2 + i] << (i * 8)
                for i in range(16):
                    alpha[i] = table[(bits >> (i * 3)) & 7]
                color_off = 8

            c0, c1, bits = struct.unpack_from("<HHI", block, color_off)
            c0rgb = rgb565(c0)
            c1rgb = rgb565(c1)
            colors = [c0rgb, c1rgb]
            if codec_norm == "DXT1" and c0 <= c1:
                colors.append(tuple((c0rgb[i] + c1rgb[i]) // 2 for i in range(3)))
                colors.append((0, 0, 0))
            else:
                colors.append(tuple((2 * c0rgb[i] + c1rgb[i]) // 3 for i in range(3)))
                colors.append(tuple((c0rgb[i] + 2 * c1rgb[i]) // 3 for i in range(3)))

            for py in range(4):
                for px in range(4):
                    x = bx * 4 + px
                    y = by * 4 + py
                    if x >= width or y >= height:
                        continue
                    idx = py * 4 + px
                    code = (bits >> (idx * 2)) & 3
                    a = 0 if codec_norm == "DXT1" and c0 <= c1 and code == 3 else alpha[idx]
                    r, g, b = colors[code]
                    pixels[y * width + x] = (r, g, b, a)
    img = Image.new("RGBA", (width, height))
    img.putdata(pixels)
    return img


def save_texture_previews(texture_rows, chunks, out_dir, wanted_names):
    out_dir.mkdir(parents=True, exist_ok=True)
    chunk_by_offset = {chunk["offset"]: chunk for chunk in chunks}
    for row in texture_rows:
        if not any(wanted in row["name"] for wanted in wanted_names):
            continue
        chunk = chunk_by_offset.get(row["chunk_offset"])
        if not chunk:
            continue
        payload = chunk["payload"]
        width = int(row["width"])
        height = int(row["height"])
        src = payload[57:]
        dxt_data = src[12:]
        img = decode_dxt(row["codec"], width, height, dxt_data)
        safe = row["name"].strip().replace(" ", "_")
        img.save(out_dir / f"{safe}_rgba.png")
        alpha = Image.new("L", img.size)
        alpha.putdata([p[3] for p in img.getdata()])
        alpha.save(out_dir / f"{safe}_alpha.png")

        checker = Image.new("RGBA", img.size)
        cp = []
        for y in range(height):
            for x in range(width):
                v = 192 if ((x // 8) + (y // 8)) % 2 else 64
                cp.append((v, v, v, 255))
        checker.putdata(cp)
        comp = Image.alpha_composite(checker, img)
        comp.save(out_dir / f"{safe}_checker.png")


def load_catalog_names(path, categories):
    if not path or not path.exists():
        return set()
    names = set()
    with path.open(newline="", encoding="utf-8", errors="replace") as f:
        for row in csv.DictReader(f):
            if row.get("Category") in categories:
                names.add(row.get("Name", ""))
    return names


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--dat", default=r"C:\Program Files (x86)\PlayOnline\SquareEnix\FINAL FANTASY XI\ROM\0\90.DAT")
    parser.add_argument("--filter", default="_con_hana_,_kusa_,_tyo,con_,_con_,pl_flb")
    parser.add_argument("--out", default=str(REPO_ROOT / "tools" / "konschtat_mapgeo_inspection.csv"))
    args = parser.parse_args()

    data = Path(args.dat).read_bytes()
    chunks = list(parse_chunks(data))
    filters = [x for x in args.filter.split(",") if x]

    rows = []
    for chunk in chunks:
        if chunk["type"] != CHUNK_MAPGEO:
            continue
        payload = decrypt_chunk(chunk["type"], chunk["payload"])
        object_name = c_name(payload[16:32]) if len(payload) >= 32 else ""
        if filters and not any(f in object_name for f in filters):
            continue
        for seg in iter_mapgeo_segments(payload) or []:
            if filters and not any(f in seg["object_name"] for f in filters):
                continue
            row = {
                "chunk_offset": chunk["offset"],
                "chunk_size": chunk["size"],
                **seg,
            }
            rows.append(row)

    out = Path(args.out)
    with out.open("w", newline="", encoding="utf-8") as f:
        fieldnames = [
            "chunk_offset", "chunk_size", "object_name", "super_index", "sub_index",
            "material", "sub_flag", "vert_stride", "vert_count", "index_count",
			"blend_flags", "flags2", "alpha_min", "alpha_max", "alpha_unique",
			"color_samples", "uv_bounds", "bounds", "radial_bounds", "elevation_bounds",
			"triangle_count", "local_valid_triangle_count", "max_triangle_edge",
			"coincident_uv_seam_count", "coincident_uv_seam_samples",
			"u_wrap_triangle_count", "max_triangle_u_span", "u_wrap_samples",
			"v_wrap_triangle_count", "max_triangle_v_span", "v_wrap_samples",
			"duplicate_geometric_triangle_count", "duplicate_geometric_triangle_samples",
			"boundary_geometric_edge_count", "boundary_geometric_edge_samples",
		]
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)

    tex_rows = parse_textures(chunks)
    tex_out = out.with_name(out.stem + "_textures.csv")
    with tex_out.open("w", newline="", encoding="utf-8") as f:
        fieldnames = [
            "chunk_offset", "name", "type", "width", "height", "bits_per_pal_color",
            "codec", "codec_norm", "alpha_min", "alpha_max", "alpha_unique",
            "alpha_zero", "alpha_low_1_127", "alpha_mid_128_254", "alpha_opaque",
            "dxt1_transparent_codes", "alpha_note",
        ]
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(tex_rows)

    save_texture_previews(
        tex_rows,
        chunks,
        REPO_ROOT / "tools" / "konschtat_texture_preview",
        {"con_f01c", "kusa_02c", "con_w03c"},
    )

    print(f"chunks={len(chunks)} mapgeo_rows={len(rows)} textures={len(tex_rows)}")
    print(f"wrote {out}")
    print(f"wrote {tex_out}")
    for row in rows[:40]:
        print(
            f"{row['chunk_offset']:>8} {row['object_name']:<16} mat={row['material']:<16} "
            f"blend=0x{row['blend_flags']:04X} flag2=0x{row['flags2']:04X} "
            f"stride={row['vert_stride']} a={row['alpha_min']}..{row['alpha_max']} "
            f"vc={row['vert_count']} ic={row['index_count']}"
        )


if __name__ == "__main__":
    main()
