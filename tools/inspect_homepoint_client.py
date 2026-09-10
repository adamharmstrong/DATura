"""Read-only disassembly helper for a locally unpacked FFXI client."""
import sys
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path[:0] = [str(ROOT / 'tmp/homepoint-analysis-deps'),
               str(ROOT / 'tmp/xi-test-client-review/tools')]
from dllstrings import load, where, va_to_off
from capstone import Cs, CS_ARCH_X86, CS_MODE_32

d, base, secs = load(str(ROOT / 'tmp/homepoint-client/FFXiMain.unpacked.dll'))
cs = Cs(CS_ARCH_X86, CS_MODE_32)

def dump(va, size):
    off = va_to_off(base, secs, va)
    for ins in cs.disasm(d[off:off + size], va):
        print(f'{ins.address:08x} {ins.mnemonic:9} {ins.op_str}')

if sys.argv[1] == 'dump':
    dump(int(sys.argv[2], 16), int(sys.argv[3], 0))
elif sys.argv[1] == 'xref':
    for arg in sys.argv[2:]:
        target = int(arg, 16)
        print('TARGET', hex(target))
        needle = struct.pack('<I', target)
        i = 0
        while True:
            i = d.find(needle, i)
            if i < 0:
                break
            sec, va = where(d, base, secs, i)
            print('XREF', sec, hex(va))
            if sec == '.text':
                dump(va - 16, 80)
            i += 1
elif sys.argv[1] == 'calls':
    target = int(sys.argv[2], 16)
    for name, rva, vs, rs, rp in secs:
        if name != '.text':
            continue
        for i in range(rs - 5):
            if d[rp+i] in (0xe8, 0xe9):
                va = base+rva+i
                dest = (va+5+struct.unpack_from('<i',d,rp+i+1)[0]) & 0xffffffff
                if dest == target:
                    print(hex(va), 'call' if d[rp+i] == 0xe8 else 'jmp')
elif sys.argv[1] == 'geometry':
    for name, rva, vs, rs, rp in secs:
        if name != '.text':
            continue
        cs.skipdata = True
        history = []
        for address, size, mnemonic, operands in cs.disasm_lite(d[rp:rp+vs], base+rva):
            history.append((address, mnemonic, operands))
            history = history[-12:]
            if mnemonic in ('add', 'lea') and ('0xe]' in operands or operands.endswith(', 0xe')):
                if any(m == 'shl' and op.endswith(', 4') for a,m,op in history):
                    for a,m,op in history: print(f'{a:08x} {m:9} {op}')
                    print()
elif sys.argv[1] == 'evidence':
    import contextlib
    output = ROOT / 'artifacts/home-point-static-inspection/client-excerpts.txt'
    with output.open('w', encoding='utf-8') as stream, contextlib.redirect_stdout(stream):
        print('Static excerpts from the locally unpacked client; addresses use preferred image base.')
        print('These are code observations, not a runtime execution trace.')
        for title, va, size in [
            ('Model bank resolver', 0x100c51d0, 142),
            ('Kind-zero model resource request', 0x100d3f2e, 38),
            ('Effect vertex pointer', 0x1003fcf0, 16),
            ('Effect material pointer', 0x1003fd00, 117),
            ('Effect vertex pointer after materials', 0x1003fd80, 44),
            ('Effect model vertex copy', 0x10043395, 95),
            ('Effect subclass selection', 0x10050f9e, 140),
            ('Specular effect constructor', 0x10041030, 45),
            ('Specular effect draw entry', 0x100412c0, 105),
            ('Primitive submission', 0x10043fee, 42),
            ('Device draw call', 0x1000cb59, 27),
        ]:
            print('\n' + title)
            dump(va, size)
    print(output)
