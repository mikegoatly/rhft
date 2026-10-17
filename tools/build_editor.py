import json
a=json.load(open('web/editor_assets.json'))
t=open('web/editor_template.html').read().replace('__PACKJS__',open('web/pack.js').read()).replace('__SHEET__','data:image/png;base64,'+a['sheet']).replace('__ORIG__',a['orig'])
open('docs/editor.html','w').write(t); print('docs/editor.html',len(t))
