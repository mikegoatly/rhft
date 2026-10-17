import asyncio, sys
from playwright.async_api import async_playwright
async def main():
    async with async_playwright() as p:
        b = await p.chromium.launch(args=["--autoplay-policy=no-user-gesture-required"])
        pg = await b.new_page(viewport={"width":1000,"height":900})
        logs=[]
        pg.on("console", lambda m: logs.append(m.type+": "+m.text))
        pg.on("pageerror", lambda e: logs.append("PAGEERROR: "+str(e)))
        html=open('web/rhft.html').read()
        await pg.set_content("<!doctype html><html><head><meta charset=utf-8><style>[hidden]{display:none!important}</style></head><body>"+html+"</body></html>")
        await pg.wait_for_timeout(2500)
        await pg.screenshot(path="/tmp/pw1.png")
        await pg.click("#start")
        await pg.wait_for_timeout(1500)
        await pg.keyboard.press("Enter")
        await pg.wait_for_timeout(2000)
        await pg.keyboard.press("Space")
        await pg.wait_for_timeout(2500)
        await pg.keyboard.down("ArrowRight"); await pg.wait_for_timeout(1500); await pg.keyboard.up("ArrowRight")
        await pg.wait_for_timeout(500)
        await pg.screenshot(path="/tmp/pw2.png")
        fps = await pg.text_content("#fps")
        print("fps:", fps)
        print("\n".join(logs[-15:]))
        await b.close()
asyncio.run(main())
