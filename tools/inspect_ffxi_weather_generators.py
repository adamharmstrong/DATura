#!/usr/bin/env python3
"""Inspect FFXI zone weather/effect generators without instantiating their meshes."""

import argparse
import struct
from pathlib import Path


def u16(data, offset):
    return struct.unpack_from("<H", data, offset)[0]


def u32(data, offset):
    return struct.unpack_from("<I", data, offset)[0]


def f32(data, offset):
    return struct.unpack_from("<f", data, offset)[0]


def fourcc(raw):
    return raw.split(b"\0", 1)[0].decode("latin-1", "replace").rstrip()


def printable_ids(raw):
    found = []
    for offset in range(0, max(0, len(raw) - 3), 4):
        value = raw[offset:offset + 4]
        if len(value) == 4 and all(0x20 <= byte <= 0x7E for byte in value):
            found.append(value.decode("latin-1"))
    return found


def opcode_streams(payload):
    if len(payload) < 0x80:
        return []
    # Stored offsets use the complete section as their origin. payload begins at +0x10.
    starts = [u32(payload, 0x70 + index * 4) - 0x10 for index in range(4)]
    streams = []
    for section_index, start in enumerate(starts):
        end = starts[section_index + 1] if section_index + 1 < 4 else len(payload)
        entries = []
        cursor = start
        while 0 <= cursor <= end - 4:
            config = u32(payload, cursor)
            opcode = config & 0xFF
            words = (config >> 8) & 0x1F
            size = max(1, words) * 4
            if cursor + size > end:
                break
            entry = payload[cursor:cursor + size]
            entries.append((opcode, size, config >> 13, printable_ids(entry[4:]), entry.hex(" ")))
            cursor += size
            if opcode == 0:
                break
        streams.append(entries)
    return streams


def iter_chunks(data):
    offset = 0
    stack = []
    while offset <= len(data) - 16:
        info = u32(data, offset + 4)
        chunk_type = info & 0x7F
        size = (info >> 3) & 0x7FFFF0
        if size <= 0 or offset + size > len(data):
            break
        name = fourcc(data[offset:offset + 4])
        if chunk_type == 0x01 and name:
            stack.append(name)
        yield offset, name, chunk_type, "/".join(stack), data[offset + 16:offset + size]
        if chunk_type == 0x00 and stack:
            stack.pop()
        offset += size


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dat", type=Path)
    parser.add_argument("--all", action="store_true", help="include non-weather generators")
    parser.add_argument("--resources", action="store_true", help="list render resources under weat")
    parser.add_argument("--environments", action="store_true", help="list decoded 0x2F sky records")
    parser.add_argument("--keyframes", action="store_true", help="list weather keyframe pairs")
    args = parser.parse_args()

    data = args.dat.read_bytes()
    count = 0
    if args.keyframes:
        for offset, name, chunk_type, directory, payload in iter_chunks(data):
            if chunk_type != 0x19 or "weat" not in directory.lower() or len(payload) < 8:
                continue
            pair_bytes = payload
            if len(pair_bytes) % 8:
                continue
            pairs = [struct.unpack_from("<ff", pair_bytes, pair_offset)
                     for pair_offset in range(0, len(pair_bytes), 8)]
            values = " ".join(f"({time:.6g},{value:.6g})" for time, value in pairs)
            print(f"0x{offset:08X} {directory}/{name}: {values}")
        return
    if args.environments:
        for offset, name, chunk_type, directory, payload in iter_chunks(data):
            if chunk_type != 0x2F or len(payload) < 0xAC:
                continue
            colors = [f"0x{u32(payload, 0x6C + index * 4):08X}" for index in range(8)]
            elevations = [f"{f32(payload, 0x8C + index * 4):.6g}" for index in range(8)]
            print(
                f"0x{offset:08X} {directory}/{name} indoor={u32(payload, 0):#x} "
                f"draw={f32(payload, 0x58):.6g} spokes={u16(payload, 0x5E)} "
                f"radius={f32(payload, 0x68):.6g}\n"
                f"  colors={' '.join(colors)}\n  elevations={' '.join(elevations)}"
            )
        return
    if args.resources:
        for offset, name, chunk_type, directory, _ in iter_chunks(data):
            if "weat" in directory.lower() and chunk_type in (0x1F, 0x20, 0x21, 0x25, 0x2E):
                print(f"0x{offset:08X} type=0x{chunk_type:02X} {directory}/{name}")
        return
    for offset, name, chunk_type, directory, payload in iter_chunks(data):
        if chunk_type != 0x05 or len(payload) < 0x80:
            continue
        more_flags = payload[0x6B]
        weather_batched = bool(more_flags & 0x20)
        if not args.all and not weather_batched and "weat" not in directory.lower():
            continue
        count += 1
        environment_id = fourcc(payload[0x54:0x58])
        gen_flags = payload[0x69]
        print(
            f"0x{offset:08X} {directory}/{name} attach={u16(payload, 0):#06x} "
            f"env={environment_id!r} interval={u16(payload, 0x66) + 1} "
            f"count={payload[0x68]} autorun={bool(gen_flags & 0x10)} "
            f"batched_weather={weather_batched}"
        )
        for stream_index, entries in enumerate(opcode_streams(payload), 1):
            summary = " ".join(
                f"{opcode:02X}[{size},a{alloc}" +
                (f",refs={','.join(refs)}" if refs else "") + "]"
                for opcode, size, alloc, refs, _ in entries
            )
            print(f"  sec{stream_index}: {summary}")
            if stream_index == 2:
                for opcode, _, _, _, raw_hex in entries:
                    if opcode == 0x01:
                        print(f"    StandardParticleSetup: {raw_hex}")
                    elif opcode in (0x09, 0x0B, 0x0F, 0x16):
                        print(f"    ElementSetup {opcode:02X}: {raw_hex}")
            if stream_index == 3:
                for opcode, _, _, _, raw_hex in entries:
                    if opcode in (0x27, 0x28):
                        print(f"    UVScroll {opcode:02X}: {raw_hex}")
    print(f"weather/effect generators: {count}")


if __name__ == "__main__":
    main()
