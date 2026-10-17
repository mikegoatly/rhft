import base64,sys
t=open('web/template.html').read()
w=base64.b64encode(open('rhft.wasm','rb').read()).decode()
p=base64.b64encode(open('rhft.pack','rb').read()).decode()
out=t.replace('__WASM__',w).replace('__PACK__',p)
open('web/rhft.html','w').write(out); print(len(out))
