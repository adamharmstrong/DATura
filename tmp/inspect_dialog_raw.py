from pathlib import Path
import struct
p=Path(r'C:\Program Files (x86)\PlayOnline\SquareEnix\FINAL FANTASY XI\ROM\25\44.DAT')
b=bytearray(p.read_bytes())
if len(b)>3 and b[3]==0x10:
    for i in range(4,len(b)): b[i]^=0x80
first=struct.unpack_from('<I', b, 4)[0]
count=first//4
print('len',len(b),'first',first,'count',count)
for i in range(count):
    off=struct.unpack_from('<I', b,4+i*4)[0]
    end=struct.unpack_from('<I', b,4+(i+1)*4)[0] if i+1<count else len(b)-4
    raw=bytes(b[4+off:4+end])
    if b'Yes' in raw or b'No' in raw or b'Cancel' in raw or b'What' in raw or b'option' in raw:
        txt=''.join(chr(c) if 32<=c<127 else f'<{c:02X}>' for c in raw[:500])
        print(i, txt)
