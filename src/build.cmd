@echo off
setlocal
pushd "%~dp0"
rem DOA5Extra 1.1 : un seul DOA5Extra.asi (et DOA5Extra-Patreon.asi, titres patrons : -DPATRON) avec tous les modules.
rem   C : doa5extra.c, punish\punish.c, hitbox\hitbox.c ; C++ : openburst, statefulcostumes, bustpress, ps4pressure,
rem   consolemusic, softbody, softenginelr, skinsss. Objets dans obj\ (efface a la fin), puis une seule edition de liens par .asi.

rem Dossier bin de LLVM-MinGW. Laisser vide si i686-w64-mingw32-gcc est deja dans le PATH.
set "LLVM_MINGW="

set "GCC=i686-w64-mingw32-gcc.exe"
set "GXX=i686-w64-mingw32-g++.exe"
if not "%LLVM_MINGW%"=="" set "GCC=%LLVM_MINGW%\i686-w64-mingw32-gcc.exe"
if not "%LLVM_MINGW%"=="" set "GXX=%LLVM_MINGW%\i686-w64-mingw32-g++.exe"

"%GCC%" --version >nul 2>&1
if errorlevel 1 goto introuvable
"%GXX%" --version >nul 2>&1
if errorlevel 1 goto introuvable

if exist DOA5Extra.asi del DOA5Extra.asi
if exist DOA5Extra-Patreon.asi del DOA5Extra-Patreon.asi
if exist obj rmdir /s /q obj
mkdir obj
set "CF=-O2 -Wall -DDOA5EXTRA -c"
set "XF=-std=c++17 -O2 -Wall -DDOA5EXTRA -c"
echo Compilation des modules...
"%GCC%" %CF% -o obj\doa5extra.o doa5extra.c
if errorlevel 1 goto echec
"%GCC%" %CF% -DPATRON -o obj\doa5extra-patreon.o doa5extra.c
if errorlevel 1 goto echec
"%GCC%" %CF% -o obj\punish.o punish\punish.c
if errorlevel 1 goto echec
"%GCC%" %CF% -o obj\hitbox.o hitbox\hitbox.c
if errorlevel 1 goto echec
"%GXX%" %XF% -o obj\openburst.o openburst\OpenBurst.cpp
if errorlevel 1 goto echec
for %%m in (NativeStatefulLoader RuntimeGlue StateCostumes CostumeFooter) do (
  "%GXX%" %XF% -o obj\%%m.o statefulcostumes\%%m.cpp
  if errorlevel 1 goto echec
)
"%GXX%" %XF% -o obj\bustpress.o bustpress\BustPress.cpp
if errorlevel 1 goto echec
"%GXX%" %XF% -o obj\ps4pressure.o ps4pressure\PS4Pressure.cpp
if errorlevel 1 goto echec
"%GXX%" %XF% -o obj\consolemusic.o consolemusic\ConsoleMusic.cpp
if errorlevel 1 goto echec
"%GXX%" %XF% -o obj\softbody.o softbody\SoftBody.cpp
if errorlevel 1 goto echec
"%GXX%" %XF% -o obj\softenginelr.o softenginelr\SoftEngineLR.cpp
if errorlevel 1 goto echec
"%GXX%" %XF% -o obj\skinsss.o skinsss\SkinSSS.cpp
if errorlevel 1 goto echec
set "MODULES=obj\punish.o obj\hitbox.o obj\openburst.o obj\NativeStatefulLoader.o obj\RuntimeGlue.o obj\StateCostumes.o obj\CostumeFooter.o obj\bustpress.o obj\ps4pressure.o obj\consolemusic.o obj\softbody.o obj\softenginelr.o obj\skinsss.o"
set "LIBS=-ld3d9 -lgdi32 -lwinmm -ladvapi32 -luser32"
echo Edition de liens de DOA5Extra.asi...
"%GXX%" -s -shared -static -o DOA5Extra.asi obj\doa5extra.o %MODULES% %LIBS%
if errorlevel 1 goto echec
echo Edition de liens de DOA5Extra-Patreon.asi...
"%GXX%" -s -shared -static -o DOA5Extra-Patreon.asi obj\doa5extra-patreon.o %MODULES% %LIBS%
if errorlevel 1 goto echec
rmdir /s /q obj
echo OK : DOA5Extra.asi et DOA5Extra-Patreon.asi crees dans ce dossier (DOA5Extra.ini a livrer avec).
goto fin

:introuvable
echo Compilateur introuvable : "%GCC%" ou "%GXX%"
echo Renseigne LLVM_MINGW en haut de build.cmd : dossier bin de LLVM-MinGW.
goto fin

:echec
echo ECHEC de la compilation : voir les messages ci-dessus.
if exist obj rmdir /s /q obj

:fin
echo.
pause
popd
