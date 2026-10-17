// node tools/runmap.js <mapfile(4500 raw)> <outdir> <frames> <every> [script "f:key:down,..."] [--trace]
const fs=require('fs');
(async()=>{
const [,, mapfile, outdir, nframes, every, script, flag]=process.argv;
const bytes=fs.readFileSync('rhft.wasm'); const pack=fs.readFileSync('rhft.pack'); const map=fs.readFileSync(mapfile);
let mem, logs=[];
const {instance}=await WebAssembly.instantiate(bytes,{env:{js_log:(p,l)=>{logs.push(new TextDecoder().decode(new Uint8Array(mem.buffer,p,l)))}}});
const e=instance.exports; mem=e.memory;
new Uint8Array(mem.buffer,e.rhft_pack_buffer(),pack.length).set(pack); e.rhft_boot(pack.length);
const base=e.rhft_pack_buffer(); let o=(base+pack.length+64)&~15; const m8=new Uint8Array(mem.buffer);
const putStr=s=>{const a=o; for(const c of s) m8[o++]=c.charCodeAt(0); m8[o++]=0; o=(o+3)&~3; return a;};
const ml="Map1\n*\n@\n"; let np=putStr("!RHFT/Maps/MapList"); let dp=o; for(const c of ml) m8[o++]=c.charCodeAt(0); o=(o+3)&~3; e.rhft_fs_add(np,dp,ml.length);
np=putStr("!RHFT/Maps/Map1"); dp=o; m8.set(map,o); o+=4500; e.rhft_fs_add(np,dp,4500);
// menu: Enter at 150, Space at 400 (intro), Space at 420 skips "Ready"
const ev={150:[[73,1]],160:[[73,0]],400:[[32,2]],430:[[32,2]]};
if(script) for(const t of script.split(',')){ const [f,k,d]=t.split(':').map(Number); (ev[f]=ev[f]||[]).push([k,d]); }
fs.mkdirSync(outdir,{recursive:true});
const pal=new Uint8Array(768); for(let p=0;p<256;p++){const t=p&3; pal[p*3]=(((p&0x10)?8:0)|((p&4)?4:0)|t)*17; pal[p*3+1]=(((p&0x40)?8:0)|((p&0x20)?4:0)|t)*17; pal[p*3+2]=(((p&0x80)?8:0)|((p&8)?4:0)|t)*17;}
for(let f=0;f<+nframes;f++){ for(const [k,d] of (ev[f]||[])){ if(d==2) e.rhft_char(k); else e.rhft_key(k,d); } const r=e.rhft_frame(); if(r==2){console.log('halted',f);break;}
  if(f%+every==0 && f>=430){ const scr=new Uint8Array(mem.buffer,e.rhft_screen(),320*256); const out=Buffer.alloc(320*256*3); for(let i=0;i<320*256;i++){out[i*3]=pal[scr[i]*3];out[i*3+1]=pal[scr[i]*3+1];out[i*3+2]=pal[scr[i]*3+2];} fs.writeFileSync(`${outdir}/f${String(f).padStart(4,'0')}.ppm`,Buffer.concat([Buffer.from('P6 320 256 255\n'),out])); } }
if(flag==='--trace') console.log(logs.slice(-20).join('\n'));
})();
