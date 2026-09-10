"""Synchronize the editable role catalog with placements; never guess NPC roles."""
import argparse
import csv
import re
from collections import Counter
from datetime import date
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FIELDS = 'zone_id zone_name entity_id npc_name title status source_url reviewed_on notes'.split()


def read_csv(path):
    with path.open(encoding='utf-8-sig', newline='') as stream:
        return list(csv.DictReader(line for line in stream if not line.startswith('#')))


def key(row):
    return int(row['zone_id']), int(row['entity_id'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sync', action='store_true', help='Add missing placements, preserving existing research')
    args = parser.parse_args()
    placements = read_csv(ROOT / 'DATura/npc_placements.csv')
    source = {key(row): row for row in placements}
    assert len(source) == len(placements), 'Duplicate placement identity'
    path = ROOT / 'DATura/npc_roles.csv'
    rows = read_csv(path) if path.exists() else []
    existing = {key(row): row for row in rows}
    assert len(existing) == len(rows), 'Duplicate role identity'
    zones = dict(re.findall(r'\{\s*(\d+),\s*"([^"]+)"',
                           (ROOT / 'DATura/zone_dat_table.h').read_text()))
    if args.sync:
        for identity, placement in source.items():
            if identity not in existing:
                row = dict.fromkeys(FIELDS, '')
                row.update(zone_id=str(identity[0]), zone_name=zones.get(str(identity[0]), ''),
                           entity_id=str(identity[1]), npc_name=placement['name'], status='pending')
                if placement['name'] in ('', 'blank', '???') or placement['name'].startswith('Door:'):
                    row.update(status='not_applicable', notes='Unnamed placeholder or door; no NPC role label.')
                existing[identity] = row
        rows = sorted(existing.values(), key=key)
    for row in rows:
        identity = key(row)
        assert set(row) == set(FIELDS), f'Unexpected columns: {identity}'
        assert identity in source, f'Removed placement needs review: {identity}'
        assert row['npc_name'] == source[identity]['name'], f'Renamed NPC needs review: {identity}'
        assert row['status'] in ('pending', 'verified', 'not_applicable'), f'Invalid status: {identity}'
        assert all(not any(c in value for c in '\r\n\t') for value in row.values()), f'Use single-line fields: {identity}'
        assert len(row['title'].encode('utf-8')) <= 128, f'Title too long: {identity}'
        if row['status'] == 'verified':
            assert row['title'].strip(), f'Missing title: {identity}'
            assert row['source_url'].startswith(('https://', 'http://')), f'Missing source: {identity}'
            date.fromisoformat(row['reviewed_on'])
    assert set(existing) == set(source), 'Missing placements; run with --sync'
    if args.sync:
        with path.open('w', encoding='utf-8', newline='') as stream:
            writer = csv.DictWriter(stream, FIELDS, lineterminator='\n')
            writer.writeheader()
            writer.writerows(rows)
    print(f'Validated {len(rows)} entries: {dict(Counter(row["status"] for row in rows))}')
    for zone in (234, 235, 236, 237):
        selected = [row for row in rows if int(row['zone_id']) == zone]
        print(f'{zones[str(zone)]}: {dict(Counter(row["status"] for row in selected))}')


if __name__ == '__main__':
    main()
