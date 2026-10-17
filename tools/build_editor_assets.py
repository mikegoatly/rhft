import sys, io, base64, json, zlib, struct
sys.path.insert(0,'tools')
from rosprite import decode
from hammer import dehammer
from PIL import Image
CELL=18
tiles=decode("ex/!MapGen/Graphics/Tiles"); mons=decode("ex/!MapGen/Graphics/Monsters"); main=decode("ex/!MapGen/Graphics/Main")
# sheet: row0 tiles 1..30 + gem(36) at col 31 ; row1 monsters 1..8 ; row2 player 1..4
sheet=Image.new('RGBA',(CELL*32,CELL*3),(0,0,0,0))
def put(im,col,row):
    sheet.paste(im,(col*CELL+(CELL-im.width)//2,row*CELL+(CELL-im.height)//2))
for t in range(1,31): put(tiles[str(t)],t,0)
put(tiles['36'],31,0)
for m in range(1,9): put(mons[str(m)],m,1)
for p in range(1,5): put(main[str(p)],p,2)
b=io.BytesIO(); sheet.save(b,'PNG'); sheet_b64=base64.b64encode(b.getvalue()).decode()
# original pack
names=[l.strip() for l in open("ex/!RHFT/Maps/MapList",encoding='latin1').read().split('\n')]
star=names.index('*'); mapnames=names[:star]; pws=names[star+1:star+1+len(mapnames)]
maps=[]
for n,pw in zip(mapnames,pws):
    d=open("ex/!RHFT/Maps/"+n,'rb').read()
    if len(d)!=4500: d=dehammer(d)
    assert len(d)==4500
    maps.append((n, '' if pw=='@' else pw, d))
payload=bytes([1,len(maps)])
for n,pw,d in maps:
    nb=n.encode('latin1'); pb=pw.encode('latin1')
    payload+=bytes([len(nb)])+nb+bytes([len(pb)])+pb+d
co=zlib.compressobj(9,zlib.DEFLATED,-15); comp=co.compress(payload)+co.flush()
print("original pack: raw",len(payload),"deflated",len(comp),"base64url chars",len(base64.urlsafe_b64encode(comp).rstrip(b'=')))
open('web/editor_assets.json','w').write(json.dumps({"sheet":sheet_b64,"orig":base64.urlsafe_b64encode(comp).decode().rstrip('=')}))
sheet.resize((sheet.width*2,sheet.height*2),Image.NEAREST).save('/tmp/sheet_all.png')
