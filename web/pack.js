// Map pack codec shared by the game page and the editor.
// A pack is { maps: [ { name, password, data: Uint8Array(4500) } ] }.
// URL form: #p=<base64url(deflate-raw(payload))>, payload = [1][count] then per map [nlen][name][plen][password][4500 bytes]
const RHFTPack = (() => {
  const MAPSIZE = 4500;
  function b64uEncode(bytes) { let s = ''; for (let i = 0; i < bytes.length; i++) s += String.fromCharCode(bytes[i]); return btoa(s).replace(/\+/g, '-').replace(/\//g, '_').replace(/=+$/, ''); }
  function b64uDecode(str) { str = str.replace(/-/g, '+').replace(/_/g, '/'); while (str.length % 4) str += '='; const bin = atob(str); const a = new Uint8Array(bin.length); for (let i = 0; i < bin.length; i++) a[i] = bin.charCodeAt(i); return a; }
  async function pump(bytes, Stream, fmt) {
    if (typeof Stream === 'undefined') throw new Error('This browser cannot compress map packs (needs CompressionStream). Try a current Chrome, Safari or Firefox.');
    const s = new Stream(fmt); const w = s.writable.getWriter(); w.write(bytes); w.close();
    const out = new Uint8Array(await new Response(s.readable).arrayBuffer()); return out;
  }
  const latin = { enc: s => Uint8Array.from(s, c => c.charCodeAt(0) & 0xFF), dec: a => String.fromCharCode(...a) };
  async function encode(pack) {
    const parts = [Uint8Array.of(1, pack.maps.length)];
    for (const m of pack.maps) {
      const n = latin.enc(m.name.slice(0, 10)), p = latin.enc((m.password || '').slice(0, 20));
      parts.push(Uint8Array.of(n.length), n, Uint8Array.of(p.length), p, m.data);
    }
    let len = 0; for (const x of parts) len += x.length;
    const payload = new Uint8Array(len); let o = 0; for (const x of parts) { payload.set(x, o); o += x.length; }
    return b64uEncode(await pump(payload, window.CompressionStream, 'deflate-raw'));
  }
  async function decode(str) {
    const payload = await pump(b64uDecode(str), window.DecompressionStream, 'deflate-raw');
    if (payload[0] !== 1) throw new Error('Unknown map pack version');
    const count = payload[1]; let o = 2; const maps = [];
    for (let i = 0; i < count; i++) {
      const nl = payload[o++]; const name = latin.dec(payload.subarray(o, o + nl)); o += nl;
      const pl = payload[o++]; const password = latin.dec(payload.subarray(o, o + pl)); o += pl;
      const data = payload.slice(o, o + MAPSIZE); o += MAPSIZE;
      if (data.length !== MAPSIZE) throw new Error('Map pack is truncated');
      maps.push({ name, password, data });
    }
    return { maps };
  }
  function fromHash() { const m = /[#&]p=([A-Za-z0-9_-]+)/.exec(location.hash); return m ? m[1] : null; }
  // MapList text as the game expects it: names, a line with *, then one password line per map (@ = none)
  function mapList(pack) { return pack.maps.map(m => m.name).join('\n') + '\n*\n' + pack.maps.map(m => m.password ? m.password.toUpperCase() : '@').join('\n') + '\n'; }
  return { MAPSIZE, encode, decode, fromHash, mapList, b64uEncode, b64uDecode };
})();
