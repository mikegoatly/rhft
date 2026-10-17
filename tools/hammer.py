import struct,sys
def dehammer(d):
    pos=[0]
    def bget():
        if pos[0]>=len(d): raise EOFError
        b=d[pos[0]]; pos[0]+=1; return b
    def word():
        return bget()|bget()<<8|bget()<<16|bget()<<24
    w0=word(); w1=word()   # header: 'Hmr?' , length
    assert w0&0xffff==0x6d48, hex(w0)
    r7=w0; size=w1; size=(size+3)&~3
    out=bytearray(size+4096)
    windowed = r7 & 0x1000000
    sl=-1; sb=size-1
    oob=[0]
    def rd(i):
        if 0<=i<size: return out[i]
        oob[0]+=1; return 0
    def put(b):
        nonlocal sl
        sl+=1; out[sl]=b
    while True:
        r0=bget()
        if r0<0x10:
            n = 0x100 if r0==0xf else r0+2
            b=bget()
            for _ in range(n): put(b)
        elif r0<0x20:
            n=(r0&0xf)+1
            for _ in range(n): put(bget())
        elif r0<0x40:
            r1=r0; r0=bget()
            n=((r1&0x1c)>>2)+2
            r3=sl-(r0+((r1&3)<<8))
            for _ in range(n):
                put(rd(r3)); r3-=1
        elif r0<0x80:
            r1=r0; r0=bget()
            n=((r1&0x38)>>3)+2
            r4=(r1&6)>>1
            if r4>=2: r4+=1
            r3=r0+((r1&1)<<8)
            r5=sl-r3
            r4=1<<r4
            r3=0
            for _ in range(n):
                put(rd(r5+(r3>>2))); r3+=r4
        elif r0==0xff:
            raise Exception("early end")
        else:
            r1=r0; r0=bget()
            n=((r1&0x78)>>3)+2
            r3=sl-(r0+((r1&7)<<8))
            for _ in range(n):
                put(rd(r3)); r3+=1
        if sl>=sb: break
    if sl!=sb: print('overrun by',sl-sb)
    e=bget(); assert e==0xff, hex(e)
    if oob[0]: print('out-of-range reads:',oob[0])
    if windowed:
        Q=size//4; phys=bytearray(size)
        for o in range(size): phys[4*(o%Q)+o//Q]=out[o]
        out=phys
    return bytes(out[:size])
if __name__=="__main__":
    d=open(sys.argv[1],'rb').read()
    o=dehammer(d); print(len(o)); open(sys.argv[2],'wb').write(o)
