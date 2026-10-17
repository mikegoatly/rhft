import io, base64, json
from PIL import Image
f=Image.open('/tmp/show/f0650.ppm').convert('RGB'); f2=Image.open('/tmp/show2/f0475.ppm').convert('RGB')
X0,Y0,TW,TH=36,52,32,23
def crop(im,x,y,w,h,scale=3):
    c=im.crop((x,y,x+w,y+h)).resize((w*scale,h*scale),Image.NEAREST); b=io.BytesIO(); c.save(b,'PNG'); return base64.b64encode(b.getvalue()).decode()
tiles={}; k=0
for by in range(7):
    for bx in range(7):
        if (bx,by)==(3,3): continue
        t=k+1; k+=1
        if t>30: break
        tiles[t]=crop(f, X0+bx*TW, Y0+by*TH, TW, TH+2)
things={
 'thing': crop(f2,133,114,30,28), 'gem': crop(f2,72,112,22,22),
 'drone': crop(f2,198,88,28,26), 'nasty': crop(f2,198,132,28,26),
}
# whole showcase frames (2x) for context
def frame(im): b=io.BytesIO(); im.crop((0,0,248,240)).resize((496,480),Image.NEAREST).save(b,'PNG'); return base64.b64encode(b.getvalue()).decode()
json.dump({'tiles':tiles,'things':things,'frame_tiles':frame(f),'frame_things':frame(f2)}, open('web/help_assets.json','w'))
print('ok', sum(len(v) for v in tiles.values())//1024,'KB tiles')
