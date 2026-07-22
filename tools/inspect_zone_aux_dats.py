import argparse
import csv
import json
import math
import os
import re
import struct
from collections import Counter


DEFAULT_FFXI_ROOT = r"C:\Program Files (x86)\PlayOnline\SquareEnix\FINAL FANTASY XI"


def read_zone_table(path):
    zones = {}
    with open(path, "r", encoding="utf-8-sig", newline="") as f:
        for row in csv.DictReader(f):
            if not row.get("ID"):
                continue
            zones[int(row["ID"])] = {
                "id": int(row["ID"]),
                "name": row.get("NAME", ""),
                "model": row.get("MODEL", ""),
                "dialog": row.get("DIALOG", ""),
                "npc": row.get("NPCs", ""),
                "event": row.get("EVENTS", ""),
            }
    return zones


def dat_path(root, rel):
    return os.path.join(root, rel.replace("/", os.sep))


def parse_chunks(data):
    chunks = []
    ofs = 0
    while ofs + 16 <= len(data):
        name = data[ofs:ofs + 4]
        info = struct.unpack_from("<I", data, ofs + 4)[0]
        chunk_type = info & 0x7F
        size = (info >> 3) & 0x7FFFF0
        if size <= 0 or ofs + size > len(data):
            break
        chunks.append({
            "offset": ofs,
            "name": name,
            "type": chunk_type,
            "size": size,
            "data_offset": ofs + 16,
            "data_size": size - 16,
        })
        ofs += size
    return chunks


def printable_strings(data, min_len=4):
    # This catches ASCII-ish names/tokens. FFXI text can use custom encodings, so
    # absence of strings here does not mean absence of dialogue.
    pattern = rb"[\x20-\x7e]{" + str(min_len).encode("ascii") + rb",}"
    out = []
    for m in re.finditer(pattern, data):
        text = m.group(0).decode("ascii", errors="replace")
        out.append((m.start(), text))
    return out


def score_float(v):
    if not math.isfinite(v):
        return False
    return -20000.0 <= v <= 20000.0 and abs(v) > 0.0001


def candidate_float_records(data, stride, limit=12):
    hits = []
    for ofs in range(0, max(0, len(data) - stride + 1), 4):
        floats = []
        for i in range(0, min(stride, 48), 4):
            try:
                floats.append(struct.unpack_from("<f", data, ofs + i)[0])
            except struct.error:
                floats.append(float("nan"))
        plausible = sum(1 for v in floats[:12] if score_float(v))
        zeros = sum(1 for v in floats[:12] if v == 0.0)
        if plausible >= 3 and zeros <= 7:
            hits.append((ofs, floats[:12]))
            if len(hits) >= limit:
                break
    return hits


def candidate_fixed_records(data, stride, limit=12):
    hits = []
    for ofs in range(0, max(0, len(data) - stride + 1), 2):
        vals = []
        for i in range(0, min(stride, 32), 2):
            vals.append(struct.unpack_from("<h", data, ofs + i)[0])
        plausible = sum(1 for v in vals[:16] if -32768 < v < 32767 and abs(v) > 8)
        if plausible >= 6:
            hits.append((ofs, vals[:16]))
            if len(hits) >= limit:
                break
    return hits


def describe_dat(label, path):
    print(f"\n[{label}] {path}")
    if not path:
        print("  no DAT path in zone table")
        return
    if not os.path.exists(path):
        print("  missing on disk")
        return

    data = open(path, "rb").read()
    print(f"  size: {len(data):,} bytes")
    print("  first16:", data[:16].hex(" "))
    if label == "EVENT" and len(data) >= 64:
        dwords = [struct.unpack_from("<I", data, i)[0] for i in range(0, 64, 4)]
        print("  first dwords:", ", ".join(f"0x{x:08X}" for x in dwords))

    chunks = parse_chunks(data)
    if chunks:
        counts = Counter(c["type"] for c in chunks)
        print(f"  chunks: {len(chunks)} parsed; type counts: {dict(sorted(counts.items()))}")
        for c in chunks[:12]:
            safe_name = "".join(chr(b) if 32 <= b < 127 else "." for b in c["name"])
            print(f"    +0x{c['offset']:06X} name={safe_name!r} type=0x{c['type']:02X} size=0x{c['size']:X}")
    else:
        print("  chunks: none parsed by DATura's chunk-header convention")

    strings = printable_strings(data)
    print(f"  printable strings: {len(strings)}")
    for ofs, text in strings[:20]:
        print(f"    +0x{ofs:06X} {text[:96]}")

    for stride in (32, 48, 64, 80, 96):
        hits = candidate_float_records(data, stride, limit=3)
        if hits:
            print(f"  candidate float records, stride {stride}:")
            for ofs, vals in hits:
                nums = ", ".join(f"{v:.3g}" for v in vals[:9])
                print(f"    +0x{ofs:06X}: {nums}")
            break

    for stride in (16, 24, 32, 48, 64):
        hits = candidate_fixed_records(data, stride, limit=3)
        if hits:
            print(f"  candidate int16/fixed records, stride {stride}:")
            for ofs, vals in hits:
                nums = ", ".join(str(v) for v in vals[:12])
                print(f"    +0x{ofs:06X}: {nums}")
            break


def read_npc_name_records(path):
    if not path or not os.path.exists(path):
        return []

    data = open(path, "rb").read()
    records = []
    for index, ofs in enumerate(range(0, len(data) - 31, 32)):
        rec = data[ofs:ofs + 32]
        raw_name = rec[:16].split(b"\0", 1)[0]
        name = raw_name.decode("ascii", errors="replace").strip()
        record_id = struct.unpack_from("<H", rec, 28)[0]
        record_tag = struct.unpack_from("<H", rec, 30)[0]
        if name or record_id or record_tag:
            records.append({
                "index": index,
                "offset": ofs,
                "name": name,
                "record_id": record_id,
                "record_tag": record_tag,
                "raw_tail_hex": rec[16:32].hex(" "),
            })
    return records


def event_tokens(path):
    if not path or not os.path.exists(path):
        return []
    data = open(path, "rb").read()
    tokens = []
    for ofs, text in printable_strings(data, min_len=4):
        if re.fullmatch(r"[A-Za-z][A-Za-z0-9_#+'\-.]{2,31}", text):
            tokens.append({"offset": ofs, "token": text})
    return tokens


def read_s32(data, ofs):
    if ofs < 0 or ofs + 4 > len(data):
        return None
    return struct.unpack_from("<i", data, ofs)[0]


def looks_like_fixed_position(vals):
    if len(vals) < 3:
        return False
    x, z, y = vals[0], vals[1], vals[2]
    sentinels = {65535, 65536, -65535, -65536, 32767, 32768, -32768, -1}
    if x in sentinels or z in sentinels:
        return False
    if 65000 <= abs(x) <= 65600 or 65000 <= abs(z) <= 65600:
        return False
    # FFXI zone coords commonly live in the low hundreds when shown to users.
    # These event values often appear as centi-units.
    return (
        -200000 <= x <= 200000 and
        -200000 <= z <= 200000 and
        -50000 <= y <= 50000 and
        (abs(x) > 500 or abs(z) > 500)
    )


def find_npc_event_references(event_data, npc_record):
    rid = int(npc_record["record_id"])
    tag = int(npc_record["record_tag"])
    if rid == 0 or tag == 0:
        return []
    needle = struct.pack("<HH", rid, tag)
    hits = []
    start = 0
    while True:
        hit = event_data.find(needle, start)
        if hit < 0:
            break
        hits.append(hit)
        start = hit + 1
    return hits


def candidate_positions_for_reference(event_data, ref_ofs):
    candidates = []
    scan_start = ref_ofs + 12
    scan_end = min(len(event_data) - 12, ref_ofs + 192)
    for ofs in range(scan_start, scan_end, 4):
        vals = [read_s32(event_data, ofs + i * 4) for i in range(8)]
        if any(v is None for v in vals):
            continue
        if not looks_like_fixed_position(vals):
            continue

        x, z, y = vals[:3]
        heading = vals[3]
        if heading < 0 or heading > 4095:
            continue

        # Avoid obvious script offset tables: many small positive values in a row.
        small_positive = sum(1 for v in vals[:6] if 0 <= v < 0x4000)
        signed_mix = sum(1 for v in vals[:6] if v < 0)
        score = 50
        if 500 <= heading <= 3595:
            score += 15
        if signed_mix >= 1:
            score += 20
        if signed_mix >= 2:
            score += 20
        if looks_like_fixed_position(vals[4:7]):
            score += 15
        if small_positive >= 5:
            score -= 35
        if abs(x - z) < 5 and abs(x - y) < 5:
            score -= 25
        if any(v in {65535, 65536, -65535, -65536, 32767, 32768, -32768, -1} for v in vals[:4]):
            score -= 60
        if any(65000 <= abs(v) <= 65600 for v in vals[:4]):
            score -= 60
        if abs(x) < 1000 and abs(z) < 1000:
            score -= 20
        if abs(x) > 100000 or abs(z) > 100000:
            score -= 10
        candidates.append({
            "offset": ofs,
            "x": x / 100.0,
            "y": y / 100.0,
            "z": z / 100.0,
            "raw_x": x,
            "raw_y": y,
            "raw_z": z,
            "score": score,
            "raw_values": vals,
        })
    candidates.sort(key=lambda c: (-c["score"], c["offset"]))
    return candidates


def infer_npc_placements(npc_records, event_path):
    if not event_path or not os.path.exists(event_path):
        return []
    event_data = open(event_path, "rb").read()
    placements = []
    for npc in npc_records:
        if not npc["name"] or npc["name"].lower() == "none":
            continue
        refs = find_npc_event_references(event_data, npc)
        best = None
        for ref in refs:
            cands = candidate_positions_for_reference(event_data, ref)
            if cands:
                cand = cands[0].copy()
                cand["event_ref_offset"] = ref
                if best is None or (cand["score"], -cand["offset"]) > (best["score"], -best["offset"]):
                    best = cand
        if best:
            placements.append({
                "npc_index": npc["index"],
                "name": npc["name"],
                "record_id": npc["record_id"],
                "record_tag": npc["record_tag"],
                "event_ref_count": len(refs),
                "event_ref_offset": best["event_ref_offset"],
                "position_offset": best["offset"],
                "x": round(best["x"], 3),
                "y": round(best["y"], 3),
                "z": round(best["z"], 3),
                "confidence": best["score"],
                "raw_x": best["raw_x"],
                "raw_y": best["raw_y"],
                "raw_z": best["raw_z"],
                "raw_values": " ".join(str(v) for v in best["raw_values"]),
            })
    return placements


def zone_paths(root, zone):
    return {key: dat_path(root, zone[key]) if zone[key] else "" for key in ("model", "npc", "dialog", "event")}


def write_exports(out_dir, zone, paths, quiet=False):
    os.makedirs(out_dir, exist_ok=True)
    safe_name = re.sub(r"[^A-Za-z0-9_.-]+", "_", zone["name"]).strip("_") or f"zone_{zone['id']}"
    base = f"{zone['id']:03d}_{safe_name}"

    npc_records = read_npc_name_records(paths["npc"])
    npc_csv = os.path.join(out_dir, base + "_npc_names.csv")
    with open(npc_csv, "w", encoding="utf-8", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=["index", "offset", "name", "record_id", "record_tag", "raw_tail_hex"])
        writer.writeheader()
        writer.writerows(npc_records)

    tokens = event_tokens(paths["event"])
    event_csv = os.path.join(out_dir, base + "_event_tokens.csv")
    with open(event_csv, "w", encoding="utf-8", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=["offset", "token"])
        writer.writeheader()
        writer.writerows(tokens)

    placements = infer_npc_placements(npc_records, paths["event"])
    placement_csv = os.path.join(out_dir, base + "_npc_placement_candidates.csv")
    with open(placement_csv, "w", encoding="utf-8", newline="") as f:
        fieldnames = [
            "npc_index", "name", "record_id", "record_tag", "event_ref_count",
            "event_ref_offset", "position_offset", "x", "y", "z",
            "confidence", "raw_x", "raw_y", "raw_z", "raw_values",
        ]
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(placements)

    summary = {
        "zone": zone,
        "paths": paths,
        "npc_record_count": len(npc_records),
        "named_npc_count": sum(1 for r in npc_records if r["name"]),
        "event_token_count": len(tokens),
        "placement_candidate_count": len(placements),
        "npc_csv": npc_csv,
        "event_csv": event_csv,
        "placement_csv": placement_csv,
    }
    summary_json = os.path.join(out_dir, base + "_summary.json")
    with open(summary_json, "w", encoding="utf-8") as f:
        json.dump(summary, f, indent=2)

    if not quiet:
        print("\n[EXPORTS]")
        print(f"  NPC names : {npc_csv}")
        print(f"  event tags: {event_csv}")
        print(f"  placement : {placement_csv}")
        print(f"  summary   : {summary_json}")

    return summary, placements


def write_batch_exports(out_dir, zones, root):
    os.makedirs(out_dir, exist_ok=True)
    all_rows = []
    summaries = []

    for zone_id in sorted(zones):
        zone = zones[zone_id]
        paths = zone_paths(root, zone)
        if not paths["npc"] or not paths["event"]:
            continue
        if not os.path.exists(paths["npc"]) or not os.path.exists(paths["event"]):
            continue
        summary, placements = write_exports(out_dir, zone, paths, quiet=True)
        summaries.append(summary)
        for row in placements:
            out = {
                "zone_id": zone["id"],
                "zone_name": zone["name"],
                **row,
            }
            all_rows.append(out)

    all_csv = os.path.join(out_dir, "all_zone_npc_placement_candidates.csv")
    fieldnames = [
        "zone_id", "zone_name", "npc_index", "name", "record_id", "record_tag",
        "event_ref_count", "event_ref_offset", "position_offset", "x", "y", "z",
        "confidence", "raw_x", "raw_y", "raw_z", "raw_values",
    ]
    with open(all_csv, "w", encoding="utf-8", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(all_rows)

    high_csv = os.path.join(out_dir, "all_zone_npc_placement_candidates_conf90.csv")
    with open(high_csv, "w", encoding="utf-8", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows([r for r in all_rows if int(r["confidence"]) >= 90])

    batch_summary = {
        "zone_count": len(summaries),
        "placement_candidate_count": len(all_rows),
        "confidence_90_plus_count": sum(1 for r in all_rows if int(r["confidence"]) >= 90),
        "all_csv": all_csv,
        "confidence_90_plus_csv": high_csv,
    }
    summary_json = os.path.join(out_dir, "all_zone_npc_placement_summary.json")
    with open(summary_json, "w", encoding="utf-8") as f:
        json.dump(batch_summary, f, indent=2)

    print("\n[BATCH EXPORTS]")
    print(f"  all candidates : {all_csv}")
    print(f"  confidence 90+: {high_csv}")
    print(f"  summary        : {summary_json}")


def main():
    parser = argparse.ArgumentParser(description="Inspect zone NPC/dialog/event DATs from Zone_DATs_Table.csv.")
    parser.add_argument("zone", help="Zone ID to inspect, or 'all' for batch exports")
    parser.add_argument("--table", default=os.path.join("DATura", "Zone_DATs_Table.csv"))
    parser.add_argument("--ffxi-root", default=DEFAULT_FFXI_ROOT)
    parser.add_argument("--export-dir", default="", help="Optional output directory for NPC-name and event-token CSVs.")
    args = parser.parse_args()

    zones = read_zone_table(args.table)
    root = os.path.abspath(args.ffxi_root)

    if args.zone.lower() == "all":
        if not args.export_dir:
            raise SystemExit("--export-dir is required for batch mode")
        write_batch_exports(args.export_dir, zones, root)
        return

    zone_id = int(args.zone)
    zone = zones.get(zone_id)
    if not zone:
        raise SystemExit(f"Zone ID {zone_id} is not in {args.table}")

    print(f"Zone {zone['id']}: {zone['name']}")
    print(f"FFXI root: {root}")
    paths = zone_paths(root, zone)
    for key in ("model", "npc", "dialog", "event"):
        print(f"  {key:6}: {zone[key]}")

    describe_dat("MODEL", paths["model"])
    describe_dat("NPC", paths["npc"])
    describe_dat("DIALOG", paths["dialog"])
    describe_dat("EVENT", paths["event"])

    if args.export_dir:
        write_exports(args.export_dir, zone, paths)


if __name__ == "__main__":
    main()
