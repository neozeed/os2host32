"""Extract original speaker data once. Not needed to build or play."""
from pathlib import Path
import hashlib,json,sys
source=Path(sys.argv[1] if len(sys.argv)>1 else 'reference-dos')
out=Path(sys.argv[2] if len(sys.argv)>2 else 'r20/assets');out.mkdir(parents=True,exist_ok=True)
b=(source/'B2.EXE').read_bytes()
assert hashlib.sha256(b).hexdigest()=='ecc25a150ec24d21aef46e50eace5ef363b11483aa768631464ac5febf89eddc','wrong corrected B2'
programs=(source/'SOUND.BIN').read_bytes();assert len(programs)==1623 and programs[0]==253
items={'SOUND.BIN':programs,'speaker-tables.bin':b[0x542:0x57e]+b[0x59c:0x5be]+b[0x5e0:0x660]}
for name,data in items.items():(out/name).write_bytes(data)
(out/'r32-speaker-assets.json').write_text(json.dumps({name:{'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest()} for name,data in items.items()},indent=2)+'\n')
print('Extracted original SOUND.BIN and speaker tables.')
