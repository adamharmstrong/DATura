#!/usr/bin/env python3
"""Write an inventory of FFXI DAT files that embed texture (0x20) chunks."""

import argparse
import csv
import struct
from pathlib import Path


def has_texture_chunk(path: Path) -> bool:
    try:
        with path.open("rb") as stream:
            offset = 0
            while True:
                stream.seek(offset + 4)
                raw_info = stream.read(4)
                if len(raw_info) != 4:
                    return False
                info = struct.unpack("<I", raw_info)[0]
                chunk_type = info & 0x7F
                chunk_size = (info >> 3) & 0x7FFFF0
                if chunk_size < 16:
                    return False
                if chunk_type == 0x20:
                    return True
                offset += chunk_size
    except OSError:
        return False


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("ffxi_root", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    root = args.ffxi_root.resolve()
    dats = sorted(root.rglob("*.DAT"))
    matches = []
    for index, path in enumerate(dats, 1):
        if has_texture_chunk(path):
            matches.append(path.relative_to(root).as_posix())
        if index % 500 == 0:
            print(f"Scanned {index:,}/{len(dats):,}; found {len(matches):,} texture DATs", flush=True)

    with args.output.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.writer(stream)
        writer.writerow(("relative_path",))
        writer.writerows((match,) for match in matches)
    print(f"Wrote {len(matches):,} texture DATs to {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
