import struct, sys, json, base64, io
from PIL import Image
def decode(path):
    d=open(path,'rb').read(); n,first,free=struct.unpack_from('<3I',d,0); p=first-4; out={}
    for i in range(n):
        sz,nm,w,h,fb,lb,img,mask,mode=struct.unpack_from('<I12s7I',d,p)
        name=nm.rstrip(b'\0').decode()
        bpp={27:4,13:8,12:4,28:8,15:8,20:4}[mode]
        W=((w+1)*32-fb-(31-lb))//bpp; H=h+1
        pal=[]
        if img>44:
            for k in range((img-44)//8):
                c=struct.unpack_from('<I',d,p+44+k*8)[0]; pal.append(((c>>8)&255,(c>>16)&255,(c>>24)&255))
        im=Image.new('RGBA',(W,H))
        px=im.load(); rowbytes=(w+1)*4
        for y in range(H):
            for x in range(W):
                bit=fb+x*bpp; byte=d[p+img+y*rowbytes+bit//8]; v=(byte>>(bit%8))&((1<<bpp)-1)
                a=255
                if mask:
                    mb=d[p+mask+y*rowbytes+bit//8]; a=255 if (mb>>(bit%8))&((1<<bpp)-1) else 0
                px[x,y]=pal[v]+(a,) if pal else (v*17,v*17,v*17,a)
        out[name]=im; p+=sz
    return out
if __name__=="__main__":
    for f in ("Tiles","Monsters","Sprites","Main"):
        sp=decode("ex/!MapGen/Graphics/"+f)
        names=sorted(sp,key=lambda s:int(s))
        sheet=Image.new('RGBA',(sum(sp[n].width for n in names)+len(names)*2, max(sp[n].height for n in names)))
        x=0
        for n in names: sheet.paste(sp[n],(x,0)); x+=sp[n].width+2
        sheet=sheet.resize((sheet.width*3,sheet.height*3),Image.NEAREST); sheet.save(f"/tmp/sheet_{f}.png"); print(f,[(n,sp[n].size) for n in names][:3])
