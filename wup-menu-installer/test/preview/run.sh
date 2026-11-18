#!/bin/sh
# The installer's screens as PNGs (host SDL2 with SDL2_ttf, SDL2_gfx, SDL2_image: brew install sdl2_ttf sdl2_gfx sdl2_image).
# The console draws with its own system font; Inter stands in for it here.
set -e
HERE=$(cd "$(dirname "$0")" && pwd); ROOT=$(cd "$HERE/../.." && pwd); OUT=${1:-$ROOT/build/preview}
FONT=${LIBWUP:-$ROOT/../libwup}/fonts/Inter-Medium.ttf
mkdir -p "$OUT/gen"
cd "$ROOT/data"
xxd -i fa-solid-900.ttf | sed 's/unsigned char/static const unsigned char/; s/unsigned int/static const unsigned int/; s/_len =/_size =/' > "$OUT/gen/fa-solid-900_ttf.h"
xxd -i ter-u32b.bdf | sed 's/unsigned char/static const unsigned char/; s/unsigned int/static const unsigned int/; s/_len =/_size =/' > "$OUT/gen/ter-u32b_bdf.h"
cd "$HERE"
VERSION=$(sed -n 's/^APP_VERSION[[:space:]]*:=[[:space:]]*//p' "$ROOT/Makefile")
FLAGS="-O1 -w -I include -I $OUT/gen -I ../../source $(sdl2-config --cflags) -DAPP_VERSION=\"$VERSION\""
cc $FLAGS -c ../../source/SDL_FontCache.c -o "$OUT/gen/fc.o"
cc $FLAGS -c ../../source/fwpatch.c -o "$OUT/gen/fwpatch.o"
c++ -std=gnu++2b $FLAGS -DPREVIEW_FONT="\"$FONT\"" -DSDL_RenderPresent=preview_present \
    -c ../../source/Gfx.cpp -o "$OUT/gen/gfx.o"
c++ -std=gnu++2b $FLAGS -DPREVIEW_FONT="\"$FONT\"" -DPREVIEW_OUT="\"$OUT\"" \
    preview.cpp "$OUT/gen/gfx.o" "$OUT/gen/fc.o" "$OUT/gen/fwpatch.o" \
    $(sdl2-config --libs) -lSDL2_ttf -lSDL2_gfx -lSDL2_image -o "$OUT/preview"
SDL_RENDER_DRIVER=software "$OUT/preview"     # (a window shows for a moment: the offscreen driver has no renderer)
