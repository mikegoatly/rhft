import json, base64, html
a=json.load(open('web/help_assets.json')); shots=json.load(open('web/help_shots.json'))
T=[(1,"Normal","Plain tile. Walk on this to your heart's content."),
(2,"Hole","Nothing there. Walk into it and Squidgy falls, which costs a life. Most of a level is holes."),
(3,"Fire","Lava. Avoid this. Fluffy Things are famous for their ability to burst into flames instantly."),
(4,"Water","Avoid this too. Fluffy Things are also famous for their inability to swim."),
(5,"Bomb","Bomb + Squidgy = dead Squidgy. Jump over it, or blow it up with a monster."),
(6,"Force right","Force arrow. Once stepped on, Squidgy can only move in the pointed direction. Works on monsters too."),
(7,"Force down","Force arrow, pointing down."),
(8,"Force left","Force arrow, pointing left."),
(9,"Force up","Force arrow, pointing up."),
(10,"Jump","Makes Squidgy jump over the tile in front of him. The way to cross a bomb, lava, water or a high tile."),
(11,"Slippy","Ice. Unless Squidgy turns, he slides straight off this tile in the direction he's facing."),
(12,"Cracked (1)","Cracking tile. Each time it is stepped on it cracks a bit more, until there is nothing left. Stage 1 takes three crossings."),
(13,"Cracked (2)","The same tile after one crossing: two more before it drops."),
(14,"Cracked (3)","Nearly gone. One more step and it is a hole."),
(15,"High","Can't be walked through, but can be jumped. A wall that stops monsters as well."),
(16,"Reduce time","Time tile. Reduces the time Squidgy has left at an alarming rate while trodden on."),
(17,"Button","Switch tile. Changes a tile elsewhere on the map; see below for how the four states work."),
(18,"Electricity off","Electric tile, starting off. Harmless when off, but electrocutes Squidgy when it's on. Flips at the timing you set."),
(19,"Electricity on","The same tile, starting on. Also kills monsters while live."),
(20,"Exit","Get here when Squidgy has all the diamonds. It opens only when the last one is collected."),
(21,"Teleport","Teleports Squidgy to somewhere else on the map; you pick where."),
(22,"Push right","Push arrow. Automatically pushes Squidgy in the pointed direction."),
(23,"Push down","Push arrow, pointing down."),
(24,"Push left","Push arrow, pointing left."),
(25,"Push up","Push arrow, pointing up."),
(26,"Rotating push right","Same as a push arrow, except it turns a quarter turn every timing interval. Shown pointing right to start."),
(27,"Rotating push down","Rotating push arrow, starting downwards."),
(28,"Rotating push left","Rotating push arrow, starting left."),
(29,"Rotating push up","Rotating push arrow, starting up."),
(30,"Disappearing","Vanishing tile. Starts to fade away when trodden on; the timing is how long each stage lasts."),
]
cards=''.join(f'    <div class="tile" id="tile{n}"><img src="data:image/png;base64,{a["tiles"][str(n)]}" alt="{html.escape(name)} tile"><div><b>{html.escape(name)} <span class="num">#{n}</span></b><span>{html.escape(desc)}</span></div></div>\n' for n,name,desc in T)
t=open('web/help_template.html').read()
t=t.replace('__TILES__\n',cards)
for k in ('thing','gem','drone','nasty'): t=t.replace(f'__THING_{k}__','data:image/png;base64,'+a['things'][k])
t=t.replace('__FRAME_TILES__','data:image/png;base64,'+a['frame_tiles']).replace('__FRAME_THINGS__','data:image/png;base64,'+a['frame_things'])
t=t.replace('__SHOT_EDITOR__','data:image/jpeg;base64,'+shots['editor']).replace('__SHOT_SWITCH__','data:image/jpeg;base64,'+shots['switch'])
assert '__' not in t.replace('__proto__','')
open('docs/help.html','w').write(t); print('docs/help.html',len(t))
