#!/usr/bin/env python3
"""Extract unchanged UI assets/tables; never execute the original game."""
from pathlib import Path
import hashlib,json,sys
source=Path(sys.argv[1] if len(sys.argv)>1 else 'reference-dos')
out=Path(sys.argv[2] if len(sys.argv)>2 else 'r20/assets');out.mkdir(parents=True,exist_ok=True)
b=(source/'B2.EXE').read_bytes()
assert hashlib.sha256(b).hexdigest()=='ecc25a150ec24d21aef46e50eace5ef363b11483aa768631464ac5febf89eddc','wrong corrected B2 reference'
names=['MAIN.EGS','BOUT1.EGS','BOUT2.EGS','BOUF1.EGS','BOUF2.EGS','IKONES.EGX','DICOGB.BIN']
items={name:(source/name).read_bytes() for name in names}
for name,data in items.items():
 assert len(data)==(6087 if name=='IKONES.EGX' else 2567 if name=='DICOGB.BIN' else 4008)
 assert data[0]==253
items['arrival-masks.bin']=b[0x149a:0x14b6]
items['destruction-tables.bin']=b[0x14b6:0x14e2]
items['globe-stream.bin']=b[0x1a30:0x233b]
items['ui-font.bin']=b[0x15c4:0x15c4+472]
items['save-template.bin']=b[0x673:0xaa4]+items['DICOGB.BIN'][7:263]+bytes(4096)
for name,data in items.items():(out/name).write_bytes(data)
manifest={name:{'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest()} for name,data in items.items()}
(out/'r32-ui-assets.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('Extracted',len(items),'UI assets from hash-checked original data.')
