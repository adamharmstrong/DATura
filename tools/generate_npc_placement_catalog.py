#!/usr/bin/env python3
"""Generate DATura's compact zone NPC catalog from LandSandBoat npc_list.sql."""

from __future__ import annotations

import argparse
import csv
from pathlib import Path


def split_sql_values(line: str) -> list[str]:
    prefix = "INSERT INTO `npc_list` VALUES ("
    if not line.startswith(prefix):
        return []
    text = line[len(prefix):]
    end = text.rfind(");")
    if end < 0:
        return []
    text = text[:end]
    fields: list[str] = []
    current: list[str] = []
    quoted = False
    escaped = False
    for char in text:
        if escaped:
            current.append(char)
            escaped = False
        elif char == "\\" and quoted:
            escaped = True
        elif char == "'":
            quoted = not quoted
        elif char == "," and not quoted:
            fields.append("".join(current).strip())
            current.clear()
        else:
            current.append(char)
    fields.append("".join(current).strip())
    return fields


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--commit", required=True)
    args = parser.parse_args()

    rows: list[list[object]] = []
    for line in args.source.read_text(encoding="utf-8", errors="replace").splitlines():
        fields = split_sql_values(line)
        if len(fields) != 19:
            continue
        try:
            npc_id = int(fields[0])
            rotation = int(fields[3])
            x, y, z = (float(fields[i]) for i in (4, 5, 6))
            animation = int(fields[10])
            status = int(fields[13])
            look_hex = fields[15][2:] if fields[15].lower().startswith("0x") else ""
            look = bytes.fromhex(look_hex)
        except (ValueError, OverflowError):
            continue
        if status != 0 or (x == 0.0 and y == 0.0 and z == 0.0) or len(look) != 20:
            continue
        look_kind = int.from_bytes(look[0:2], "little")
        if look_kind not in (0, 1):
            continue
        name = fields[2] if fields[2] not in ("", "NULL") else fields[1]
        rows.append([
            (npc_id & 0xFFF000) >> 12,
            npc_id,
            npc_id & 0xFFF,
            name,
            rotation,
            f"{x:.3f}",
            f"{y:.3f}",
            f"{z:.3f}",
            animation,
            look_hex.upper(),
        ])

    rows.sort(key=lambda row: (int(row[0]), int(row[2])))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", encoding="utf-8", newline="") as stream:
        stream.write("# Derived from LandSandBoat sql/npc_list.sql\n")
        stream.write(f"# upstream_commit={args.commit}\n")
        stream.write("# Only normally visible, positioned NPCs with renderable character/model looks.\n")
        writer = csv.writer(stream, lineterminator="\n")
        writer.writerow(["zone_id", "entity_id", "entity_index", "name", "rotation", "x", "y", "z", "animation", "look_hex"])
        writer.writerows(rows)
    print(f"wrote {len(rows)} NPC placements to {args.output}")


if __name__ == "__main__":
    main()
