"""Render native captures made by r32/tests/check.c. No generated artwork.
The preview deliberately slows page alternation to make missile squares visible.
"""
from pathlib import Path
from PIL import Image
out=Path(__file__).resolve().parent/'evidence'
pal=[(0,0,0),(0,0,170),(0,170,0),(0,170,170),(170,0,0),(170,0,170),(170,85,0),(170,170,170),(85,85,85),(85,85,255),(85,255,85),(85,255,255),(255,85,85),(255,85,255),(255,255,85),(255,255,255)]
def frame(b):
 im=Image.frombytes('P',(320,200),b);im.putpalette([v for c in pal for v in c])
 return im.resize((640,400),Image.Resampling.NEAREST)
for name in ['howdy-hover','reply-cleared','missiles-low','missiles-high']:
 frame((out/f'r32-{name}.raw').read_bytes()).save(out/f'r32-{name}.png')
frames=[];durations=[]
b=(out/'r32-conversation-frames.raw').read_bytes();assert len(b)%64000==0
for i in range(0,len(b),64000):frames.append(frame(b[i:i+64000]));durations.append(1000)
b=(out/'r32-missile-frames.raw').read_bytes();assert len(b)==256000
for zoom in range(2):
 for i in range(20):
  start=(zoom*2+i%2)*64000
  frames.append(frame(b[start:start+64000]));durations.append(120)
frames[0].save(out/'R32-conversation-missiles-preview.gif',save_all=True,append_images=frames[1:],duration=durations,loop=0,optimize=False)
print(len(frames),'native frames in R32 preview')
