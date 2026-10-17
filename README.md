# Rock Hard Fluffy Thing — browser port

The original 1996 Risc PC game, unchanged, running under a small ARM (26-bit) emulator
compiled to WebAssembly.

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
- `docs/`          the GitHub Pages site: `index.html` + `rhft.wasm` + `rhft.pack` (plain static files, no build step on the server)
- `extracted/`     the Spark archive unpacked with nspark (the original disc contents)

## Publishing on GitHub Pages

The site is the `docs/` folder. Push the repo, then in the repository settings choose
Pages -> Source: "Deploy from a branch", branch `main`, folder `/docs`. A minute later the
game is at `https://<user>.github.io/<repo>/`. Nothing needs compiling on GitHub's side;
`docs/rhft.wasm` and `docs/rhft.pack` are committed build outputs (`./build.sh` regenerates them).

## Building

    # native test build
    clang -O2 -o rhft_native src/cpu.c src/os.c src/rhft.c src/native.c
    ./rhft_native rhft.pack 600 "150:73:1,160:73:0,400:32:2"     # frames, then frame:key:down events (down=2 pushes an ASCII char)

    # wasm (clang 18 + wasm-ld, no emscripten needed)
    clang --target=wasm32 -O2 -nostdlib -fno-builtin -Wl,--no-entry -Wl,--export-all -Wl,--allow-undefined \
      -Wl,--initial-memory=50331648 -Wl,--max-memory=67108864 -Wl,-z,stack-size=262144 \
      -o rhft.wasm src/cpu.c src/os.c src/rhft.c src/wasm_shim.c
    python3 tools/mkpack.py extracted rhft.pack listing.txt
    python3 tools/build_page.py

## Notes / assumptions

- Mode 13 with the default 256-colour palette; two screen banks at &1FD8000.
- Sound_Pitch(&4000) is assumed to give a phase increment of 1024 (sample plays at 20.8 kHz); set
  `rhft_set_pitch_base()` to retune if samples sound sharp/flat.
- Default game keys: Z X ; / (the page also maps the arrow keys to those).
