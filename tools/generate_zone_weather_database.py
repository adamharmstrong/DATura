#!/usr/bin/env python3
"""Build a complete zone-to-authored-weather inventory from installed FFXI DATs."""

import argparse
import csv
import json
import re
import sqlite3
import struct
from collections import Counter
from pathlib import Path


DEFAULT_FFXI_ROOT = Path(r"C:\Program Files (x86)\PlayOnline\SquareEnix\FINAL FANTASY XI")
DEFAULT_ZONE_TABLE = Path(__file__).resolve().parents[1] / "DATura" / "zone_dat_table.h"
DEFAULT_OUTPUT = Path(__file__).resolve().parent / "zone_weather_database"
DEFAULT_SCHEDULE_SQL = Path(__file__).resolve().parents[1] / ".tmp_lsb" / "sql" / "zone_weather.sql"

WEATHER_IDS = {
    0: "none", 1: "sunshine", 2: "clouds", 3: "fog",
    4: "hot_spell", 5: "heat_wave", 6: "rain", 7: "squalls",
    8: "dust_storm", 9: "sandstorm", 10: "wind", 11: "gales",
    12: "snow", 13: "blizzard", 14: "thunder", 15: "thunderstorms",
    16: "aurora", 17: "stellar_glare", 18: "gloom", 19: "darkness",
}

# Resource directory names used by the retail zone DATs.  The normalized value is
# deliberately a visual/weather family rather than an elemental game mechanic.
WEATHER_NAMES = {
    "fine": "clear",
    "suny": "sunshine",
    "clod": "clouds",
    "mist": "fog",
    "wind": "wind",
    "dust": "dust_storm",
    "sand": "sandstorm",
    "rain": "rain",
    "squa": "squalls",
    "thdr": "thunder",
    "snow": "snow",
    "bliz": "blizzard",
    "dryw": "hot_spell",
    "dry1": "hot_spell",
    "heat": "heat_wave",
    "stom": "gales",
    "squl": "squalls",
    "squ1": "squalls",
    "bolt": "thunderstorms",
    "aura": "aurora",
    "aur1": "aurora",
    "ligt": "stellar_glare",
    "ligh": "stellar_glare",
    "fogd": "gloom",
    "dark": "darkness",
    "clo1": "clouds",
    "mis1": "fog",
    "fog1": "gloom",
    "rai1": "rain",
    "sun1": "sunshine",
    "thd1": "thunder",
    "win1": "wind",
    "dus1": "dust_storm",
}

ZONE_RE = re.compile(
    r'^\s*\{\s*(\d+)\s*,\s*"([^"]*)"\s*,\s*"([^"]*)"\s*,', re.MULTILINE
)
SCHEDULE_RE = re.compile(r"INSERT INTO `zone_weather` VALUES \((\d+),0x([0-9A-Fa-f]+)\);")


def u32(data, offset):
    return struct.unpack_from("<I", data, offset)[0]


def fourcc(raw):
    return raw.split(b"\0", 1)[0].decode("latin-1", "replace").rstrip()


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
        yield name, chunk_type, tuple(stack), data[offset + 16:offset + size]
        if chunk_type == 0x00 and stack:
            stack.pop()
        offset += size


def load_zones(path):
    text = path.read_text(encoding="utf-8")
    zones = [
        {"zone_id": int(zone_id), "zone_name": name, "model_dat": model_dat}
        for zone_id, name, model_dat in ZONE_RE.findall(text)
    ]
    by_id = {zone["zone_id"]: zone for zone in zones}
    return [by_id.get(zone_id, {"zone_id": zone_id, "zone_name": "(unknown)", "model_dat": ""})
            for zone_id in range(300)]


def inspect_dat(path):
    raw_names = set()
    paths = set()
    detail = {}
    for name, chunk_type, parts, payload in iter_chunks(path.read_bytes()):
        lower = [part.lower() for part in parts]
        for index, part in enumerate(lower[:-1]):
            if part == "weat":
                raw = lower[index + 1]
                weather_path = "/".join(parts[:index + 2])
                raw_names.add(raw)
                paths.add(weather_path)
                item = detail.setdefault(weather_path, {
                    "environment_records": 0,
                    "environment_record_names": [],
                    "particle_generators": 0,
                    "active_particle_generators": 0,
                    "weather_batched_generators": 0,
                    "rain_activatable_generators": 0,
                    "sound_resources": 0,
                })
                if chunk_type == 0x2F:
                    item["environment_records"] += 1
                    if name:
                        item["environment_record_names"].append(name)
                elif chunk_type == 0x05 and len(payload) >= 0x6C:
                    item["particle_generators"] += 1
                    generator_flags = payload[0x69]
                    more_flags = payload[0x6B]
                    if generator_flags & 0x10:
                        item["active_particle_generators"] += 1
                    if more_flags & 0x20:
                        item["weather_batched_generators"] += 1
                    # The PS2 rain activation path selects authored, enabled
                    # weather generators. These are the corresponding serialized flags.
                    if (generator_flags & 0x10) and (more_flags & 0x20):
                        item["rain_activatable_generators"] += 1
                elif chunk_type == 0x3D:
                    item["sound_resources"] += 1
    normalized = sorted({WEATHER_NAMES.get(name, f"unknown:{name}") for name in raw_names})
    for item in detail.values():
        item["environment_record_names"] = sorted(set(item["environment_record_names"]))
    return sorted(raw_names), normalized, sorted(paths), detail


def load_schedules(path):
    if not path or not path.is_file():
        return {}
    schedules = {}
    for zone_id, hex_blob in SCHEDULE_RE.findall(path.read_text(encoding="utf-8")):
        data = bytes.fromhex(hex_blob)
        weather_ids = set()
        for offset in range(0, len(data) - 1, 2):
            value = struct.unpack_from("<H", data, offset)[0]
            if value:
                weather_ids.update((value >> 10, (value >> 5) & 0x1F, value & 0x1F))
        schedules[int(zone_id)] = sorted(weather_ids)
    return schedules


def build_rows(ffxi_root, zone_table, schedule_sql):
    schedules = load_schedules(schedule_sql)
    rows = []
    for zone in load_zones(zone_table):
        relative = zone["model_dat"]
        dat_path = ffxi_root / Path(relative.replace("/", "\\")) if relative else None
        if not relative:
            status, raw, weather, paths, detail = "no_model_mapping", [], [], [], {}
        elif not dat_path.is_file():
            status, raw, weather, paths, detail = "model_missing", [], [], [], {}
        else:
            try:
                raw, weather, paths, detail = inspect_dat(dat_path)
                status = "ok" if weather else "no_authored_weather"
            except (OSError, struct.error) as error:
                status, raw, weather, paths, detail = f"parse_error:{type(error).__name__}", [], [], [], {}
        schedule_ids = schedules.get(zone["zone_id"])
        scheduled = ([WEATHER_IDS.get(value, f"unknown_id:{value}") for value in schedule_ids]
                     if schedule_ids is not None else [])
        rows.append({
            **zone,
            "asset_status": status,
            "schedule_status": "ok" if schedule_ids is not None else "not_in_schedule_source",
            "allows_scheduled_weather": bool(scheduled),
            "scheduled_weather_ids": schedule_ids or [],
            "scheduled_weather_types": scheduled,
            "authored_weather_types": weather,
            "raw_weather_groups": raw,
            "weather_paths": paths,
            "weather_details": detail,
        })
    return rows


def write_csv(path, rows):
    fields = ["zone_id", "zone_name", "model_dat", "schedule_status", "asset_status",
              "allows_scheduled_weather", "scheduled_weather_ids", "scheduled_weather_types",
              "authored_weather_types", "raw_weather_groups", "weather_paths", "weather_details"]
    with path.open("w", newline="", encoding="utf-8-sig") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        for row in rows:
            writer.writerow({**row,
                             "scheduled_weather_ids": ";".join(map(str, row["scheduled_weather_ids"])),
                             "scheduled_weather_types": ";".join(row["scheduled_weather_types"]),
                             "authored_weather_types": ";".join(row["authored_weather_types"]),
                             "raw_weather_groups": ";".join(row["raw_weather_groups"]),
                             "weather_paths": ";".join(row["weather_paths"]),
                             "weather_details": json.dumps(row["weather_details"], separators=(",", ":"))})


def write_sqlite(path, rows, metadata):
    if path.exists():
        path.unlink()
    db = sqlite3.connect(path)
    try:
        db.executescript("""
            CREATE TABLE zones (
                zone_id INTEGER PRIMARY KEY, zone_name TEXT NOT NULL, model_dat TEXT NOT NULL,
                schedule_status TEXT NOT NULL, asset_status TEXT NOT NULL,
                allows_scheduled_weather INTEGER NOT NULL CHECK(allows_scheduled_weather IN (0,1))
            );
            CREATE TABLE weather_types (
                weather_id INTEGER PRIMARY KEY, weather_type TEXT NOT NULL UNIQUE
            );
            CREATE TABLE zone_scheduled_weather (
                zone_id INTEGER NOT NULL REFERENCES zones(zone_id),
                weather_id INTEGER NOT NULL REFERENCES weather_types(weather_id),
                PRIMARY KEY(zone_id, weather_id)
            );
            CREATE TABLE zone_authored_weather (
                zone_id INTEGER NOT NULL REFERENCES zones(zone_id), weather_type TEXT NOT NULL,
                raw_group TEXT NOT NULL, resource_path TEXT NOT NULL,
                environment_records INTEGER NOT NULL, particle_generators INTEGER NOT NULL,
                active_particle_generators INTEGER NOT NULL,
                weather_batched_generators INTEGER NOT NULL,
                rain_activatable_generators INTEGER NOT NULL,
                sound_resources INTEGER NOT NULL,
                environment_record_names TEXT NOT NULL,
                PRIMARY KEY(zone_id, raw_group, resource_path)
            );
            CREATE TABLE metadata (key TEXT PRIMARY KEY, value TEXT NOT NULL);
            CREATE INDEX scheduled_weather_by_type ON zone_scheduled_weather(weather_id, zone_id);
            CREATE INDEX authored_weather_by_type ON zone_authored_weather(weather_type, zone_id);
        """)
        db.executemany("INSERT INTO zones VALUES (?,?,?,?,?,?)", [
            (r["zone_id"], r["zone_name"], r["model_dat"], r["schedule_status"],
             r["asset_status"], int(r["allows_scheduled_weather"]))
            for r in rows
        ])
        db.executemany("INSERT INTO weather_types VALUES (?,?)", sorted(WEATHER_IDS.items()))
        scheduled_links = [(r["zone_id"], weather_id) for r in rows for weather_id in r["scheduled_weather_ids"]]
        db.executemany("INSERT INTO zone_scheduled_weather VALUES (?,?)", scheduled_links)
        links = []
        for row in rows:
            for path in row["weather_paths"]:
                raw = path.rsplit("/", 1)[-1].lower()
                item = row["weather_details"][path]
                links.append((row["zone_id"], WEATHER_NAMES.get(raw, f"unknown:{raw}"), raw, path,
                              item["environment_records"], item["particle_generators"],
                              item["active_particle_generators"], item["weather_batched_generators"],
                              item["rain_activatable_generators"], item["sound_resources"],
                              ";".join(item["environment_record_names"])))
        db.executemany("INSERT INTO zone_authored_weather VALUES (?,?,?,?,?,?,?,?,?,?,?)", links)
        db.executemany("INSERT INTO metadata VALUES (?,?)", sorted(metadata.items()))
        db.commit()
    finally:
        db.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ffxi-root", type=Path, default=DEFAULT_FFXI_ROOT)
    parser.add_argument("--zone-table", type=Path, default=DEFAULT_ZONE_TABLE)
    parser.add_argument("--schedule-sql", type=Path, default=DEFAULT_SCHEDULE_SQL)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT,
                        help="output stem; .csv, .json, and .sqlite are written")
    args = parser.parse_args()
    rows = build_rows(args.ffxi_root, args.zone_table, args.schedule_sql)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    metadata = {
        "scope": "FFXI retail zone IDs 0-299",
        "schedule_source": "LandSandBoat sql/zone_weather.sql 2160-day cycles",
        "asset_source": "authored /weat resource directories in each installed zone model DAT",
        "ffxi_root": "local installed retail data (path intentionally omitted)",
        "zone_table": "DATura/zone_dat_table.h",
        "interpretation": "scheduled types come from the 2160-day server cycle; authored types are client visuals",
    }
    write_csv(args.output.with_suffix(".csv"), rows)
    args.output.with_suffix(".json").write_text(
        json.dumps({"metadata": metadata, "zones": rows}, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8")
    write_sqlite(args.output.with_suffix(".sqlite"), rows, metadata)
    counts = Counter(r["asset_status"] for r in rows)
    schedule_counts = Counter(r["schedule_status"] for r in rows)
    types = Counter(t for r in rows for t in r["scheduled_weather_types"])
    print(f"zones={len(rows)} status={dict(counts)}")
    print(f"schedules={dict(schedule_counts)}")
    print("weather=" + ", ".join(f"{name}:{count}" for name, count in sorted(types.items())))
    print(f"wrote {args.output.with_suffix('.csv')}")
    print(f"wrote {args.output.with_suffix('.json')}")
    print(f"wrote {args.output.with_suffix('.sqlite')}")


if __name__ == "__main__":
    main()
