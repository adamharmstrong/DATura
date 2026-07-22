#!/usr/bin/env python3
import argparse
import re
import struct
from collections import Counter, defaultdict
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
DECRYPT_HEADER = REPO_ROOT / "DATura" / "model_ff11_decrypt.h"


def read_u32(data, ofs):
    return struct.unpack_from("<I", data, ofs)[0]


def read_i32(data, ofs):
    return struct.unpack_from("<i", data, ofs)[0]


def read_u16(data, ofs):
    return struct.unpack_from("<H", data, ofs)[0]


def cstr(raw):
    text = raw.split(b"\0", 1)[0].decode("latin-1", "replace")
    out = []
    for ch in text:
        code = ord(ch)
        if 32 <= code < 127:
            out.append(ch)
        else:
            out.append(f"\\x{code:02X}")
    return "".join(out)


def load_key_table(name):
    text = DECRYPT_HEADER.read_text(encoding="utf-8")
    m = re.search(rf"{name}\[256\]\s*=\s*\{{(.*?)\}};", text, re.S)
    if not m:
        raise RuntimeError(f"could not find {name} in {DECRYPT_HEADER}")
    vals = [int(x, 16) for x in re.findall(r"0x[0-9A-Fa-f]{2}", m.group(1))]
    if len(vals) != 256:
        raise RuntimeError(f"{name} has {len(vals)} entries")
    return vals


KEY_TABLE = load_key_table("skKeyTable")
KEY_TABLE2 = load_key_table("skKeyTable2")


def decode_mmb2(buf):
    if len(buf) < 8 or buf[6] != 0xFF or buf[7] != 0xFF:
        return
    decode_len = min(buf[0] | (buf[1] << 8) | (buf[2] << 16), len(buf))
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


def decode_mmb(buf):
    if len(buf) >= 8 and buf[3] >= 5:
        decode_len = min(buf[0] | (buf[1] << 8) | (buf[2] << 16), len(buf))
        key = KEY_TABLE[buf[5] ^ 0xF0]
        key_counter = 0
        for pos in range(8, decode_len):
            x = ((key & 0xFF) << 8) | (key & 0xFF)
            key_counter += 1
            key = (key + key_counter) & 0xFFFFFFFF
            buf[pos] ^= (x >> (key & 7)) & 0xFF
            key_counter += 1
            key = (key + key_counter) & 0xFFFFFFFF
    decode_mmb2(buf)


def decode_object_map(buf):
    if len(buf) < 8 or buf[3] < 0x1B:
        return
    decode_len = min(buf[0] | (buf[1] << 8) | (buf[2] << 16), len(buf))
    key = KEY_TABLE[buf[7] ^ 0xFF]
    key_counter = 0
    pos = 8
    while pos < decode_len:
        xor_len = ((key >> 4) & 7) + 16
        if (key & 1) and (pos + xor_len < decode_len):
            for i in range(xor_len):
                buf[pos + i] ^= 0xFF
        key_counter += 1
        key = (key + key_counter) & 0xFFFFFFFF
        pos += xor_len

    node_count = buf[4] | (buf[5] << 8) | (buf[6] << 16)
    stride = 96
    max_nodes = max(0, (len(buf) - 32) // stride)
    for i in range(min(node_count, max_nodes)):
        id_ofs = 32 + i * stride
        for j in range(16):
            buf[id_ofs + j] ^= 0x55


def decrypt_chunk(payload, chunk_type):
    out = bytearray(payload)
    if chunk_type == 0x1C:
        decode_object_map(out)
    elif chunk_type in (0x20, 0x29, 0x2A, 0x2B, 0x2E):
        decode_mmb(out)
    return bytes(out)


def iter_chunks(data):
    ofs = 0
    index = 0
    while ofs <= len(data) - 16:
        name = cstr(data[ofs:ofs + 4])
        info = read_u32(data, ofs + 4)
        chunk_type = info & 0x7F
        size = (info >> 3) & 0x7FFFF0
        if size <= 0 or ofs + size > len(data):
            break
        yield {
            "index": index,
            "ofs": ofs,
            "name": name,
            "type": chunk_type,
            "size": size,
            "payload": data[ofs + 16:ofs + size],
        }
        ofs += size
        index += 1


def parse_texture_header(payload):
    if len(payload) < 60:
        return None
    header_ofs = 0
    ver_ofs = 20
    if read_u32(payload, header_ofs + ver_ofs) != 40:
        found = None
        for candidate in range(0, min(len(payload) - 60, 256)):
            for candidate_ver_ofs in (17, 20):
                if read_u32(payload, candidate + candidate_ver_ofs) == 40:
                    found = (candidate, candidate_ver_ofs)
                    break
            if found:
                break
        if found:
            header_ofs, ver_ofs = found
    p = header_ofs
    width_ofs = ver_ofs + 4
    height_ofs = ver_ofs + 8
    bits_ofs = ver_ofs + 36
    return {
        "header_ofs": header_ofs,
        "ver_ofs": ver_ofs,
        "type_byte": payload[p],
        "name": cstr(payload[p + 1:p + 17]),
        "version": read_u32(payload, p + ver_ofs),
        "width": read_i32(payload, p + width_ofs),
        "height": read_i32(payload, p + height_ofs),
        "bits_per_pal_color": read_u32(payload, p + bits_ofs),
    }


def texture_header_plausible(t):
    return (
        t
        and t["version"] == 40
        and t["type_byte"] in (0xA1, 0x91, 0x01, 0x81, 0xB1)
        and 0 < t["width"] <= 4096
        and 0 < t["height"] <= 4096
    )


def parse_texture(chunk):
    raw_header = parse_texture_header(chunk["payload"])
    dec_header = parse_texture_header(decrypt_chunk(chunk["payload"], chunk["type"]))
    if texture_header_plausible(raw_header):
        h = raw_header
        mode = "raw"
    elif texture_header_plausible(dec_header):
        h = dec_header
        mode = "decrypted"
    else:
        h = dec_header or raw_header
        mode = "unverified"
    if not h:
        return None
    p = h["header_ofs"]
    return {
        "chunk": chunk,
        "decode_mode": mode,
        **h,
    }


def mapgeo_segments(chunk):
    payload = decrypt_chunk(chunk["payload"], chunk["type"])
    if len(payload) < 64:
        return []
    object_name = cstr(payload[16:32])
    out = []
    draw_ofs = 32
    end_draw_ofs = len(payload) - 32
    while draw_ofs <= end_draw_ofs:
        if draw_ofs + 32 > len(payload):
            break
        super_seg_count = read_i32(payload, draw_ofs)
        draw_ofs += 32
        if super_seg_count <= 0 or super_seg_count > 4096:
            break
        for super_index in range(super_seg_count):
            if draw_ofs + 32 > len(payload):
                break
            sub_seg_count = read_i32(payload, draw_ofs)
            sub_flag = read_i32(payload, draw_ofs + 28)
            draw_ofs += 32
            if sub_seg_count < 0 or sub_seg_count > 4096:
                return out
            for sub_index in range(sub_seg_count):
                if draw_ofs + 20 > len(payload):
                    return out
                mat_name = cstr(payload[draw_ofs:draw_ofs + 16])
                draw_ofs += 16
                vert_count = read_u16(payload, draw_ofs)
                blend_flags = read_u16(payload, draw_ofs + 2)
                draw_ofs += 4
                vert_stride = 48 if sub_flag == 0 else 36
                vert_bytes = vert_stride * vert_count
                if draw_ofs + vert_bytes + 4 > len(payload):
                    return out
                draw_ofs += vert_bytes
                index_count = read_u16(payload, draw_ofs)
                flags2 = read_u16(payload, draw_ofs + 2)
                draw_ofs += 4
                if draw_ofs + index_count * 2 > len(payload):
                    return out
                draw_ofs += index_count * 2
                draw_ofs = (draw_ofs + 3) & ~3
                out.append({
                    "object": object_name,
                    "material": mat_name,
                    "vert_count": vert_count,
                    "index_count": index_count,
                    "blend_flags": blend_flags,
                    "flags2": flags2,
                    "sub_flag": sub_flag,
                    "vert_stride": vert_stride,
                })
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("dat")
    args = ap.parse_args()

    data = Path(args.dat).read_bytes()
    chunks = list(iter_chunks(data))
    type_counts = Counter(ch["type"] for ch in chunks)
    mqo_needles = [
        b"Metasequoia Document",
        b"Format Text Ver",
        b"Scene {",
        b"Material ",
        b"Object ",
        b"vertex ",
        b"face ",
        b"tex(",
        b"shader(",
    ]
    raw_hits = {needle.decode("ascii"): data.find(needle) for needle in mqo_needles}

    print(f"file={args.dat}")
    print(f"size={len(data)} chunks={len(chunks)}")
    print("chunk_type_counts=" + ", ".join(f"0x{k:02X}:{v}" for k, v in sorted(type_counts.items())))
    print("raw_mqo_signature_hits=" + ", ".join(
        f"{name}:{'none' if ofs < 0 else hex(ofs)}" for name, ofs in raw_hits.items()))

    textures = [parse_texture(ch) for ch in chunks if ch["type"] == 0x20]
    textures = [t for t in textures if t]
    print("\ntextures:")
    for i, t in enumerate(textures):
        print(
            f"  {i}: chunk={t['chunk']['name']} dat_name={t['name']} "
            f"mode={t['decode_mode']} header_ofs=0x{t['header_ofs']:X} "
            f"ver_ofs=0x{t['ver_ofs']:X} type=0x{t['type_byte']:02X} ver={t['version']} "
            f"{t['width']}x{t['height']} bpc={t['bits_per_pal_color']}"
        )

    segments = []
    decrypted_hits = Counter()
    for ch in chunks:
        if ch["type"] in (0x1C, 0x20, 0x29, 0x2A, 0x2B, 0x2E):
            payload = decrypt_chunk(ch["payload"], ch["type"])
            for needle in mqo_needles:
                if payload.find(needle) >= 0:
                    decrypted_hits[needle.decode("ascii")] += 1
        if ch["type"] == 0x2E:
            segments.extend(mapgeo_segments(ch))
    print("decrypted_chunk_mqo_signature_hits=" + (
        ", ".join(f"{k}:{v}" for k, v in sorted(decrypted_hits.items())) if decrypted_hits else "none"))

    print(f"\nmapgeo_segments={len(segments)}")
    by_mat = defaultdict(lambda: {"segments": 0, "verts": 0, "indices": 0, "blend_flags": Counter(), "objects": Counter()})
    for seg in segments:
        row = by_mat[seg["material"]]
        row["segments"] += 1
        row["verts"] += seg["vert_count"]
        row["indices"] += seg["index_count"]
        row["blend_flags"][seg["blend_flags"]] += 1
        row["objects"][seg["object"]] += 1

    print("native_material_references:")
    for mat, row in sorted(by_mat.items()):
        flags = " ".join(f"0x{k:04X}:{v}" for k, v in sorted(row["blend_flags"].items()))
        objects = ", ".join(name for name, _ in row["objects"].most_common(4))
        print(
            f"  {mat}: segments={row['segments']} verts={row['verts']} "
            f"indices={row['indices']} blend_flags=[{flags}] objects=[{objects}]"
        )

    print("\nimporter_generated_variants_per_texture:")
    suffixes = ["", "_explicitshiny", "_explicitsoftblend", "_explicitnoblend", "_explicithardalpha"]
    for t in textures:
        print("  " + ", ".join(t["name"] + suffix for suffix in suffixes))


if __name__ == "__main__":
    main()
