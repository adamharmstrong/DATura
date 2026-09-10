import struct, sys, json
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
from inspect_ffxi_weather_generators import iter_chunks
from inspect_konschtat_mapgeo import decrypt_mmb

root = Path(r'C:\Program Files (x86)\PlayOnline\SquareEnix\FINAL FANTASY XI')
chunks = iter_chunks((root / 'ROM/0/102.DAT').read_bytes())
all_batches = []
for _, name, kind, path, payload in chunks:
    if kind != 0x2e or name not in ('umi0', 'umif', 'ukro') or path != 'f_ki/effe': continue
    data = decrypt_mmb(payload)
    cursor = 32
    supercount = struct.unpack_from('<i', data, cursor)[0]
    cursor += 32
    for _ in range(supercount):
        subcount = struct.unpack_from('<i', data, cursor)[0]
        cursor += 32
        for batch in range(subcount):
            material = data[cursor:cursor+16]
            packed, = struct.unpack_from('<I', data, cursor+16)
            n = packed & 0x0fffffff
            cursor += 20
            stride = 48 if data[4] & 2 else 36
            vertices = [struct.unpack_from('<3f', data, cursor+i*stride) for i in range(n)]
            uv = [struct.unpack_from('<2f', data, cursor+i*stride+stride-8) for i in range(n)]
            alpha = [data[cursor+i*stride+stride-9] for i in range(n)]
            cursor += n*stride
            count, flags = struct.unpack_from('<HH', data, cursor)
            cursor += 4
            indices = struct.unpack_from(f'<{count}H',data,cursor)
            cursor = (cursor+count*2+3)&~3
            signs = {-1:0, 0:0, 1:0}
            tris=[]
            for i in range(count-2):
                triangle = [indices[i],indices[i+1],indices[i+2]]
                if i & 1: triangle[1],triangle[2]=triangle[2],triangle[1]
                a,b,c=[vertices[v] for v in triangle]
                signed = (b[2]-a[2])*(c[0]-a[0])-(b[0]-a[0])*(c[2]-a[2])
                sign = 1 if signed>1e-9 else -1 if signed < -1e-9 else 0
                signs[sign]+=1
                if sign: tris.append(triangle)
            out={'name':name,'batch':batch,'data4':data[4], 'signs':signs,'vertices':vertices, 'uv':uv,'alpha':alpha,'triangles':tris}
            all_batches.append(out)
            print(name,batch,'orientation Y',signs,'first UV',uv[:8], 'alpha',alpha[:8])
Path(__file__).with_name('sea_topology.json').write_text(json.dumps(all_batches))

# Local XZ coverage and overlap; water is planar so this tests actual polygons.
from PIL import Image, ImageDraw
def inside(x,z, points):
    signs=[]
    for a,b in zip(points,points[1:]+points[:1]):
        signs.append((b[0]-a[0])*(z-a[2])-(b[2]-a[2])*(x-a[0]))
    return min(signs)>1e-7 or max(signs)<-1e-7
for name in ('umi0','umif','ukro'):
    batches=[b for b in all_batches if b['name']==name]
    p=[[ [b['vertices'][i] for i in t] for t in b['triangles']] for b in batches]
    counts=[]
    for source in (0,1):
        overlaps=0
        for tri in p[source]:
            x=sum(v[0] for v in tri)/3; z=sum(v[2] for v in tri)/3
            overlaps+=any(inside(x,z,q) for q in p[1-source])
        counts.append(overlaps)
    print(name,'centroid overlap counts',counts,'of',[len(q) for q in p])
    xs=[v[0] for b in batches for v in b['vertices']]; zs=[v[2] for b in batches for v in b['vertices']]
    loX,hiX,loZ,hiZ=min(xs),max(xs),min(zs),max(zs)
    sx=750/(hiX-loX); sz=350/(hiZ-loZ)
    image=Image.new('RGB',(800,400),'white'); draw=ImageDraw.Draw(image)
    for b,triangles in zip(batches,p):
        for tri,indices in zip(triangles,b['triangles']):
            alpha=sum(b['alpha'][i] for i in indices)/3/128
            color=(40,100,180) if b['batch']==0 else (int(255*(1-alpha)),int(255*(1-alpha/2)),255)
            pts=[(25+(v[0]-loX)*sx,375-(v[2]-loZ)*sz) for v in tri]
            draw.polygon(pts,fill=color,outline=(160,160,160))
    draw.text((10,8),f'{name}: dark = batch0 alpha128, light = batch1 mean vertex alpha',fill='black')
    image.save(Path(__file__).with_name(name+'_topology.png'))

report=json.loads(Path(__file__).with_name('water_assets.json').read_text())
group=[g for g in report['Valkurm Dunes']['generators'] if g['path']=='f_ki/effe/umi1' and g.get('resource') in ('umi0','umif')]
world=[]
for g in group:
    tris=[]
    for b in all_batches:
        if b['name']!=g['resource']:continue
        for t in b['triangles']:
            tri=[[v[0]*g['scale'][0]+g['position'][0],g['position'][1],v[2]*g['scale'][2]+g['position'][2]] for v in [b['vertices'][i] for i in t]]
            tris.append(tri)
    world.append((g['name'],tris))
for name, source in world:
    for other, target in world:
        if name==other:continue
        boxes=[(min(v[0] for v in t),max(v[0] for v in t),min(v[2] for v in t),max(v[2] for v in t),t) for t in target]
        overlap=0
        for tri in source:
            x=sum(v[0] for v in tri)/3;z=sum(v[2] for v in tri)/3
            overlap+=any(loX<x<hiX and loZ<z<hiZ and inside(x,z,t) for loX,hiX,loZ,hiZ,t in boxes)
        print('WORLD CENTROIDS',name,'in',other,overlap,'/',len(source))
