# DOA5Extra

Extra modules for **Dead or Alive 5 Last Round**. Needs DOA5Tools (or an ASI Loader).

## Install

Use [DOA5Installer](https://github.com/DOAReborn/DOA5Installer/releases/latest) (manual install: extract the zip into the `scripts` folder next to `game.exe`).

Turn modules on or off in the **EXTRA OPTIONS** menu (main menu) or in `DOA5Extra.ini`.

## Modules

- **Style**: custom titles, new life bar colors, DOAReborn title portrait, REVOLUTION title and character select screens, Patreon banner in the main menu.
- **Punish Training**: punishment challenges in Training (F8 / L3).
- **Hitbox**: attack and body hitboxes in Training (F6).
- **Critical Burst**: PS4 Critical Burst fragments.
- **PS4 costumes**: transformations, tearing and effects of the PS4 costumes (installed with DOA5Installer), also in lobbies.
- **Bust physics**: breast press on contact, with the PS4 spring, and the PS4 "LR" breast motion.
- **PS4 skin**: subsurface scattering and sweat, like PS4.
- **Console music**: music of the console stages.

## Build

32-bit, with [LLVM-MinGW](https://github.com/mstorsjo/llvm-mingw). From `src/`: `build.cmd` (C files with `gcc -c`, C++ files with `g++ -std=c++17 -c`, all with `-DDOA5EXTRA`, then one `g++ -shared -static -s` link with `-ld3d9 -lgdi32 -lwinmm -ladvapi32 -luser32`).
