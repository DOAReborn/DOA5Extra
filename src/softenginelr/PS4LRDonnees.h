#pragma once
#include <windows.h>
#include <cstdint>
#include <cstring>
#include "../commun/D5SD.h"
namespace PS4LR {
struct Row { uint8_t costume, form, vib, def, swing; float vibs[14]; float defa[21]; float swing18[18]; };
static_assert(sizeof(Row) == 220, "disposition des fiches de SoftEngineLR.d5sd");
inline constexpr uint64_t kExclude = 0x427cbfce4b5dULL, kInclude = 0x1bd834031b4a2ULL;
inline float kOptLR[12], kSwing[10][18], kState[20][5], kClamp, kState3, kState4;
inline uint16_t kFirst[64], kCount[64];
inline const Row* kRows = nullptr;
inline unsigned kNombreFiches = 0;
inline uint64_t Fnv64(const BYTE* p, size_t n) { uint64_t h = 0xcbf29ce484222325ULL; while (n--) h = (h ^ *p++) * 0x100000001b3ULL; return h; }

inline const char* Charger(const char* racine, BYTE* (*pc)(DWORD))
{
    static D5SD d;
    if (!d.Charger(racine, "maps\\DOA5Stages\\SoftEngineLR.d5sd")) return "SoftEngineLR.d5sd absent ou illisible";
    const D5SDBloc *opt = d.Bloc("opt_lr"), *sw = d.Bloc("swing_ps4"), *cst = d.Bloc("constantes"),
                   *pre = d.Bloc("premier"), *nb = d.Bloc("nombre"), *fi = d.Bloc("fiches");
    if (!opt || opt->taille != 48 || !sw || sw->taille != 5 * 72 || !cst || cst->taille != 12 || !pre || pre->taille != 128
        || !nb || nb->taille != 128 || !fi || fi->tailleElement != 220 || !fi->taille) return "SoftEngineLR.d5sd : bloc manquant ou de mauvaise taille";

    if (Fnv64(pc(0x10bd7a0), 5 * 17 * 4) != 0xaeec6f5b9c8acc72ULL || Fnv64(pc(0x10bda48), 4 * 19 * 4) != 0xd6f9abf084fa7f3fULL
        || Fnv64(pc(0x10bde60), 20 * 5 * 4) != 0xfaa7f45fa8c91647ULL) return "tables PC inattendues (game.exe 1.10C ?)";
    memcpy(kOptLR, opt->octets, 48);
    memset(kSwing, 0, sizeof kSwing);
    for (int r = 0; r < 5; r++) memcpy(kSwing[r], pc(0x10bd7a0) + r * 68, 68);
    memcpy(kSwing[5], sw->octets, 5 * 72);
    memcpy(kState, pc(0x10bde60), sizeof kState);
    memcpy(&kClamp, cst->octets, 4); memcpy(&kState3, cst->octets + 4, 4); memcpy(&kState4, cst->octets + 8, 4);
    memcpy(kFirst, pre->octets, 128); memcpy(kCount, nb->octets, 128);
    kNombreFiches = fi->taille / 220;
    for (int c = 0; c < 64; c++) if (kCount[c] && (unsigned)kFirst[c] + kCount[c] > kNombreFiches) return "SoftEngineLR.d5sd : index de fiches incoherent";
    kRows = reinterpret_cast<const Row*>(fi->octets);
    return nullptr;
}
}
