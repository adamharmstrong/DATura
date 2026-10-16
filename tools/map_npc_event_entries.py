"""Export actor/event entry points from a retail event DAT (no branch guessing)."""
import argparse, csv, struct
from pathlib import Path
parser = argparse.ArgumentParser()
parser.add_argument("dat", type=Path)
parser.add_argument("output", type=Path)
args = parser.parse_args()
data = args.dat.read_bytes()
def u32(p): return struct.unpack_from("<I", data, p)[0]
def u16(p): return struct.unpack_from("<H", data, p)[0]
base = 0 if 0 < u32(0) <= 10000 else 4
count = u32(base)
pos = base + 4 + count * 4
with args.output.open("w", newline="") as stream:
    writer = csv.writer(stream)
    writer.writerow(["entity_id", "event_id", "entry_offset", "actor_code_size"])
    for index in range(count):
        length = u32(base + 4 + index * 4)
        entity, capacity = u32(pos), u32(pos + 4)
        refs = pos + 8 + capacity * 4
        code_size = u32(refs + 4 + u32(refs) * 4)
        for event in range(capacity):
            writer.writerow([entity, u16(pos + 8 + capacity * 2 + event * 2),
                             u16(pos + 8 + event * 2), code_size])
        pos += length
