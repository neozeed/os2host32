#!/usr/bin/env python3
"""Render exact palette-indexed framebuffer captures from the SDL test harness."""
from pathlib import Path
from PIL import Image
pal=[(0,0,0),(0,0,170),(0,170,0),(0,170,170),(170,0,0),(170,0,170),(170,85,0),(170,170,170),(85,85,85),(85,85,255),(85,255,85),(85,255,255),(255,85,85),(255,85,255),(255,255,85),(255,255,255)]
out=Path(__file__).resolve().parent/'evidence'
def frame(data,scale):
 im=Image.frombytes('P',(320,200),data);im.putpalette([v for c in pal for v in c])
 return im.resize((320*scale,200*scale),Image.Resampling.NEAREST)
for name in ['orbit','flight','encounter','landscape','bridge','scan-low','scan-high','galaxy-return','galaxy-warp','galaxy-hover','arrival-full','destroyed-orbit']:
 frame((out/f'r32-{name}.raw').read_bytes(),3).save(out/f'r32-{name}.png')
b=(out/'r32-ui-frames.raw').read_bytes();assert len(b)%64000==0
frames=[frame(b[i:i+64000],2) for i in range(0,len(b),64000)]
frames[0].save(out/'R32-UpCom-preview.gif',save_all=True,append_images=frames[1:],duration=150,loop=0,optimize=False)
print('Rendered',len(frames),'native frames; no screenshot emulation or generated artwork.')

for name,duration in [('oorx',90),('globe',90)]:
 p=out/f'r32-{name}-frames.raw'
 if p.exists():
  b=p.read_bytes();assert len(b)%64000==0
  frames=[frame(b[i:i+64000],2) for i in range(0,len(b),64000)]
  frames[0].save(out/f'R32-{name}-preview.gif',save_all=True,append_images=frames[1:],duration=duration,loop=0,optimize=False)

frames=[];durations=[]
sequence=[('galaxy-hover',900,1),('galaxy-warp',900,1),('warp-frames',60,2),
          ('arrival-frames',60,2),('arrival-full',700,1),('destroy-frames',120,4),
          ('destroyed-orbit',700,1),('transition-frames',90,1),
          ('collision-frames',60,1),('static-frames',60,2),('mouth-frames',90,1)]
for name,duration,stride in sequence:
 b=(out/f'r32-{name}.raw').read_bytes();assert len(b)%64000==0
 count=len(b)//64000
 # A single collision/pause is enough to demonstrate the seven identical tests.
 if name=='collision-frames':count=min(count,34)
 for i in range(0,count,stride):frames.append(frame(b[i*64000:(i+1)*64000],2));durations.append(duration)
frames[0].save(out/'R32-UI-preview.gif',save_all=True,append_images=frames[1:],duration=durations,loop=0,optimize=False)
print('R32 effects preview:',len(frames),'native captures')
