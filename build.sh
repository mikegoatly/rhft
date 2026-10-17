#!/bin/sh
# Rebuild docs/rhft.wasm and docs/rhft.pack (needs clang with wasm32 support + wasm-ld, python3)
set -e
clang --target=wasm32 -O2 -nostdlib -fno-builtin -Wall \
  -Wl,--no-entry -Wl,--export-all -Wl,--allow-undefined \
  -Wl,--initial-memory=50331648 -Wl,--max-memory=67108864 -Wl,-z,stack-size=262144 \
  -o docs/rhft.wasm src/cpu.c src/os.c src/rhft.c src/wasm_shim.c
python3 tools/mkpack.py extracted docs/rhft.pack listing.txt
echo "built docs/rhft.wasm and docs/rhft.pack"
