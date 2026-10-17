const fs=require('fs');
(async()=>{
const bytes=fs.readFileSync('rhft.wasm'); const pack=fs.readFileSync('rhft.pack');
let mem;
const {instance}=await WebAssembly.instantiate(bytes,{env:{js_log:(p,l)=>{console.log('[log]',new TextDecoder().decode(new Uint8Array(mem.buffer,p,l)))}}});
const e=instance.exports; mem=e.memory;
new Uint8Array(mem.buffer,e.rhft_pack_buffer(),pack.length).set(pack);
console.log('boot',e.rhft_boot(pack.length));
const audio=e.rhft_mem()+ (32<<20) + 64; // scratch? no: allocate separately
const t0=Date.now();
const script={150:[73,1],160:[73,0],400:[32,2],500:[66,1],620:[66,0]};
for(let f=0;f<461;f++){ const ev=script[f]; if(ev){ if(ev[1]==2) e.rhft_char(ev[0]); else e.rhft_key(ev[0],ev[1]); } const r=e.rhft_frame(); if(r==2){console.log('halted at',f);break;} }
console.log('800 frames in',Date.now()-t0,'ms');
const scr=new Uint8Array(mem.buffer,e.rhft_screen(),320*256);
fs.writeFileSync('/tmp/wasm_last.ppm',Buffer.concat([Buffer.from('P6 320 256 255\n'),Buffer.from(Array.from(scr).flatMap(p=>{const t=p&3;return [(((p&0x10)?8:0)|((p&4)?4:0)|t)*17,(((p&0x40)?8:0)|((p&0x20)?4:0)|t)*17,(((p&0x80)?8:0)|((p&8)?4:0)|t)*17]}))]));
})();
