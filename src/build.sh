#!/bin/sh
# Compile DOA5Extra.asi et DOA5Extra-Patreon.asi sous Linux avec MinGW-w64 32 bits
# (paquets : mingw-w64-gcc / gcc-mingw-w64-i686 et g++-mingw-w64-i686). Meme compilation que build.cmd.
set -e
cd "$(dirname "$0")"
CC="${CC:-i686-w64-mingw32-gcc}"
CXX="${CXX:-i686-w64-mingw32-g++}"
for c in "$CC" "$CXX"; do
    if ! command -v "$c" >/dev/null 2>&1; then
        echo "Compilateur introuvable : $c"
        exit 1
    fi
done
rm -f DOA5Extra.asi DOA5Extra-Patreon.asi
rm -rf obj
mkdir obj
trap 'rm -rf obj' EXIT
CF="-O2 -Wall -DDOA5EXTRA -c"
XF="-std=c++17 -O2 -Wall -DDOA5EXTRA -c"
echo "Compilation des modules..."
"$CC" $CF -o obj/doa5extra.o doa5extra.c
"$CC" $CF -DPATRON -o obj/doa5extra-patreon.o doa5extra.c
"$CC" $CF -o obj/punish.o punish/punish.c
"$CC" $CF -o obj/hitbox.o hitbox/hitbox.c
"$CXX" $XF -o obj/openburst.o openburst/OpenBurst.cpp
for m in NativeStatefulLoader RuntimeGlue StateCostumes CostumeFooter; do
    "$CXX" $XF -o "obj/$m.o" "statefulcostumes/$m.cpp"
done
"$CXX" $XF -o obj/bustpress.o bustpress/BustPress.cpp
"$CXX" $XF -o obj/ps4pressure.o ps4pressure/PS4Pressure.cpp
"$CXX" $XF -o obj/consolemusic.o consolemusic/ConsoleMusic.cpp
"$CXX" $XF -o obj/softbody.o softbody/SoftBody.cpp
"$CXX" $XF -o obj/softenginelr.o softenginelr/SoftEngineLR.cpp
"$CXX" $XF -o obj/skinsss.o skinsss/SkinSSS.cpp
MODULES="obj/punish.o obj/hitbox.o obj/openburst.o obj/NativeStatefulLoader.o obj/RuntimeGlue.o obj/StateCostumes.o obj/CostumeFooter.o obj/bustpress.o obj/ps4pressure.o obj/consolemusic.o obj/softbody.o obj/softenginelr.o obj/skinsss.o"
LIBS="-ld3d9 -lgdi32 -lwinmm -ladvapi32 -luser32"
echo "Edition de liens..."
"$CXX" -s -shared -static -o DOA5Extra.asi obj/doa5extra.o $MODULES $LIBS
"$CXX" -s -shared -static -o DOA5Extra-Patreon.asi obj/doa5extra-patreon.o $MODULES $LIBS
echo "OK : DOA5Extra.asi et DOA5Extra-Patreon.asi crees dans ce dossier (DOA5Extra.ini a livrer avec)."
