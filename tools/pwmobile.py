import asyncio
from playwright.async_api import async_playwright
async def main():
    async with async_playwright() as p:
        b = await p.chromium.launch()
        for name, dev in (("portrait", p.devices["iPhone 13"]), ("landscape", p.devices["iPhone 13 landscape"])):
            ctx = await b.new_context(**dev); pg = await ctx.new_page()
            logs=[]; pg.on("pageerror", lambda e: logs.append("PAGEERROR: "+str(e))); pg.on("console", lambda m: logs.append(m.text) if m.type=='error' else None)
            await pg.goto("http://localhost:8765/index.html"); await pg.wait_for_timeout(2500)
            await pg.tap("#start"); await pg.wait_for_timeout(1500)
            await pg.tap("button[data-key='73']"); await pg.wait_for_timeout(2000)
            await pg.tap("button[data-key='98']"); await pg.wait_for_timeout(2500)
            # slide thumb on dpad to the right for a while
            r = await pg.eval_on_selector("#dpad", "e=>{const r=e.getBoundingClientRect();return [r.x,r.y,r.width,r.height]}")
            cx, cy = r[0]+r[2]/2, r[1]+r[3]/2
            await pg.touchscreen.tap(cx + r[2]*0.4, cy)  # quick tap right
            await pg.wait_for_timeout(300)
            cdp = await ctx.new_cdp_session(pg)
            await cdp.send("Input.dispatchTouchEvent", {"type":"touchStart","touchPoints":[{"x":cx+r[2]*0.4,"y":cy}]})
            await pg.wait_for_timeout(1500)
            await cdp.send("Input.dispatchTouchEvent", {"type":"touchEnd","touchPoints":[]})
            await pg.wait_for_timeout(300)
            await pg.screenshot(path=f"/tmp/m_{name}.png")
            sw = await pg.evaluate("[document.documentElement.scrollWidth, innerWidth, document.documentElement.scrollHeight, innerHeight, document.body.className]")
            print(name, sw, logs)
            await ctx.close()
        await b.close()
asyncio.run(main())
