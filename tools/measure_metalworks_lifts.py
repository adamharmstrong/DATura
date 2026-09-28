"""Report vertex height concentrations for the unrotated Metalworks lift assets."""
import struct
import sys
from collections import Counter
from pathlib import Path
import inspect_zone_collision as dat

chunks = list(dat.parse_chunks(Path(sys.argv[1]).read_bytes()))
objects = next(dat.parse_map_objects(c['payload']) for c in chunks
               if c['type'] == dat.CHUNK_MAP)
for chunk in chunks:
    if chunk['type'] != dat.CHUNK_MAPGEO:
        continue
    body = dat.decrypt_mmb(chunk['payload'])
    name = dat.c_name(body[16:32])
    if name.rstrip() not in ('liftall', 'liftallb', 'liftbase', 'liftbaseb',
                             'liftstep', 'liftstepb'):
        continue
    placements = [(i, p) for i, p in enumerate(objects) if p[0] == name
                  and abs(p[1][0] + 56) < 20 and abs(p[1][2]) < 30]
    if not placements:
        continue
    offset = 32
    vertices = []
    while offset + 32 <= len(body):
        count = struct.unpack_from('<i', body, offset)[0]
        offset += 32
        if not 0 <= count <= 4096:
            break
        for _ in range(count):
            subcount = struct.unpack_from('<i', body, offset)[0]
            offset += 32
            for _ in range(subcount):
                packed = struct.unpack_from('<I', body, offset + 16)[0]
                n = packed & 0xFFFF
                stride = 48 if body[4] & 2 else 36
                offset += 20
                vertices.extend(struct.unpack_from('<fff', body, offset + j * stride)
                                for j in range(n))
                offset += n * stride
                indices = struct.unpack_from('<H', body, offset)[0]
                offset = (offset + 4 + indices * 2 + 3) & ~3
    for index, placement in placements:
        position, scale = placement[1:3]
        heights = Counter(round(v[1] * scale[1] + position[1], 3) for v in vertices
                          if abs(v[0] * scale[0] + position[0] + 56) < 8
                          and abs(abs(v[2] * scale[2] + position[2]) - 12) < 6)
        if heights:
            print(index, repr(name), 'origin=', position, 'heights=', heights.most_common(8))
