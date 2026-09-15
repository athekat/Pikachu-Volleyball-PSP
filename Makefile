# Pikachu Volleyball - PSP port (PSPSDK)
#
# Build requirements:
#   - pspsdk toolchain (psp-gcc, pspsdk, bin2o) - https://github.com/pspdev/pspsdk
#   - generated assets (run:  python3 tools/convert_assets.py)
#
# Build:  make            ->  EBOOT.PBP
# Clean:  make clean

TARGET = PikachuVolleyball
OBJS = src/main.o src/game.o src/physics.o src/rand.o src/render.o \
       src/audio.o src/input.o \
       assets/sprite_sheet_0.o assets/sprite_sheet_1.o \
       assets/sfx_bin.o assets/bgm_raw.o

INCDIR = src

CFLAGS   = -O2 -G0 -Wall
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS  = $(CFLAGS)

LIBS = -lpspgu -lpspctrl -lpspaudio -lpspge -lpspdisplay -lm

BUILD_PRX = 1
PSP_FW_VERSION = 660
# Request the standard 24 MB user memory (safe on PSP-1000; the game's
# ~17 MB footprint fits).  Avoids the MEMSIZE=2/64 MB request that only
# PSP-2000+ can honour.
PSP_LARGE_MEMORY = 0
PSP_EBOOT_TITLE = Pikachu Volleyball
PSP_EBOOT_ICON = assets/ICON0.PNG
PSP_EBOOT_PIC1 = assets/PIC1.PNG

EXTRA_TARGETS = EBOOT.PBP

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak

# ---------------------------------------------------------------------------
# Embed raw binary assets into the EBOOT via .incbin assembly (psp-as).
# Each .s file declares a global <label>_start symbol pointing at the data.
# The C sources declare:  extern unsigned char <label>_start[];
# (bin2o is NOT used here: the modern bin2o emits mips:5900 objects that fail
# to link against allegrex; psp-as produces correct allegrex objects.)
# ---------------------------------------------------------------------------
PSPDEV := $(shell psp-config --pspdev-path 2>/dev/null || psp-config --psp-prefix)
PSP_AS := $(PSPDEV)/bin/psp-as

assets/%.o: assets/%.s
	$(PSP_AS) -o $@ $<

assets/sprite_sheet_0.o: assets/sprite_sheet_0.raw
assets/sprite_sheet_1.o: assets/sprite_sheet_1.raw
assets/sfx_bin.o: assets/sfx.bin
assets/bgm_raw.o: assets/bgm.raw
