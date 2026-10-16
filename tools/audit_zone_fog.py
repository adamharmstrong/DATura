"""Inventory authored 0x2F fog; reads retail DATs without modifying them."""
import argparse
import csv
import math
import struct
from pathlib import Path


def records(path):
    data = path.read_bytes()
    offset, directories = 0, []
    while offset + 16 <= len(data):
        info = struct.unpack_from('<I', data, offset + 4)[0]
        kind, size = info & 0x7f, (info >> 3) & 0x7ffff0
        if size < 16 or offset + size > len(data):
            raise ValueError(f'{path}: invalid chunk at {offset:#x}')
        name = data[offset:offset + 4].decode('ascii', errors='replace').rstrip('\0 ')
        if kind == 1 and name:
            directories.append(name)
        if kind == 0x2f and size >= 16 + 0x6c:
            payload = offset + 16
            far, near = struct.unpack_from('<ff', data, payload + 0x3c)
            color = struct.unpack_from('<I', data, payload + 0x38)[0]
            enabled = math.isfinite(near) and math.isfinite(far) and far > 0 and far > near
            yield '/'.join(directories), name, near, far, f'{color:08x}', enabled
        if kind == 0 and directories:
            directories.pop()
        offset += size


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game_root', type=Path)
    parser.add_argument('--output', type=Path, default=Path('artifacts/zone-fog'))
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    table = Path(__file__).resolve().parents[1] / 'DATura/Zone_DATs_Table.csv'
    details, summary = [], []
    with table.open(encoding='utf-8-sig', newline='') as source:
        for zone in csv.DictReader(source):
            model = zone['MODEL'].strip()
            if not model:
                continue
            path = args.game_root / model
            if not path.is_file():
                summary.append([zone['ID'], zone['NAME'], model, 'missing', 0, 0, '', ''])
                continue
            fog = list(records(path))
            active = [r for r in fog if r[-1]]
            summary.append([zone['ID'], zone['NAME'], model, 'scanned', len(fog), len(active),
                            min((r[3] for r in active), default=''),
                            max((r[3] for r in active), default='')])
            details.extend([zone['ID'], zone['NAME'], model, *r] for r in fog)
    for name, header, rows in [
        ('zones.csv', ['zone_id', 'zone', 'model', 'status', 'records', 'fog_records', 'min_far', 'max_far'], summary),
        ('records.csv', ['zone_id', 'zone', 'model', 'environment', 'time', 'near', 'far', 'packed_color', 'enabled'], details),
    ]:
        with (args.output / name).open('w', encoding='utf-8', newline='') as target:
            writer = csv.writer(target)
            writer.writerow(header)
            writer.writerows(rows)
    print(f'{len(summary)} zones; {sum(r[5] > 0 for r in summary)} with authored fog; '
          f'{sum(r[3] == "missing" for r in summary)} missing DATs; {len(details)} environment records')


if __name__ == '__main__':
    main()
