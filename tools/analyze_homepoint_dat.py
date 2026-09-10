"""Inspect the installed Home Point effect without running the game.

Writes metadata and a wireframe SVG; never modifies installed files.
"""
import argparse
from collections import Counter
import hashlib
import json
import math
from pathlib import Path
import re
import struct
from export_ffxi_effect_chunks import iter_chunks

ap = argparse.ArgumentParser()
ap.add_argument('installation', type=Path)
ap.add_argument('--out', type=Path, default=Path('artifacts/home-point-static-inspection'))
args = ap.parse_args()
args.out.mkdir(parents=True, exist_ok=True)
v = (args.installation / 'VTABLE.DAT').read_bytes()
f = (args.installation / 'FTABLE.DAT').read_bytes()
logical = 1300 + 0x33
physical = struct.unpack_from('<H', f, logical * 2)[0]
rom = v[logical]
relative = Path('ROM' if rom == 1 else f'ROM{rom}') / str(physical >> 7) / f'{physical & 127}.DAT'
data = (args.installation / relative).read_bytes()
chunks = list(iter_chunks(data))
result = dict(appearance_id=0x33, logical_id=logical, path=relative.as_posix(),
              sha256=hashlib.sha256(data).hexdigest(),
              chunk_counts=dict(Counter(hex(t) for _,_,_,t,_ in chunks)),
              meshes=[], generators=[], schedules=[])
svg = ['<svg xmlns="http://www.w3.org/2000/svg" width="1000" height="620" viewBox="0 0 1000 620">',
       '<rect width="1000" height="620" fill="#101925"/>',
       '<text x="30" y="36" fill="white" font-family="sans-serif" font-size="22">Home Point DAT — stored geometry, without textures or animation</text>']
for _, offset, name, kind, payload in chunks:
    if kind == 0x1f:
        marker, images, secondary, triangles = struct.unpack_from('<IBBH', payload)
        if marker != 6:
            continue
        groups = images + secondary
        # Retail 1003FD00: group count words, padded by its observed branch.
        material_offset = 8 + (groups * 2 if groups % 4 == 0 else (groups & ~3) * 2 + 6)
        vertex_offset = material_offset + images * 16
        assert vertex_offset + triangles * 3 * 36 <= len(payload)
        vertices = [struct.unpack_from('<3f', payload, vertex_offset + i * 36) for i in range(triangles * 3)]
        assert all(math.isfinite(c) and abs(c) < 10000 for xyz in vertices for c in xyz)
        material = payload[material_offset:material_offset + 16].decode('latin1')
        result['meshes'].append(dict(name=name, offset=hex(offset), triangles=triangles,
            vertex_offset=vertex_offset, material=material,
            bounds=[[min(p[a] for p in vertices), max(p[a] for p in vertices)] for a in range(3)]))
        panel = len(result['meshes']) - 1
        ox, oy = 105 + panel * 195, 505
        for i in range(0, len(vertices), 3):
            points = ' '.join(f'{ox + (x + z * .48) * 125:.2f},{oy + (y - z * .2) * 155:.2f}' for x,y,z in vertices[i:i+3])
            svg.append(f'<polygon points="{points}" fill="#73caff" fill-opacity=".025" stroke="#73caff" stroke-opacity=".55" stroke-width=".7"/>')
        svg.append(f'<text x="{ox-50}" y="570" fill="white" font-family="sans-serif" font-size="17">{name}: {triangles} triangles</text>')
    elif kind == 5:
        record = dict(name=name, offset=hex(offset), interval_field=struct.unpack_from('<H',payload,0x66)[0], commands=[])
        starts = [struct.unpack_from('<I',payload,0x70+i*4)[0]-16 for i in range(4)] + [len(payload)]
        assert starts == sorted(starts) and starts[0] >= 0 and starts[-1] <= len(payload)
        for stream in range(4):
            cursor, end = starts[stream:stream+2]
            while cursor+4 <= end:
                config = struct.unpack_from('<I',payload,cursor)[0]
                opcode, size = config & 255, max(1,(config >> 8)&31)*4
                assert cursor+size <= end
                if opcode:
                    record['commands'].append(dict(stream=stream, opcode=hex(opcode), offset=hex(cursor), bytes=payload[cursor:cursor+size].hex()))
                if stream == 1 and opcode == 1 and size >= 36:
                    record.update(resource=payload[cursor+12:cursor+16].decode('latin1'),
                        flags=hex(struct.unpack_from('<I',payload,cursor+4)[0]),
                        resource_type=hex(payload[cursor+33]), lifetime_field=struct.unpack_from('<H',payload,cursor+34)[0])
                cursor += size
        result['generators'].append(record)
    elif kind == 7:
        result['schedules'].append(dict(name=name, offset=hex(offset),
            printable_references=[dict(offset=hex(m.start()),name=m.group().decode()) for m in re.finditer(rb'[ -~]{4,}',payload)]))
svg.append('</svg>')
(args.out/'geometry.svg').write_text('\n'.join(svg),encoding='utf-8')
(args.out/'verified-dat.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
print(f"Verified {relative}: {len(result['meshes'])} effect meshes, {sum(m['triangles'] for m in result['meshes'])} triangles; {len(result['generators'])} generators.")
