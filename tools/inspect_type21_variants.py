#!/usr/bin/env python3
"""Inspect the uncommon grouped-card variants of FFXI type-0x21 chunks."""

import argparse
import csv
import math
import struct
from pathlib import Path


def u16(data, offset):
    return struct.unpack_from("<H", data, offset)[0]


def f32(data, offset):
    return struct.unpack_from("<f", data, offset)[0]


def sane_card(data, offset):
    if offset < 0 or offset + 144 > len(data):
        return False
    for vertex in range(6):
        xyz = struct.unpack_from("<3f", data, offset + vertex * 24)
        if not all(math.isfinite(value) and abs(value) <= 1_000_000 for value in xyz):
            return False
    return True


def chunk_payload(dat_path, chunk_offset):
    data = dat_path.read_bytes()
    info = struct.unpack_from("<I", data, chunk_offset + 4)[0]
    size = (info >> 3) & 0x7FFFF0
    return data[chunk_offset + 16:chunk_offset + size]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--ffxi-root", default=r"C:\Program Files (x86)\PlayOnline\SquareEnix\FINAL FANTASY XI")
    parser.add_argument("--scan", default="tools/effect_chunk_scan.csv")
    parser.add_argument("names", nargs="*", default=["hon4", "hib1"])
    args = parser.parse_args()

    wanted = set(args.names)
    seen = set()
    with Path(args.scan).open(newline="", encoding="utf-8") as stream:
        for row in csv.DictReader(stream):
            if row["chunk_name"] not in wanted or row["chunk_name"] in seen:
                continue
            seen.add(row["chunk_name"])
            payload = chunk_payload(Path(args.ffxi_root) / row["model_dat"], int(row["chunk_offset"], 16))
            print(f"{row['chunk_name']} {row['model_dat']} payload={len(payload)} "
                  f"header={payload[:8].hex(' ')}")

            group_count = payload[2]
            card_count = payload[6]
            cursor = 0x18
            controls = []
            parsed_cards = 0
            valid = True
            for _ in range(group_count):
                kind, group_cards = struct.unpack_from("<HH", payload, cursor)
                controls.append((kind, group_cards))
                cursor += 4
                for _ in range(group_cards):
                    valid = valid and sane_card(payload, cursor)
                    cursor += 144
                    parsed_cards += 1
            expected_size = (cursor + 15) & ~15
            valid = valid and parsed_cards == card_count
            print(f"  cards={card_count} groups={group_count} parsed={parsed_cards} valid={valid} "
                  f"used={cursor} aligned={expected_size} trailing={len(payload) - cursor}")
            print("  prefix controls:", controls)


if __name__ == "__main__":
    main()
