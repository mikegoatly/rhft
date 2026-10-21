# Rock Hard Fluffy Thing — browser port

The original 1996 Risc PC game, unchanged, running under a small ARM (26-bit) emulator
compiled to WebAssembly.

[Play it here! ![Screenshot of the first level of RHFT](docs/rhft_screenshot.png)](https://mikegoatly.github.io/rhft/)



## Layout

- `src/cpu.c`     ARM2/3-style 26-bit CPU interpreter (data processing, mul, swp, ldr/str, ldm/stm, b/bl, swi; USR/SVC/IRQ banking)
- `src/os.c`      RISC OS environment: module loading + SWI dispatch, virtual filing system, VDU/screen banks,
                  keyboard (OS_Byte 121/122/129), callbacks, Sound channel handler (voice generator calling convention)
- `src/rhft.c`    Machine setup (loads FastSpr, Amnesia, Stasis, StasisRaw, Blitz; runs the !Run2 StasisLoad commands;
                  loads Code.Game at &8000) and the host API
- `src/native.c`  Headless test harness (PPM screenshots, raw audio, scripted keys)
- `src/wasm_shim.c` host_log import for the wasm build
- `tools/mkpack.py`   bundles the !RHFT files into `rhft.pack` (names, RISC OS filetypes, data)
- `tools/hammer.py`   stand-alone decoder for Andy Southgate's Hammer compression (maps + graphics)
- `tools/armdis.py`   capstone-based disassembler with RISC OS SWI names
- `tools/build_page.py` inlines rhft.wasm + rhft.pack into web/template.html -> web/rhft.html
- `docs/`          (generated, git-ignored) the site: `index.html` (game), `editor.html` (map editor), `help.html` (guide), `rhft.wasm`, `rhft.pack`
- `web/pack.js`    map-pack codec shared by both pages (deflate-raw + base64url in the URL fragment, `#p=...`)
- `web/editor_template.html`, `tools/build_editor.py`, `tools/build_editor_assets.py`  the editor and its graphics (decoded from !MapGen's sprite files)
- `web/help_template.html`, `tools/build_help.py`, `tools/build_help_assets.py` the guide; its tile pictures are cropped from emulator screenshots of a showcase level (`tools/runmap.js` runs any raw map headlessly and dumps frames)
- `tools/rosprite.py`  RISC OS sprite file decoder; `tools/bbcbasic.py` BBC BASIC detokeniser (used to read !MapGen's source)
- `original/rhft.zip` the 1996 disc as a Spark archive (see original/README.md); `rhft.pack` is `!RHFT` from it, bundled, and is what the build uses

## Map packs

A pack is a list of maps (name, optional password, 4500-byte level). Both pages keep the whole pack in the
URL fragment `#p=<base64url(deflate-raw(payload))>`; nothing is stored anywhere. The game page writes the
pack's `MapList` and map files into the emulator's filing system on boot (later files override the originals),
so the original game code loads them like any other level. Map format notes are at the top of
`web/editor_template.html`; entity coordinates are stored one row above the grid row (a quirk of the original).

## Building

    # native test build
    clang -O2 -o rhft_native src/cpu.c src/os.c src/rhft.c src/native.c
    ./rhft_native rhft.pack 600 "150:73:1,160:73:0,400:32:2"     # frames, then frame:key:down events (down=2 pushes an ASCII char)

    # wasm (clang 18 + wasm-ld, no emscripten needed)
    clang --target=wasm32 -O2 -nostdlib -fno-builtin -Wl,--no-entry -Wl,--export-all -Wl,--allow-undefined \
      -Wl,--initial-memory=50331648 -Wl,--max-memory=67108864 -Wl,-z,stack-size=262144 \
      -o rhft.wasm src/cpu.c src/os.c src/rhft.c src/wasm_shim.c
    python3 tools/build_editor_assets.py && python3 tools/build_editor.py   # docs/editor.html
    python3 tools/build_help.py                                               # docs/help.html (needs web/help_assets.json + web/help_shots.json)
    python3 tools/build_docs.py                                               # docs/index.html
    python3 tools/build_page.py                                               # web/rhft.html (single-file artifact variant)

## Notes / assumptions

- Mode 13 with the default 256-colour palette; two screen banks at &1FD8000.
- Sound_Pitch(&4000) is assumed to give a phase increment of 1024 (sample plays at 20.8 kHz); set
  `rhft_set_pitch_base()` to retune if samples sound sharp/flat.
- Default game keys: Z X ; / (the page also maps the arrow keys to those).
