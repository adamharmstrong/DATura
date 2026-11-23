#!/usr/bin/env python3
"""Build a complete zone-to-authored-weather inventory from installed FFXI DATs."""

import argparse
import csv
import hashlib
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
DEFAULT_RUNTIME_SCHEDULE = Path(__file__).resolve().parents[1] / "DATura" / "zone_weather_schedule.bin"
SCHEDULE_DAYS = 2160

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


def describe_generator(name, payload):
    """Flags plus the StandardParticleSetup (opcode 0x01) link of a 0x05 record."""
    link_type, link_name = None, ""
    setup = u32(payload, 0x74) - 0x10
    if 0 <= setup <= len(payload) - 0x22 and payload[setup] == 0x01:
        link_name = fourcc(payload[setup + 0x0C:setup + 0x10])
        link_type = payload[setup + 0x21]
    return {
        "name": name,
        "flags": payload[0x69],
        "more_flags": payload[0x6B],
        # PS2 YmGenerater::GetGenerateNum emits (count & 0x1FF) + 1.
        "count_byte": payload[0x68],
        "link_type": link_type,
        "link_name": link_name,
    }


LINK_TYPE_NAMES = {0x0B: "mesh/MMB", 0x0E: "sprite", 0x39: "lens", 0x3D: "sound"}


def classify_rain_branch(item):
    """Explain how a rain/squall branch is represented (see RAIN_AND_WEATHER docs)."""
    generators = item["generators"]
    autorun = [g for g in generators if g["flags"] & 0x10]
    precipitation = [g for g in autorun if g["more_flags"] & 0x20]
    links = sorted({LINK_TYPE_NAMES.get(g["link_type"], f"0x{g['link_type'] or 0:02X}")
                    for g in autorun})
    sounds = sorted({s["sound_id"] for s in item["sounds"] if not s["indoor"]})
    sound_text = (f"outdoor loop SeWave {','.join(map(str, sounds))}" if sounds
                  else "no outdoor loop")
    if precipitation:
        return ("precipitation",
                f"{len(precipitation)} autorun weather-batched emitters "
                f"({', '.join(links)}) render; {sound_text}")
    if not generators:
        return ("environment_only",
                "no 0x05 generators: weather changes 0x2F lighting/fog and ambient sound only "
                f"(indoor/cave branch); {sound_text}")
    if autorun and all(g["link_type"] == 0x3D for g in autorun):
        return ("positional_sound_only",
                f"{len(autorun)} autorun generators emit 0x3D positional sounds, no visuals; "
                f"not yet played by DATura; {sound_text}")
    if autorun:
        return ("weather_scenery_unsupported",
                f"{len(autorun)} autorun non-precipitation generators ({', '.join(links)}) that "
                "the PS2 YmCombineWeather constructor activates outdoors; DATura's weather pass "
                f"draws only weather-batched emitters; {sound_text}")
    return ("inactive_generators_only",
            f"{len(generators)} generators, none autorun (0x69 & 0x10); {sound_text}")


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
                    "sounds": [],
                    "generators": [],
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
                    item["generators"].append(describe_generator(name, payload))
                elif chunk_type == 0x3D:
                    item["sound_resources"] += 1
                    # 0x3D weather sounds are SeSep records named by HHMM; the
                    # SeWave ID at +8 maps to sound*/win/se/seNNN/seNNNNNN.spw.
                    if len(payload) >= 12:
                        item["sounds"].append({
                            "name": name,
                            "sound_id": u32(payload, 8),
                            "indoor": len(lower) > index + 2 and lower[index + 2] == "indo",
                        })
    normalized = sorted({WEATHER_NAMES.get(name, f"unknown:{name}") for name in raw_names})
    for item in detail.values():
        item["environment_record_names"] = sorted(set(item["environment_record_names"]))
    return sorted(raw_names), normalized, sorted(paths), detail


def load_schedule_blobs(path):
    """Return {zone_id: [2160 packed LSB day values]} from zone_weather.sql."""
    if not path or not path.is_file():
        return {}
    blobs = {}
    for zone_id, hex_blob in SCHEDULE_RE.findall(path.read_text(encoding="utf-8")):
        data = bytes.fromhex(hex_blob)
        days = [struct.unpack_from("<H", data, offset)[0] for offset in range(0, len(data) - 1, 2)]
        blobs[int(zone_id)] = (days + [0] * SCHEDULE_DAYS)[:SCHEDULE_DAYS]
    return blobs


def load_schedules(blobs):
    schedules = {}
    for zone_id, days in blobs.items():
        weather_ids = set()
        for value in days:
            if value:
                weather_ids.update((value >> 10, (value >> 5) & 0x1F, value & 0x1F))
        schedules[zone_id] = sorted(weather_ids)
    return schedules


def write_runtime_schedule(path, blobs):
    """Compact schedule read by DATura's ZoneWeatherSchedule::Table."""
    out = bytearray(b"DWSC")
    out += struct.pack("<III", 1, len(blobs), SCHEDULE_DAYS)
    for zone_id in sorted(blobs):
        out += struct.pack("<I", zone_id)
        out += struct.pack(f"<{SCHEDULE_DAYS}H", *blobs[zone_id])
    path.write_bytes(bytes(out))


def build_rows(ffxi_root, zone_table, schedule_blobs):
    schedules = load_schedules(schedule_blobs)
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


def rain_branch_audit(rows):
    audit = []
    for row in rows:
        for path in row["weather_paths"]:
            raw = path.rsplit("/", 1)[-1].lower()
            weather_type = WEATHER_NAMES.get(raw, f"unknown:{raw}")
            if weather_type not in ("rain", "squalls"):
                continue
            item = row["weather_details"][path]
            classification, cause = classify_rain_branch(item)
            audit.append({
                "zone_id": row["zone_id"], "zone_name": row["zone_name"],
                "model_dat": row["model_dat"], "resource_path": path,
                "weather_type": weather_type, "classification": classification, "cause": cause,
                "environment_records": item["environment_records"],
                "generators": item["particle_generators"],
                "precipitation_generators": item["rain_activatable_generators"],
                "outdoor_sound_ids": ";".join(str(i) for i in sorted(
                    {s["sound_id"] for s in item["sounds"] if not s["indoor"]})),
                "generator_summary": " ".join(
                    f"{g['name']}:f{g['flags']:02x}/m{g['more_flags']:02x}/"
                    f"{LINK_TYPE_NAMES.get(g['link_type'], g['link_type'])}:{g['link_name']}"
                    for g in item["generators"]),
            })
    return audit


def write_rain_audit_csv(path, rows):
    audit = rain_branch_audit(rows)
    if not audit:
        return audit
    with path.open("w", newline="", encoding="utf-8-sig") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(audit[0]))
        writer.writeheader()
        writer.writerows(audit)
    return audit


def write_sqlite(path, rows, metadata, schedule_blobs):
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
            CREATE TABLE zone_weather_schedule (
                zone_id INTEGER NOT NULL REFERENCES zones(zone_id),
                cycle_day INTEGER NOT NULL CHECK(cycle_day BETWEEN 0 AND 2159),
                normal_weather_id INTEGER NOT NULL REFERENCES weather_types(weather_id),
                common_weather_id INTEGER NOT NULL REFERENCES weather_types(weather_id),
                rare_weather_id INTEGER NOT NULL REFERENCES weather_types(weather_id),
                PRIMARY KEY(zone_id, cycle_day)
            ) WITHOUT ROWID;
            CREATE TABLE zone_authored_weather_sounds (
                zone_id INTEGER NOT NULL REFERENCES zones(zone_id),
                resource_path TEXT NOT NULL, record_name TEXT NOT NULL,
                indoor INTEGER NOT NULL CHECK(indoor IN (0,1)),
                sound_id INTEGER NOT NULL, spw_path TEXT NOT NULL
            );
            CREATE TABLE zone_rain_branch_audit (
                zone_id INTEGER NOT NULL REFERENCES zones(zone_id), zone_name TEXT NOT NULL,
                model_dat TEXT NOT NULL, resource_path TEXT NOT NULL, weather_type TEXT NOT NULL,
                classification TEXT NOT NULL, cause TEXT NOT NULL,
                environment_records INTEGER NOT NULL, generators INTEGER NOT NULL,
                precipitation_generators INTEGER NOT NULL, outdoor_sound_ids TEXT NOT NULL,
                generator_summary TEXT NOT NULL,
                PRIMARY KEY(zone_id, resource_path)
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
        # LSB semantics: only non-zero days are entries; a day uses the most
        # recent entry at or before it.
        db.executemany("INSERT INTO zone_weather_schedule VALUES (?,?,?,?,?)", [
            (zone_id, day, value >> 10, (value >> 5) & 0x1F, value & 0x1F)
            for zone_id, days in sorted(schedule_blobs.items())
            for day, value in enumerate(days) if value
        ])
        db.executemany("INSERT INTO zone_authored_weather_sounds VALUES (?,?,?,?,?,?)", [
            (row["zone_id"], path, sound["name"], int(sound["indoor"]), sound["sound_id"],
             f"se/se{sound['sound_id'] // 1000:03d}/se{sound['sound_id']:06d}.spw")
            for row in rows for path in row["weather_paths"]
            for sound in row["weather_details"][path]["sounds"]
        ])
        db.executemany("INSERT INTO zone_rain_branch_audit VALUES (?,?,?,?,?,?,?,?,?,?,?,?)", [
            (a["zone_id"], a["zone_name"], a["model_dat"], a["resource_path"], a["weather_type"],
             a["classification"], a["cause"], a["environment_records"], a["generators"],
             a["precipitation_generators"], a["outdoor_sound_ids"], a["generator_summary"])
            for a in rain_branch_audit(rows)
        ])
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
    parser.add_argument("--runtime-schedule", type=Path, default=DEFAULT_RUNTIME_SCHEDULE,
                        help="compact 2160-day schedule loaded by DATura at runtime")
    args = parser.parse_args()
    schedule_blobs = load_schedule_blobs(args.schedule_sql)
    rows = build_rows(args.ffxi_root, args.zone_table, schedule_blobs)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    metadata = {
        "scope": "FFXI retail zone IDs 0-299",
        "schedule_source": "LandSandBoat sql/zone_weather.sql 2160-day cycles",
        "asset_source": "authored /weat resource directories in each installed zone model DAT",
        "ffxi_root": "local installed retail data (path intentionally omitted)",
        "zone_table": "DATura/zone_dat_table.h",
        "interpretation": "scheduled types come from the 2160-day server cycle; authored types are client visuals",
    }
    if args.schedule_sql.is_file():
        metadata["schedule_sql_sha256"] = hashlib.sha256(args.schedule_sql.read_bytes()).hexdigest()
    write_csv(args.output.with_suffix(".csv"), rows)
    args.output.with_suffix(".json").write_text(
        json.dumps({"metadata": metadata, "zones": rows}, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8")
    write_sqlite(args.output.with_suffix(".sqlite"), rows, metadata, schedule_blobs)
    audit_path = args.output.parent / "zone_rain_branch_audit.csv"
    audit = write_rain_audit_csv(audit_path, rows)
    print("rain/squall branches=" + str(len(audit)) + " " + ", ".join(
        f"{name}:{count}" for name, count in sorted(Counter(a["classification"] for a in audit).items())))
    print(f"wrote {audit_path}")
    if schedule_blobs:
        write_runtime_schedule(args.runtime_schedule, schedule_blobs)
        print(f"wrote {args.runtime_schedule}")
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
