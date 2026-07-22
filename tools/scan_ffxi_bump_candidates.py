#!/usr/bin/env python3
import argparse
import csv
import re
import struct
from collections import Counter, defaultdict
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
DECRYPT_HEADER = REPO_ROOT / "DATura" / "model_ff11_decrypt.h"
ZONE_TABLE = REPO_ROOT / "DATura" / "zone_dat_table.h"

CHUNK_TEXTURE = 0x20
CHUNK_MAP = 0x1C
CHUNK_MAPGEO = 0x2E
NAME_LEN = 16


def cstr(raw):
    raw = raw.split(b"\0", 1)[0]
    return "".join(chr(b) if 32 <= b < 127 else "?" for b in raw)


def read_u16(data, ofs):
    return struct.unpack_from("<H", data, ofs)[0]


def read_u32(data, ofs):
    return struct.unpack_from("<I", data, ofs)[0]


def read_i32(data, ofs):
    return struct.unpack_from("<i", data, ofs)[0]


def load_key_table(name):
    text = DECRYPT_HEADER.read_text(encoding="utf-8")
    match = re.search(rf"{name}\[256\]\s*=\s*\{{(.*?)\}};", text, re.S)
    if not match:
        raise RuntimeError(f"missing {name}")
    return [int(x, 16) for x in re.findall(r"0x[0-9A-Fa-f]{2}", match.group(1))]


KEY_TABLE = load_key_table("skKeyTable")
KEY_TABLE2 = load_key_table("skKeyTable2")


def u24le(data, ofs):
    return data[ofs] | (data[ofs + 1] << 8) | (data[ofs + 2] << 16)


def decode_mmb2(buf):
    if len(buf) < 8 or buf[6] != 0xFF or buf[7] != 0xFF:
        return
    decode_len = min(u24le(buf, 0), len(buf))
    decode_count = ((decode_len - 8) & ~0xF) // 2
    key1 = buf[5] ^ 0xF0
    key2 = KEY_TABLE2[key1]
    p1 = 8
    p2 = 8 + decode_count
    for _ in range(0, decode_count, 8):
        if key2 & 1:
            buf[p1:p1 + 8], buf[p2:p2 + 8] = buf[p2:p2 + 8], buf[p1:p1 + 8]
        key1 = (key1 + 9) & 0xFFFFFFFF
        key2 = (key2 + key1) & 0xFFFFFFFF
        p1 += 8
        p2 += 8


def decode_mmb(payload):
    out = bytearray(payload)
    if len(out) >= 8 and out[3] >= 5:
        decode_len = min(u24le(out, 0), len(out))
        key = KEY_TABLE[out[5] ^ 0xF0]
        key_counter = 0
        for pos in range(8, decode_len):
            x = ((key & 0xFF) << 8) | (key & 0xFF)
            key_counter += 1
            key = (key + key_counter) & 0xFFFFFFFF
            out[pos] ^= (x >> (key & 7)) & 0xFF
            key_counter += 1
            key = (key + key_counter) & 0xFFFFFFFF
    decode_mmb2(out)
    return bytes(out)


def decode_map(payload):
    out = bytearray(payload)
    if len(out) < 8 or out[3] < 0x1B:
        return bytes(out)
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
        key = (key + key_counter) & 0xFFFFFFFF
        pos += xor_len

    node_count = u24le(out, 4)
    for i in range(min(node_count, max(0, (len(out) - 32) // 96))):
        off = 32 + i * 96
        for j in range(16):
            out[off + j] ^= 0x55
    return bytes(out)


def decrypt_chunk(chunk_type, payload):
    if chunk_type == CHUNK_MAP:
        return decode_map(payload)
    if chunk_type in (0x20, 0x29, 0x2A, 0x2B, CHUNK_MAPGEO):
        return decode_mmb(payload)
    return payload


def iter_chunks(data):
    off = 0
    index = 0
    while off <= len(data) - 16:
        info = read_u32(data, off + 4)
        chunk_type = info & 0x7F
        size = (info >> 3) & 0x7FFFF0
        if size <= 0 or off + size > len(data):
            break
        yield {
            "index": index,
            "offset": off,
            "type": chunk_type,
            "size": size,
            "payload": data[off + 16:off + size],
            "chunk_name": cstr(data[off:off + 4]),
        }
        off += size
        index += 1


def parse_texture(chunk):
    payloads = [chunk["payload"]]
    dec_payload = decrypt_chunk(chunk["type"], chunk["payload"])
    if dec_payload != chunk["payload"]:
        payloads.append(dec_payload)
    # Texture headers in current zone/model DATs use version 40 at offset 0x11.
    for payload in payloads:
        if len(payload) < 57:
            continue
        candidates = [0]
        candidates.extend(range(1, min(len(payload) - 57, 128)))
        for h in candidates:
            for ver_ofs in (17, 20):
                if h + ver_ofs + 40 > len(payload) or read_u32(payload, h + ver_ofs) != 40:
                    continue
                width = read_i32(payload, h + ver_ofs + 4)
                height = read_i32(payload, h + ver_ofs + 8)
                if not (0 < width <= 4096 and 0 < height <= 4096):
                    continue
                tex_type = payload[h]
                if tex_type not in (0x01, 0x81, 0x91, 0xA1, 0xB1):
                    continue
                src = payload[h + 57:]
                codec = ""
                extra = ""
                if tex_type == 0x81:
                    bpc = read_u32(payload, h + ver_ofs + 36)
                    safe_bpc = bpc if bpc in (16, 32) else 32
                    pal_size = 256 * (safe_bpc // 8)
                    dxt_off = pal_size + width * height
                    codec = cstr(src[dxt_off:dxt_off + 4]) if dxt_off + 4 <= len(src) else ""
                elif tex_type == 0xA1:
                    codec = cstr(src[:4])
                elif tex_type == 0xB1 and len(src) >= 16:
                    extra = f"leading_u32=0x{read_u32(src, 0):08X}"
                return {
                    "name": cstr(payload[h + 1:h + 17]),
                    "type": tex_type,
                    "width": width,
                    "height": height,
                    "codec": codec,
                    "extra": extra,
                    "chunk_name": chunk["chunk_name"],
                    "chunk_offset": chunk["offset"],
                }
    return None


def iter_mapgeo_segments(chunk):
    payload = decrypt_chunk(chunk["type"], chunk["payload"])
    if len(payload) < 64:
        return
    object_name = cstr(payload[16:32])
    unknown_name = cstr(payload[8:16])
    unknown1 = read_u32(payload, 4)
    draw_off = 32
    end_draw = len(payload) - 32
    super_index = 0
    while draw_off <= end_draw:
        if draw_off + 32 > len(payload):
            return
        super_count = read_i32(payload, draw_off)
        super_flag = read_i32(payload, draw_off + 28)
        draw_off += 32
        if super_count < 0 or super_count > 4096:
            return
        for si in range(super_count):
            if draw_off + 32 > len(payload):
                return
            sub_count = read_i32(payload, draw_off)
            sub_flag = read_i32(payload, draw_off + 28)
            draw_off += 32
            if sub_count < 0 or sub_count > 4096:
                return
            for sub_i in range(sub_count):
                if draw_off + NAME_LEN + 4 > len(payload):
                    return
                material = cstr(payload[draw_off:draw_off + NAME_LEN])
                draw_off += NAME_LEN
                vert_count = read_u16(payload, draw_off)
                blend_flags = read_u16(payload, draw_off + 2)
                draw_off += 4
                vert_stride = 48 if sub_flag == 0 else 36
                vert_bytes = vert_stride * vert_count
                if draw_off + vert_bytes + 4 > len(payload):
                    return
                draw_off += vert_bytes
                index_count = read_u16(payload, draw_off)
                flags2 = read_u16(payload, draw_off + 2)
                draw_off += 4
                if draw_off + index_count * 2 > len(payload):
                    return
                draw_off += index_count * 2
                draw_off = (draw_off + 3) & ~3
                yield {
                    "object_name": object_name,
                    "unknown_name": unknown_name,
                    "unknown1": unknown1,
                    "super_index": super_index,
                    "sub_index": sub_i,
                    "material": material,
                    "super_flag": super_flag,
                    "sub_flag": sub_flag,
                    "blend_flags": blend_flags,
                    "flags2": flags2,
                    "vert_stride": vert_stride,
                    "vert_count": vert_count,
                    "index_count": index_count,
                }
        super_index += 1


def clean_name(name):
    return "".join(ch for ch in name.lower() if ch.isalnum())


def prefix_key(name):
    s = clean_name(name)
    # Strip common texture namespace words before deriving a fuzzy key.
    for prefix in ("model", "effect", "menu"):
        if s.startswith(prefix):
            s = s[len(prefix):]
    return s[:-1] if len(s) > 2 and s[-1].isalpha() else s


def load_zones():
    text = ZONE_TABLE.read_text(encoding="utf-8", errors="ignore")
    zone_re = re.compile(r'\{\s*(\d+),\s*"([^"]*)",\s*"([^"]*)"', re.S)
    for m in zone_re.finditer(text):
        yield int(m.group(1)), m.group(2), m.group(3)


def selected_zones(names):
    wanted = [n.lower() for n in names]
    if wanted == ["all"]:
        wanted = []
    for zone_id, zone_name, model_dat in load_zones():
        if not model_dat:
            continue
        if not wanted or any(w in zone_name.lower() for w in wanted):
            yield zone_id, zone_name, model_dat


def scan_dat(ffxi_root, zone_id, zone_name, rel_path):
    path = ffxi_root / rel_path.replace("/", "\\")
    data = path.read_bytes()
    chunks = list(iter_chunks(data))
    textures = [parse_texture(ch) for ch in chunks if ch["type"] == CHUNK_TEXTURE]
    textures = [t for t in textures if t]
    segments = []
    for ch in chunks:
        if ch["type"] == CHUNK_MAPGEO:
            segments.extend(iter_mapgeo_segments(ch) or [])
    referenced = Counter(seg["material"] for seg in segments)
    tex_names = {tex["name"] for tex in textures}
    companion_groups = defaultdict(list)
    for tex in textures:
        companion_groups[prefix_key(tex["name"])].append(tex["name"])
    companions = {
        key: sorted(set(vals))
        for key, vals in companion_groups.items()
        if len(set(vals)) > 1
    }
    return {
        "zone_id": zone_id,
        "zone_name": zone_name,
        "rel_path": rel_path,
        "path": path,
        "chunks": chunks,
        "textures": textures,
        "segments": segments,
        "referenced": referenced,
        "unreferenced_textures": sorted(tex_names - set(referenced)),
        "companion_groups": companions,
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ffxi-root", default=r"C:\Program Files (x86)\PlayOnline\SquareEnix\FINAL FANTASY XI")
    ap.add_argument("--zones", default="Kuftal,Uleguerand,Beaucedine,Xarcabard,Lufaise,Misareaux,Valkurm,Quicksand,Adoulin,Yahse,Ceizak,Morimar")
    ap.add_argument("--out", default=str(REPO_ROOT / "tools" / "bump_candidate_scan.csv"))
    args = ap.parse_args()

    ffxi_root = Path(args.ffxi_root)
    names = [z.strip() for z in args.zones.split(",") if z.strip()]
    rows = []
    summaries = []
    for zone_id, zone_name, rel_path in selected_zones(names):
        try:
            scan = scan_dat(ffxi_root, zone_id, zone_name, rel_path)
        except FileNotFoundError:
            continue
        except Exception as exc:
            summaries.append(f"{zone_id:3d} {zone_name}: ERROR {exc}")
            continue

        flag_counts = Counter((s["blend_flags"], s["flags2"], s["sub_flag"]) for s in scan["segments"])
        summaries.append(
            f"{zone_id:3d} {zone_name:<28} textures={len(scan['textures']):3d} "
            f"materials={len(scan['referenced']):3d} segments={len(scan['segments']):4d} "
            f"unref_tex={len(scan['unreferenced_textures']):3d} companion_groups={len(scan['companion_groups']):2d}"
        )
        for tex in scan["textures"]:
            rows.append({
                "zone_id": zone_id,
                "zone_name": zone_name,
                "kind": "texture",
                "name": tex["name"],
                "referenced_segments": scan["referenced"].get(tex["name"], 0),
                "type_hex": f"0x{tex['type']:02X}",
                "codec": tex["codec"],
                "width": tex["width"],
                "height": tex["height"],
                "blend_flags": "",
                "flags2": "",
                "sub_flag": "",
                "object_name": "",
                "note": ";".join(x for x in [
                    "unreferenced" if tex["name"] in scan["unreferenced_textures"] else "",
                    tex.get("extra", ""),
                ] if x),
            })
        for seg in scan["segments"]:
            mat = seg["material"]
            suspicious = []
            lower = (seg["object_name"] + " " + mat).lower()
            if any(tok in lower for tok in ("sand", "snow", "ice", "wave", "water", "kusa", "stone", "rock", "iwa", "lava")):
                suspicious.append("surface-name")
            if seg["flags2"] != 0:
                suspicious.append("flags2")
            if (seg["blend_flags"] & 0x7000) not in (0,):
                suspicious.append("blend-high")
            if suspicious:
                rows.append({
                    "zone_id": zone_id,
                    "zone_name": zone_name,
                    "kind": "segment",
                    "name": mat,
                    "referenced_segments": scan["referenced"].get(mat, 0),
                    "type_hex": "",
                    "codec": "",
                    "width": "",
                    "height": "",
                    "blend_flags": f"0x{seg['blend_flags']:04X}",
                    "flags2": f"0x{seg['flags2']:04X}",
                    "sub_flag": f"0x{seg['sub_flag'] & 0xFFFFFFFF:08X}",
                    "object_name": seg["object_name"],
                    "note": "+".join(suspicious),
                })
        for key, vals in scan["companion_groups"].items():
            rows.append({
                "zone_id": zone_id,
                "zone_name": zone_name,
                "kind": "companion_group",
                "name": key,
                "referenced_segments": "",
                "type_hex": "",
                "codec": "",
                "width": "",
                "height": "",
                "blend_flags": "",
                "flags2": "",
                "sub_flag": "",
                "object_name": "",
                "note": " | ".join(vals),
            })
        common_flags = ", ".join(
            f"blend=0x{bf:04X}/f2=0x{f2:04X}/sub=0x{sf & 0xFFFFFFFF:08X}:{count}"
            for (bf, f2, sf), count in flag_counts.most_common(5)
        )
        summaries.append(f"    common_flags: {common_flags}")

    out = Path(args.out)
    with out.open("w", newline="", encoding="utf-8") as f:
        fieldnames = [
            "zone_id", "zone_name", "kind", "name", "referenced_segments",
            "type_hex", "codec", "width", "height", "blend_flags", "flags2",
            "sub_flag", "object_name", "note",
        ]
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)

    print("\n".join(summaries))
    print(f"wrote {out}")


if __name__ == "__main__":
    main()
