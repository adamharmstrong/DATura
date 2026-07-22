import argparse
import csv
import re
import struct
from collections import Counter, defaultdict
from pathlib import Path


CHUNK_HEADER_SIZE = 16
CHUNK_MAP = 0x1C
CHUNK_MAPGEO = 0x2E
NAME_LEN = 16


def parse_key_table(header_text, table_name):
    match = re.search(rf"{table_name}\[256\]\s*=\s*\{{(.*?)\}};", header_text, re.S)
    if not match:
        raise RuntimeError(f"could not find {table_name}")
    return [int(x, 16) for x in re.findall(r"0x[0-9A-Fa-f]{2}", match.group(1))]


SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parent
DECRYPT_HEADER = REPO_ROOT / "DATura" / "model_ff11_decrypt.h"
HEADER_TEXT = DECRYPT_HEADER.read_text(encoding="utf-8", errors="ignore")
KEY_TABLE = parse_key_table(HEADER_TEXT, "skKeyTable")
KEY_TABLE2 = parse_key_table(HEADER_TEXT, "skKeyTable2")


def c_name(raw):
    raw = raw.split(b"\0", 1)[0]
    return "".join(chr(b) if 32 <= b < 127 else "?" for b in raw)


def u32_name(v):
    raw = struct.pack("<I", v)
    return c_name(raw)


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
    node_stride = 96
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


def parse_chunks(data):
    off = 0
    while off <= len(data) - CHUNK_HEADER_SIZE:
        info = struct.unpack_from("<I", data, off + 4)[0]
        chunk_type = info & 0x7F
        chunk_size = (info >> 3) & 0x7FFFF0
        if chunk_size <= 0 or off + chunk_size > len(data):
            break
        yield {
            "offset": off,
            "name": c_name(data[off:off + 4]),
            "type": chunk_type,
            "size": chunk_size,
            "payload": data[off + CHUNK_HEADER_SIZE:off + chunk_size],
        }
        off += chunk_size


def parse_map_objects(payload):
    payload = decrypt_object_map(payload)
    if len(payload) < 32:
        return []
    count = u24le(payload, 4)
    out = []
    for i in range(min(count, max(0, (len(payload) - 32) // 96))):
        off = 32 + i * 96
        name = c_name(payload[off:off + NAME_LEN])
        trans = struct.unpack_from("<fff", payload, off + 16)
        scale = struct.unpack_from("<fff", payload, off + 40)
        flags0 = struct.unpack_from("<I", payload, off + 56)[0]
        flags1 = struct.unpack_from("<I", payload, off + 72)[0]
        out.append((name, trans, scale, flags0, flags1))
    return out


def iter_mapgeo_segments(payload):
    payload = decrypt_mmb(payload)
    if len(payload) < 32:
        return
    object_name = c_name(payload[16:32])
    draw_off = 32
    end_draw = len(payload) - 32
    while draw_off <= end_draw:
        if draw_off + 32 > len(payload):
            return
        super_count = struct.unpack_from("<i", payload, draw_off)[0]
        super_flag = struct.unpack_from("<i", payload, draw_off + 28)[0]
        draw_off += 32
        if super_count < 0 or super_count > 4096:
            return
        for _ in range(super_count):
            if draw_off + 32 > len(payload):
                return
            sub_count = struct.unpack_from("<i", payload, draw_off)[0]
            sub_flag = struct.unpack_from("<i", payload, draw_off + 28)[0]
            draw_off += 32
            if sub_count < 0 or sub_count > 4096:
                return
            for _ in range(sub_count):
                if draw_off % 4 or draw_off + NAME_LEN + 4 > len(payload):
                    return
                material = c_name(payload[draw_off:draw_off + NAME_LEN])
                draw_off += NAME_LEN
                vert_count, blend_flags = struct.unpack_from("<HH", payload, draw_off)
                draw_off += 4
                vert_stride = 48 if sub_flag == 0 else 36
                vert_data_off = draw_off
                draw_off += vert_stride * vert_count
                if draw_off + 4 > len(payload):
                    return
                index_count, flags2 = struct.unpack_from("<HH", payload, draw_off)
                draw_off += 4 + index_count * 2
                draw_off = (draw_off + 3) & ~3
                if vert_data_off + vert_stride * vert_count > len(payload):
                    return

                yield {
                    "object_name": object_name,
                    "material": material,
                    "super_flag": super_flag,
                    "sub_flag": sub_flag,
                    "blend_flags": blend_flags,
                    "flags2": flags2,
                    "vert_stride": vert_stride,
                    "vert_count": vert_count,
                    "index_count": index_count,
                }


def inspect(path, out_csv, dump_type, list_chunks):
    data = Path(path).read_bytes()
    chunks = list(parse_chunks(data))
    chunk_counts = Counter(c["type"] for c in chunks)
    if list_chunks:
        for i, c in enumerate(chunks):
            print(
                f"{i:04d}: offset=0x{c['offset']:08X} name={c['name']!r:<8} "
                f"type=0x{c['type']:02X} size={c['size']}"
            )
        return
    if dump_type is not None:
        for c in chunks:
            if c["type"] != dump_type:
                continue
            payload = c["payload"]
            print(
                f"chunk offset=0x{c['offset']:08X} name={c['name']!r} "
                f"type=0x{c['type']:02X} size={c['size']} payload={len(payload)}"
            )
            print("  first64=" + payload[:64].hex(" "))
            if len(payload) >= 32:
                ints = struct.unpack_from("<" + "I" * min(8, len(payload) // 4), payload, 0)
                print("  u32=" + " ".join(f"0x{x:08X}" for x in ints))
            if payload[:4] == b"RID\0" and len(payload) >= 64:
                record_count = struct.unpack_from("<I", payload, 48)[0]
                record_stride = 64
                max_records = max(0, (len(payload) - 64) // record_stride)
                print(f"  RID records={record_count} stride={record_stride} max={max_records}")
                for ri in range(min(record_count, max_records, 24)):
                    ro = 64 + ri * record_stride
                    raw = payload[ro:ro + record_stride]
                    name0 = c_name(raw[:16])
                    name1 = c_name(raw[16:32])
                    vals = struct.unpack_from("<" + "I" * 8, raw, 32)
                    id_names = [u32_name(v) for v in vals]
                    print(
                        f"    {ri:02d}: name0={name0!r:<18} name1={name1!r:<18} "
                        + " ".join(f"{v:08X}" for v in vals)
                        + "  ids=" + ",".join(repr(n) for n in id_names)
                    )
        return

    map_objects = []
    segments = []
    for c in chunks:
        if c["type"] == CHUNK_MAP:
            map_objects.extend(parse_map_objects(c["payload"]))
        elif c["type"] == CHUNK_MAPGEO:
            segments.extend(iter_mapgeo_segments(c["payload"]) or [])

    map_object_names = Counter(o[0] for o in map_objects)
    for segment in segments:
        segment["map_refs"] = map_object_names.get(segment["object_name"], 0)

    if out_csv:
        with Path(out_csv).open("w", newline="", encoding="utf-8") as f:
            fields = [
                "object_name", "material", "super_flag", "sub_flag", "blend_flags",
                "flags2", "vert_stride", "vert_count", "index_count", "map_refs",
            ]
            writer = csv.DictWriter(f, fields)
            writer.writeheader()
            writer.writerows(segments)

    print(f"path={path}")
    print(f"chunks={len(chunks)} chunk_types={dict(sorted(chunk_counts.items()))}")
    print(f"map_objects={len(map_objects)} unique_object_names={len(set(o[0] for o in map_objects))}")
    print(f"mapgeo_segments={len(segments)} unique_geo_names={len(set(s['object_name'] for s in segments))}")
    geo_names = Counter(s["object_name"] for s in segments)
    suspicious_map_objects = [
        (name, count) for name, count in map_object_names.items()
        if any(tok in name.lower() for tok in ("col", "hit", "walk", "wall", "floor", "bound"))
    ]
    suspicious_geo_names = [
        (name, geo_names[name], map_object_names.get(name, 0))
        for name in geo_names
        if any(tok in name.lower() for tok in ("col", "hit", "walk", "wall", "floor", "bound"))
    ]
    print(f"suspicious_map_object_names={len(suspicious_map_objects)}")
    for name, count in sorted(suspicious_map_objects)[:20]:
        print(f"  map_ref {name:<16} count={count}")
    print(f"suspicious_geo_names={len(suspicious_geo_names)}")
    for name, seg_count, ref_count in sorted(suspicious_geo_names)[:20]:
        print(f"  geo     {name:<16} segments={seg_count} map_refs={ref_count}")
    print(f"mapgeo_flag_pairs={len(set((s['sub_flag'], s['blend_flags'], s['flags2']) for s in segments))}")
    for (sub_flag, blend_flags, flags2), count in Counter((s["sub_flag"], s["blend_flags"], s["flags2"]) for s in segments).most_common(12):
        print(f"  flags count={count:5d} sub=0x{sub_flag:08X} blend=0x{blend_flags:04X} flags2=0x{flags2:04X}")
    suspicious = [s for s in segments if any(tok in (s["object_name"] + " " + s["material"]).lower() for tok in ("col", "hit", "walk", "wall", "floor", "bound"))]
    print(f"suspicious_name_segments={len(suspicious)}")
    for s in suspicious[:20]:
        print(f"  {s['object_name']:<16} mat={s['material']:<16} sub=0x{s['sub_flag']:08X} blend=0x{s['blend_flags']:04X} flags2=0x{s['flags2']:04X}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("path")
    parser.add_argument("--out")
    parser.add_argument("--dump-type", type=lambda s: int(s, 0))
    parser.add_argument("--list-chunks", action="store_true")
    args = parser.parse_args()
    inspect(args.path, args.out, args.dump_type, args.list_chunks)


if __name__ == "__main__":
    main()
