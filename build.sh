#!/bin/sh
# Builds the whole site into docs/ (which is not committed; GitHub Actions runs this on every push).
# Needs: clang with wasm32 support + wasm-ld (apt: clang lld), python3. No other dependencies.
# rhft.pack (the game's files bundled) is committed; see original/README.md to rebuild it from the Spark archive.
set -e
mkdir -p docs
clang --target=wasm32 -O2 -nostdlib -fno-builtin -Wall \
  -Wl,--no-entry -Wl,--export-all -Wl,--allow-undefined \
  -Wl,--initial-memory=50331648 -Wl,--max-memory=67108864 -Wl,-z,stack-size=262144 \
  -o docs/rhft.wasm src/cpu.c src/os.c src/rhft.c src/wasm_shim.c
cp rhft.pack docs/rhft.pack
python3 tools/build_docs.py      # docs/index.html  (game)
python3 tools/build_editor.py    # docs/editor.html (map editor; uses web/editor_assets.json)
python3 tools/build_help.py      # docs/help.html   (guide; uses web/help_assets.json + web/help_shots.json)
echo "site built in docs/"
