"""Read-only DAT probe: writes summaries only next to this script."""
import json
import struct
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "tools"))
from inspect_ffxi_weather_generators import iter_chunks, fourcc, u16, u32
from scan_ffxi_effect_chunks import parse_type25, parse_type21
from inspect_konschtat_mapgeo import decrypt_mmb, iter_mapgeo_segments

ROOT = Path(r"C:\Program Files (x86)\PlayOnline\SquareEnix\FINAL FANTASY XI")
ZONES = {"East Ronfaure": "ROM/0/121.DAT", "Bastok Markets": "ROM/1/35.DAT", "Valkurm Dunes": "ROM/0/102.DAT"}

def floats(data, offset, count=1):
    return [round(x, 7) for x in struct.unpack_from("<" + "f" * count, data, offset)]

def streams(data):
    starts = [u32(data, 0x70 + i * 4) - 16 for i in range(4)]
    result = []
    for stream, start in enumerate(starts):
        end = min([x for x in starts if x > start] + [len(data)])
        entries = []
        cursor = start
        while cursor >= 0 and cursor + 4 <= end:
            config = u32(data, cursor)
            words = (config >> 8) & 31
            size = max(1, words) * 4
            if cursor + size > end:
                break
            raw = data[cursor:cursor + size]
            entries.append({"op": f"{config & 255:02x}", "offset": cursor, "slot": (config >> 13) & 63,
                            "hex": raw.hex(" "), "floats": floats(raw, 4, words - 1) if words > 1 else []})
            cursor += size
            if words == 0:
                break
        result.append(entries)
    return result

def main():
    report = {}
    for zone, relative in ZONES.items():
        data = (ROOT / relative).read_bytes()
        chunks = list(iter_chunks(data))
        resources = []
        for offset, name, kind, directory, payload in chunks:
            if kind == 0x2e and "/effe" in directory:
                try:
                    for segment in iter_mapgeo_segments(decrypt_mmb(payload)):
                        resources.append({"path": directory, "name": name, "type": hex(kind), "offset": hex(offset), **segment})
                except (struct.error, ValueError):
                    resources.append({"path": directory, "name": name, "type": hex(kind), "offset": hex(offset), "error": "mapgeo probe failed"})
            if kind in (0x21, 0x25):
                info = parse_type25(payload) if kind == 0x25 else parse_type21(payload)
                resources.append({"path": directory, "name": name, "type": hex(kind), "offset": hex(offset), **info})
            elif kind == 0x19 and "/weat/" not in directory:
                resources.append({"path": directory, "name": name, "type": hex(kind), "offset": hex(offset), "hex": payload.hex(" ")})
        generators = []
        for offset, name, kind, directory, payload in chunks:
            if kind != 5 or len(payload) < 0x80 or "/weat/" in directory:
                continue
            parsed = streams(payload)
            entry = {"path": directory, "name": name, "offset": hex(offset), "attach": hex(u16(payload, 0)),
                     "generator_flags": hex(payload[0x69]), "more_flags": hex(payload[0x6b]),
                     "interval": u16(payload, 0x66) + 1, "count": payload[0x68], "streams": parsed}
            for op in parsed[1]:
                raw = bytes.fromhex(op["hex"])
                if op["op"] == "01" and len(raw) >= 36:
                    entry.update(resource=fourcc(raw[12:16]), flags=hex(u32(raw, 4)),
                                 position=floats(raw, 20, 3), lifetime=u16(raw, 34), datatype=raw[33])
                if op["op"] in ("09", "0f") and len(raw) >= 16:
                    entry["rotation" if op["op"] == "09" else "scale"] = floats(raw, 4, 3)
                if op["op"] == "16" and len(raw) >= 8:
                    entry["color"] = hex(u32(raw, 4))
                if op["op"] == "1e" and len(raw) >= 8:
                    entry["blend"] = hex(u16(raw, 4))
            generators.append(entry)
        report[zone] = {"dat": relative, "generators": generators, "resources": resources}
    output = Path(__file__).with_name("water_assets.json")
    output.write_text(json.dumps(report, indent=2, allow_nan=True), encoding="utf-8")
    for zone, info in report.items():
        print(zone, info["dat"], "generators", len(info["generators"]))
        for g in info["generators"]:
            if "resource" not in g or not any(part in g["path"] for part in ("/effe/kawa", "/effe/sea", "/effe/funs", "/effe/umi")):
                continue
            print(f"  {g['path']}/{g['name']} -> {g['resource']} pos={g['position']} scale={g.get('scale')} flags={g.get('flags')} gen={g['generator_flags']}/{g['more_flags']} blend={g.get('blend')} life={g['lifetime']}")
    print(output)

if __name__ == "__main__":
    main()
