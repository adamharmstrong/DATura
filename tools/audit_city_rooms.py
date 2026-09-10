"""Audit city room ownership from MZB placements, RID triggers, and file tables.

Reads the installation without modifying it; emits the evidence as JSON.
"""
import argparse
import json
import re
import struct
from pathlib import Path
CHUNK_HEADER_SIZE = 16
def parse_key_table(header_text, table_name):
    match = re.search(rf"{table_name}\[256\]\s*=\s*\{{(.*?)\}};", header_text, re.S)
    if not match:
        raise RuntimeError(f"could not find {table_name}")
    return [int(x, 16) for x in re.findall(r"0x[0-9A-Fa-f]{2}", match.group(1))]


SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parent
DECRYPT_HEADER = REPO_ROOT / "DATura" / "model_ff11_decrypt.h"
_header = DECRYPT_HEADER.read_text(encoding="utf-8", errors="ignore")
KEY_TABLE = parse_key_table(_header, "skKeyTable")


def c_name(raw):
    raw = raw.split(b"\0", 1)[0]
    return "".join(chr(b) if 32 <= b < 127 else "?" for b in raw)


def u24le(data, off):
    return data[off] | (data[off + 1] << 8) | (data[off + 2] << 16)


def decrypt_object_map(buf):
    if len(buf) < 8 or buf[3] < 0x1B:
        return buf
    out = bytearray(buf)
    decode_len = min(u24le(out, 0), len(out))
    key = KEY_TABLE[out[7] ^ 0xFF]
    key_counter = 0
    pos = 8
    while pos < decode_len:
        xor_len = ((key >> 4) & 7) + 16
        if (key & 1) and (pos + xor_len < decode_len):
            for i in range(xor_len):
                out[pos + i] ^= 0xFF
        key_counter += 1
        key += key_counter
        pos += xor_len

    node_count = u24le(out, 4)
    node_stride = 100
    max_nodes = max(0, (len(out) - 32) // node_stride)
    for i in range(min(node_count, max_nodes)):
        off = 32 + i * node_stride
        for j in range(16):
            out[off + j] ^= 0x55
    return bytes(out)



def parse_chunks(data):
    off = 0
    while off <= len(data) - CHUNK_HEADER_SIZE:
        name = data[off:off + 4]
        info = struct.unpack_from("<I", data, off + 4)[0]
        chunk_type = info & 0x7F
        chunk_size = (info >> 3) & 0x7FFFF0
        if chunk_size <= 0 or off + chunk_size > len(data):
            break
        yield {
            "offset": off,
            "name": c_name(name),
            "type": chunk_type,
            "size": chunk_size,
            "payload": data[off + CHUNK_HEADER_SIZE:off + chunk_size],
        }
        off += chunk_size




CITY_IDS = set(range(230, 251)) | {252, 26, 48, 50, 53, 256, 257, 80, 87, 94}


def load_tables(root):
    tables = []
    for bank in range(1, 20):
        suffix = '' if bank == 1 else str(bank)
        directory = root if bank == 1 else root / ('ROM' + suffix)
        vtable = directory / ('VTABLE' + suffix + '.DAT')
        ftable = directory / ('FTABLE' + suffix + '.DAT')
        if vtable.exists() and ftable.exists():
            tables.append((bank, vtable.read_bytes(), ftable.read_bytes()))
    return tables


def resolve(file_id, tables):
    for bank,v,f in tables:
        if file_id<len(v) and v[file_id]==bank and file_id*2+2<=len(f):
            packed=struct.unpack_from('<H',f,file_id*2)[0]
            return ('ROM' if bank==1 else 'ROM'+str(bank))+f'/{packed//128}/{packed%128}.DAT'

def triggers(path):
    found=set()
    for chunk in parse_chunks(path.read_bytes()):
        data=chunk['payload']
        if not data.startswith(b'RID') or len(data)<20:continue
        offset=struct.unpack_from('<I',data,16)[0]
        if offset+16>len(data):continue
        count=struct.unpack_from('<I',data,offset)[0]
        for i in range(min(count,(len(data)-offset-16)//64)):
            entry=offset+16+i*64
            if data[entry+36:entry+37]==b'm':
                param=struct.unpack_from('<I',data,entry+44)[0]
                if param:found.add(param)
    return found

def links(path):
    found={}
    for chunk in parse_chunks(path.read_bytes()):
        if chunk['type']!=0x1c: continue
        data=decrypt_object_map(chunk['payload'])
        if len(data) < 32:
            continue
        count=min(u24le(data,4),(len(data)-32)//100)
        for i in range(count):
            off=32+100*i
            link=struct.unpack_from('<I',data,off+0x50)[0]
            if link:
                found.setdefault(link,[]).append(data[off:off+16].decode('ascii',errors='replace').strip())
    return found

def audit(root):
    tables = load_tables(root)
    rows = []
    text = (REPO_ROOT / 'DATura/zone_dat_table.h').read_text()
    for match in re.finditer(r'\{\s*(\d+),\s*"([^"]+)",\s*"([^"]+)"', text):
        zone_id, name, path = match.groups()
        if int(zone_id) not in CITY_IDS:
            continue
        entries = []
        placements = links(root / path)
        room_triggers = triggers(root / path)
        for link in sorted(set(placements) | room_triggers):
            objects = placements.get(link, [])
            offset = 100 if link < 0x271 else 0x14768 - 0x271
            candidate = resolve(link + offset, tables)
            header = b''
            if candidate and (root / candidate).exists():
                with (root / candidate).open('rb') as stream:
                    header = stream.read(16)
            entries.append(dict(subAreaId=link, path=candidate,
                                root=header[:4].decode('ascii', errors='replace'),
                                trigger=link in room_triggers, objects=objects))
        rows.append(dict(zone=int(zone_id), name=name, path=path, links=entries))
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('installation', type=Path)
    parser.add_argument('--output', type=Path, help='Write JSON here instead of stdout')
    args = parser.parse_args()
    result = json.dumps(audit(args.installation), indent=2) + '\n'
    if args.output:
        args.output.write_text(result, encoding='utf-8')
    else:
        print(result, end='')


if __name__ == '__main__':
    main()
