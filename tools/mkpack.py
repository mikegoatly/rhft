import sys, os, struct, subprocess, re
root=sys.argv[1]; out=sys.argv[2]; listing=sys.argv[3]
# filetypes from nspark listing
types={}
for line in open(listing, encoding='latin1'):
    m=re.match(r'(\S.*?)\s+(\d+)\s+\S+\s+\S+\s+&([0-9A-F]{3})', line)
    if m: types[m.group(1).strip()]=int(m.group(3),16)
entries=[]
for dp,dn,fn in os.walk(root):
    for f in fn:
        full=os.path.join(dp,f); rel=os.path.relpath(full,root).replace(os.sep,'/')
        if not (rel.startswith('!RHFT/')): continue
        entries.append((rel, open(full,'rb').read(), types.get(rel,0xFFD)))
entries.sort()
hdr=4+16*len(entries); strs=b''; data=b''
recs=[]
for rel,d,t in entries:
    name_off=hdr+len(strs); strs+=rel.encode('latin1')+b'\0'
recs=[]; strs_len=(len(strs)+3)&~3; strs=strs.ljust(strs_len,b'\0')
blob=b''
off=hdr+strs_len; so=0
table=b''
for rel,d,t in entries:
    name_off=hdr+so; so+=len(rel)+1
    data_off=hdr+strs_len+len(blob)
    blob+=d; blob=blob.ljust((len(blob)+3)&~3,b'\0')
    table+=struct.pack('<4I',name_off,data_off,len(d),t)
pk=struct.pack('<I',len(entries))+table+strs+blob
open(out,'wb').write(pk)
print(len(entries),'files',len(pk),'bytes')
