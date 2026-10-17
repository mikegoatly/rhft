# web/template.html -> docs/index.html (standalone page that fetches rhft.wasm + rhft.pack)
t=open('web/template.html').read().replace('__PACKJS__', open('web/pack.js').read())
head='''<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<style>[hidden]{display:none!important} img{max-width:100%}</style>
'''
t=t.replace('<meta name="claude:viewport-fit-sides" content="cover">\n','')
i=t.index('<div class="wrap">')
page=head+t[:i]+'</head>\n<body>\n'+t[i:]+'\n</body>\n</html>\n'
page=page.replace('''  let packBytes = null;
  const WASM_B64 = "__WASM__";
  const PACK_B64 = "__PACK__";
  function b64(s) { const bin = atob(s); const a = new Uint8Array(bin.length); for (let i = 0; i < bin.length; i++) a[i] = bin.charCodeAt(i); return a; }
''','''  let packBytes = null;
  async function fetchBytes(url) { const r = await fetch(url); if (!r.ok) throw new Error(url + ': ' + r.status); return new Uint8Array(await r.arrayBuffer()); }
''')
page=page.replace('''    const wasm = b64(WASM_B64); const pack = b64(PACK_B64); packBytes = pack;
    const { instance } = await WebAssembly.instantiate(wasm, { env: { js_log: log } });''','''    const [wasm, pack] = await Promise.all([fetchBytes('rhft.wasm'), fetchBytes('rhft.pack')]);
    packBytes = pack;
    const { instance } = await WebAssembly.instantiate(wasm, { env: { js_log: log } });''')
page=page.replace("else if (ex.rhft_halted()) { const pack = b64(PACK_B64); new Uint8Array","else if (ex.rhft_halted()) { const pack = packBytes; new Uint8Array")
assert 'b64(' not in page
open('docs/index.html','w').write(page); print('docs/index.html', len(page))
