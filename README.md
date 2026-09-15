# Pikachu Volleyball PSP port

A native PlayStation Portable port of *Pikachu Volleyball* (対戦ぴかちゅ～ ビーチバレー編,
© 1997 SACHI SOFT / SAWAYAKAN Programmers, © 1997 Satoshi Takenouchi), targeting
**6.60 PRO-C** custom firmware (also runs under PPSSPP).

Big shout out to [Kyutae Lee](https://gorisanson.github.io/) for reverse engineering the original game and sharing it [here](https://github.com/gorisanson/pikachu-volleyball).


---

## Installing

- Download the EBOOT.PBP from [releases](https://github.com/athekat/Pikachu-Volleyball-PSP/releases)
- Copy `EBOOT.PBP` to `ms0:/PSP/GAME/PikachuVolleyball/EBOOT.PBP`

## Building from source

Requirements:
- **pspsdk** toolchain (`psp-gcc`, `pspsdk`, `bin2o`) https://github.com/pspdev/pspsdk
- Python 3 + Pillow, and either `mpg123` or `ffmpeg` (for BGM decode)

```
cd PSP-PORT
python3 tools/convert_assets.py     # generate assets/ + generated headers
make                                # -> EBOOT.PBP
```

---

## Controls

| Input           | Action                                        |
|-----------------|-----------------------------------------------|
| D-pad           | Player movement / jump (also menu navigation) |
| CROSS (⨯)      | Player power hit (spike/dive) / confirm / skip |
| START           | Pause                                         |
| SELECT          | Restart (back to intro)                       |
| HOME            | Exit                                          |

Player vs Player was removed from the game, so there's no Player 2 controller bindings.

---

## Directory layout

```
PSP-PORT/
├── Makefile                 # PSPSDK build (EBOOT.PBP, PARAM.SFO, 6.60 FW)
├── README.md
├── assets/                  # generated binaries (committed)
│   ├── sprite_sheet_0.raw   #   repacked atlas tile 0 (512x512 GU_PSM_5551)
│   ├── sprite_sheet_1.raw   #   repacked atlas tile 1
│   ├── sfx.bin              #   7 SFX, 16-bit mono 44100 (header + PCM)
│   ├── bgm.raw              #   BGM, 16-bit mono 44100 (loops)
│   ├── ICON0.PNG / PIC1.PNG #   EBOOT icons
├── src/
│   ├── main.c               # PSP entry, exit callback, 25fps vblank loop
│   ├── game.c / game.h      # controller (state machine, input mapping)
│   ├── physics.c / physics.h# physics + AI (faithful port of physics.js)
│   ├── render.c / render.h  # GU rendering (port of view.js + cloud/wave)
│   ├── audio.c / audio.h    # PSP audio (SFX panning + streaming BGM)
│   ├── input.c / input.h    # PSP controller polling + edge detection
│   ├── rand.c / rand.h      # PRNG (Visual Studio LCG, [0,32767])
│   ├── atlas.h              # generated: frame -> (tile,x,y,w,h) table
│   └── audio_layout.h       # generated: SFX offsets/sizes + BGM length
└── tools/
    ├── convert_assets.py    # asset conversion (atlas repack, audio, icons)
    ├── host_test.c          # host-side physics sanity test
    └── stubs/               # minimal PSP headers for `make host-check`
```

---

