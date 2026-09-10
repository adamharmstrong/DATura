from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
from inspect_konschtat_mapgeo import parse_chunks, parse_textures, save_texture_previews

root = Path(r'C:\Program Files (x86)\PlayOnline\SquareEnix\FINAL FANTASY XI')
output = Path(__file__).parent / 'textures'
for relative, wanted in [('ROM/0/102.DAT', ['effect  umi0']), ('ROM/0/121.DAT', ['effect  kaw1'])]:
    chunks = list(parse_chunks((root / relative).read_bytes()))
    rows = parse_textures(chunks)
    selected = [row for row in rows if any(name in row['name'] for name in wanted)]
    print(relative, selected)
    save_texture_previews(rows, chunks, output, wanted)
