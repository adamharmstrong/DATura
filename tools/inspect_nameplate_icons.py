"""Export locally installed UI atlases for nameplate icon verification."""
import argparse
import io
import json
import struct
from pathlib import Path
from PIL import Image

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('root', type=Path)
parser.add_argument('output', type=Path)
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)
data = (args.root / 'ROM/119/51.DAT').read_bytes()
offset = 0
shapes = []
while offset + 16 <= len(data):
    info = struct.unpack_from('<I', data, offset + 4)[0]
    size = (info >> 3) & 0x7ffff0
    if size < 16 or offset + size > len(data):
        break
    chunk = data[offset:offset+size]
    if info & 127 == 32 and size >= 85:
        name = chunk[17:33].decode('ascii', errors='replace').strip()
        if name in ('menu    ustatshd', 'menu    menu2fon', 'font    font'):
            width, height = struct.unpack_from('<ii', chunk, 37)
            fourcc = chunk[73:77][::-1]
            header = struct.pack('<7I', 124, 0x81007, height, width, width*height, 0, 0)
            header += bytes(44) + struct.pack('<II4s5I', 32, 4, fourcc, 0, 0, 0, 0, 0)
            header += struct.pack('<5I', 4096, 0, 0, 0, 0)
            image = Image.open(io.BytesIO(b'DDS '+header+chunk[85:])).convert('RGBA')
            image.save(args.output / (name.split()[-1] + '.png'))
    if info & 127 == 49 and chunk[16:32] == b'font    fontshp ':
        # Record all texture-qualified quads for inspection, with their raw offsets.
        for p in range(33, size - 59):
            if chunk[p:p+16].lower() not in (b'menu    ustatshd', b'font    font    '):
                continue
            if chunk[p+16] != 1:
                continue
            xy = struct.unpack_from('<8h', chunk, p+17)
            w, h, x, y = struct.unpack_from('<4H', chunk, p+33)
            if 0 < w <= 256 and 0 < h <= 256 and x+w <= 256 and y+h <= 256:
                shapes.append(dict(offset=p, texture=chunk[p:p+16].decode('ascii'),
                                   vertices=xy, rect=[x,y,w,h]))
    offset += size
(args.output / 'shapes.json').write_text(json.dumps(shapes, indent=2))
print(f'Exported atlases and {len(shapes)} quads to {args.output}')
