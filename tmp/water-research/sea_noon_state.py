import json,struct
from pathlib import Path
report=json.loads(Path(__file__).with_name('water_assets.json').read_text())['Valkurm Dunes']
curves={}
for resource in report['resources']:
    if resource['type']!='0x19':continue
    raw=bytes.fromhex(resource['hex']);pairs=[]
    for offset in range(0,len(raw)-7,8):
        time,value=struct.unpack_from('<ff',raw,offset);pairs.append((time,value))
        if time==1:break
    curves[(resource['path'],resource['name'])]=pairs
def value(pairs):
    for (t0,v0),(t1,v1) in zip(pairs,pairs[1:]):
        if t0<=.5<=t1:return v0+(v1-v0)*(.5-t0)/(t1-t0)
    return pairs[-1][1]
for g in report['generators']:
    if g.get('resource') not in ('umi0','umif','ukro'):continue
    links={};unknown=[]
    for op in g['streams'][1]:
        raw=bytes.fromhex(op['hex'])
        if op['op'] in ('60','61','62','63'):
            links[int(op['op'],16)-0x60]=raw[8:12].decode().rstrip('\0 ')
    color=int(g['color'],16)
    base=[((color>>16)&255)/128,((color>>8)&255)/128,(color&255)/128,((color>>24)&255)/64]
    values=[value(curves[(g['path'],links[i])]) if i in links else 1 for i in range(4)]
    state=[a*b for a,b in zip(base,values)];state[3]=min(state[3],1)
    batches=[r for r in report['resources'] if r.get('name')==g['resource'] and r['type']=='0x2e']
    print(g['path']+'/'+g['name'],g['resource'],'base',*[round(v,4) for v in base],'curve',*[round(v,4) for v in values],'state',*[round(v,4) for v in state], 'alphas',[(r['sub_index'],r['alpha_min'],r['alpha_max'])for r in batches])
