#pragma once
#include <windows.h>
#include <string.h>

struct D5SDBloc { char nom[64]; DWORD tailleElement, taille; const BYTE* octets; };

struct D5SD {
    BYTE* donnees = nullptr; DWORD taille = 0, nombre = 0; D5SDBloc* blocs = nullptr;

    bool Charger(const char* racine, const char* chemin) {
        char complet[MAX_PATH];
        if (strlen(racine) + strlen(chemin) + 1 > sizeof(complet)) return false;
        strcpy(complet, racine); strcat(complet, chemin);
        HANDLE f = CreateFileA(complet, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
        if (f == INVALID_HANDLE_VALUE) return false;
        const DWORD t = GetFileSize(f, nullptr); DWORD lu = 0;
        BYTE* d = (t >= 12 && t < (64u << 20)) ? (BYTE*)HeapAlloc(GetProcessHeap(), 0, t) : nullptr;
        const bool ok = d && ReadFile(f, d, t, &lu, nullptr) && lu == t;
        CloseHandle(f);
        if (!ok || memcmp(d, "D5SD", 4) || Lire32(d + 4) != 1) { if (d) HeapFree(GetProcessHeap(), 0, d); return false; }
        const DWORD n = Lire32(d + 8);
        D5SDBloc* b = n && n < 65536 ? (D5SDBloc*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, n * sizeof(D5SDBloc)) : nullptr;
        if (!b) { HeapFree(GetProcessHeap(), 0, d); return false; }
        DWORD o = 12;
        for (DWORD i = 0; i < n; ++i) {
            if (o + 2 > t) return Echec(d, b);
            const DWORD l = d[o] | d[o + 1] << 8; o += 2;
            if (!l || l >= sizeof(b[i].nom) || o + l + 8 > t) return Echec(d, b);
            memcpy(b[i].nom, d + o, l); b[i].nom[l] = 0; o += l;
            b[i].tailleElement = Lire32(d + o); b[i].taille = Lire32(d + o + 4); o += 8;
            if (b[i].taille > t - o || (b[i].tailleElement && b[i].taille % b[i].tailleElement)) return Echec(d, b);
            b[i].octets = d + o; o += b[i].taille;
        }
        if (o != t) return Echec(d, b);

        DWORD total = 0;
        for (DWORD i = 0; i < n; ++i) total += (b[i].taille + 15) & ~15u;
        BYTE* aligne = (BYTE*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, total + 16);
        if (!aligne) return Echec(d, b);
        BYTE* q = (BYTE*)(((ULONG_PTR)aligne + 15) & ~(ULONG_PTR)15);
        for (DWORD i = 0; i < n; ++i) { memcpy(q, b[i].octets, b[i].taille); b[i].octets = q; q += (b[i].taille + 15) & ~15u; }
        HeapFree(GetProcessHeap(), 0, d);
        donnees = aligne; taille = total; nombre = n; blocs = b;
        return true;
    }
    const D5SDBloc* Bloc(DWORD i) const { return i < nombre ? &blocs[i] : nullptr; }
    const D5SDBloc* Bloc(const char* nom) const {
        for (DWORD i = 0; i < nombre; ++i) if (!strcmp(blocs[i].nom, nom)) return &blocs[i];
        return nullptr;
    }
private:
    static DWORD Lire32(const BYTE* p) { return p[0] | p[1] << 8 | p[2] << 16 | (DWORD)p[3] << 24; }
    static bool Echec(BYTE* d, D5SDBloc* b) { HeapFree(GetProcessHeap(), 0, b); HeapFree(GetProcessHeap(), 0, d); return false; }
};
