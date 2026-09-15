#!/bin/sh
# Host-side syntax/type check of the PSP C sources (no PSP toolchain needed).
# Compiles each file against minimal PSP header stubs with -Wall -Wextra to
# catch C-level errors that psp-gcc would otherwise be needed to detect.
set -e
cd "$(dirname "$0")/.."

CC="${HOST_CC:-gcc}"
for f in src/render.c src/game.c src/audio.c src/input.c src/main.c; do
    echo "checking $f"
    $CC -fsyntax-only -std=c11 -Wall -Wextra -I tools/stubs -I src "$f"
done
echo "host syntax check OK"
