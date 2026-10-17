import base64,sys
t=open('web/template.html').read().replace('__PACKJS__', open('web/pack.js').read())
w=base64.b64encode(open('rhft.wasm','rb').read()).decode()
p=base64.b64encode(open('rhft.pack','rb').read()).decode()
out=t.replace('__WASM__',w).replace('__PACK__',p)
# the hosted artifact has no editor page and cannot carry a #p= link, so drop those references there
out=out.replace('<div class="banner" id="banner" hidden><span id="bannertext"></span> <a id="editlink" href="editor.html">open in the map editor</a> · <a href="editor.html">make your own</a></div>','<div class="banner" id="banner" hidden><span id="bannertext"></span><a id="editlink" hidden></a></div>')
out=out.replace('35 levels, plus whatever you build in the <a href="editor.html" style="color:var(--accent2)">map editor</a>.','35 levels.')
open('web/rhft.html','w').write(out); print(len(out))
