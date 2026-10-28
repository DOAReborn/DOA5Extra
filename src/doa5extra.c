#include <windows.h>
#include <d3d9.h>
#include <stdio.h>
#include <string.h>
#include <shellapi.h>
#include "commun/extra.h"

#define VERSION_EXTRA "1.3"

#define RVA_KTOL_PRINTF  0x846010
static const BYTE KTOL_PRINTF_BYTES[] = {0x55, 0x8B, 0xEC, 0x81, 0xEC, 0x04, 0x02, 0x00, 0x00};

static BYTE *base;

static FILE *g_log;
static long g_octets;
static DWORD g_t0;
static CRITICAL_SECTION g_cs;
static char g_ini[MAX_PATH];
static wchar_t g_iniW[MAX_PATH];
const char *Extra_Ini(void) { return g_ini; }
const wchar_t *Extra_IniW(void) { return g_iniW; }

void Extra_Journal(const char *module, const char *ligne)
{
    if (!g_log || g_octets > 256 * 1024) return;
    EnterCriticalSection(&g_cs);
    const int n = fprintf(g_log, "t=%lu %s %s\n", (unsigned long)(GetTickCount() - g_t0), module, ligne);
    if (n > 0) g_octets += n;
    fflush(g_log);
    LeaveCriticalSection(&g_cs);
}

static void Journal(const char *libelle, long a, long b)
{
    char l[160];
    snprintf(l, sizeof l, "%s %ld %ld", libelle, a, b);
    Extra_Journal("DOA5Extra", l);
}

static void *Detourner(DWORD rva, const BYTE *attendu, size_t nv, size_t n, void *crochet)
{
    BYTE *f = base + rva;
    if (n < 5 || n > 16 || nv > n || memcmp(f, attendu, nv)) return NULL;
    BYTE *t = VirtualAlloc(NULL, n + 5, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!t) return NULL;
    memcpy(t, f, n);
    t[n] = 0xE9; const LONG retour = (LONG)((f + n) - (t + n + 5)); memcpy(t + n + 1, &retour, 4);
    DWORD ancien;
    if (!VirtualProtect(f, n, PAGE_EXECUTE_READWRITE, &ancien)) { VirtualFree(t, 0, MEM_RELEASE); return NULL; }
    BYTE saut[16]; memset(saut, 0x90, sizeof saut);
    saut[0] = 0xE9; const LONG rel = (LONG)((BYTE *)crochet - (f + 5)); memcpy(saut + 1, &rel, 4);
    memcpy(f, saut, n);
    VirtualProtect(f, n, ancien, &ancien);
    FlushInstructionCache(GetCurrentProcess(), f, n);
    return t;
}

#define RVA_DYN_PRESENT  0x6830D0
#define RVA_DYN_NOM      0x6831F0
#define RVA_DYN_CATEG    0x683290
#define RVA_DYN_CADRE    0x683330
#define RVA_TITRE_OBTENU 0x420260
#define RVA_TITRE_EQUIPER 0x0BE2E0
#define RVA_SON          0x557C40
#define ID_PREMIER       1035
#define NB_TITRES        20

typedef struct { int id; const WCHAR *texte; int perso; int cadre; } TitreCustom;
#include "titres.h"
#ifdef PATRON
#define VERSION_PATRON 1
#else
#define VERSION_PATRON 0
#endif

static const TitreCustom *g_titre[NB_TITRES];
static BYTE g_patron[NB_TITRES];
static long g_ignores;
static void Ranger(const TitreCustom *liste, BYTE patron)
{
    for (const TitreCustom *t = liste; t->id; t++) {
        const int k = t->id - ID_PREMIER;
        if (!t->texte || !t->texte[0]) continue;
        if (k < 0 || k >= NB_TITRES || g_titre[k]) { g_ignores++; continue; }
        g_titre[k] = t; g_patron[k] = patron;
    }
}
static const TitreCustom *Custom(int id)
{
    return id >= ID_PREMIER && id < ID_PREMIER + NB_TITRES ? g_titre[id - ID_PREMIER] : NULL;
}

typedef int (__cdecl *Dyn_t)(int id);
static Dyn_t o_present, o_nom, o_categ, o_cadre;
static int __cdecl hk_present(int id) { return Custom(id) ? 1 : o_present(id); }
static int __cdecl hk_nom(int id) { const TitreCustom *t = Custom(id); return t ? (int)(ULONG_PTR)t->texte : o_nom(id); }
static int __cdecl hk_categ(int id) { const TitreCustom *t = Custom(id); return t ? (int)(unsigned char)(char)t->perso : o_categ(id); }
static int __cdecl hk_cadre(int id) { const TitreCustom *t = Custom(id); return t ? t->cadre : o_cadre(id); }

typedef char (__attribute__((thiscall)) *Obtenu_t)(void *systeme, int id);
static Obtenu_t o_obtenu;

static char __attribute__((thiscall)) hk_obtenu(void *systeme, int id)
{
    return Custom(id) ? 1 : o_obtenu(systeme, id);
}

typedef char (__cdecl *Equiper_t)(int place, int id);
static Equiper_t o_equiper;
static char __cdecl hk_equiper(int place, int id)
{
    if (!VERSION_PATRON && Custom(id) && g_patron[id - ID_PREMIER]) {
        ((void (__cdecl *)(DWORD, DWORD))(base + RVA_SON))(0x10000, 0);
        Journal("titre_patron_refuse", id, place);
        return 1;
    }
    return o_equiper(place, id);
}

static void Titres_Poser(void)
{
    static const BYTE seh[] = {0x55, 0x8B, 0xEC, 0x6A, 0xFF, 0x68};
    static const BYTE obt[] = {0x55, 0x8B, 0xEC, 0x56, 0x8B, 0xF1};
    Ranger(TITRES_PATRON, 1);
    Ranger(TITRES_TOUS, 0);
    int n = 0;
    for (int i = 0; i < NB_TITRES; i++) n += g_titre[i] != NULL;
    if (g_ignores) Journal("titres_ignores", g_ignores, 0);
    int ok = 0;
    ok += (o_present = (Dyn_t)Detourner(RVA_DYN_PRESENT, seh, sizeof seh, 10, (void *)hk_present)) != NULL;
    ok += (o_nom = (Dyn_t)Detourner(RVA_DYN_NOM, seh, sizeof seh, 10, (void *)hk_nom)) != NULL;
    ok += (o_categ = (Dyn_t)Detourner(RVA_DYN_CATEG, seh, sizeof seh, 10, (void *)hk_categ)) != NULL;
    ok += (o_cadre = (Dyn_t)Detourner(RVA_DYN_CADRE, seh, sizeof seh, 10, (void *)hk_cadre)) != NULL;
    ok += (o_obtenu = (Obtenu_t)Detourner(RVA_TITRE_OBTENU, obt, sizeof obt, sizeof obt, (void *)hk_obtenu)) != NULL;
    static const BYTE equ[] = {0x55, 0x8B, 0xEC, 0x8B, 0x45, 0x0C};
    ok += (o_equiper = (Equiper_t)Detourner(RVA_TITRE_EQUIPER, equ, sizeof equ, sizeof equ, (void *)hk_equiper)) != NULL;
    Journal("titres", ok, n);
}

#include "logo_portrait.h"
#include "black_titre.h"
#include "select_revolution.h"
#include "patreon_banniere.h"
#define RVA_IAT_D3DX_TEX 0x96639C

static DWORD Fnv1a(const BYTE *p, UINT n)
{
    DWORD h = 0x811C9DC5u;
    for (UINT i = 0; i < n; i++) { h ^= p[i]; h *= 0x01000193u; }
    return h;
}

static BYTE *Verrouiller(IDirect3DTexture9 *t, UINT l, UINT h, D3DFORMAT f, INT *pas)
{
    D3DSURFACE_DESC dd; D3DLOCKED_RECT lr;
    if (FAILED(IDirect3DTexture9_GetLevelDesc(t, 0, &dd)) || dd.Width != l || dd.Height != h || dd.Format != f ||
        dd.Pool != D3DPOOL_MANAGED) { Journal("texture_inattendue", (long)l, (long)dd.Format); return NULL; }
    if (FAILED(IDirect3DTexture9_LockRect(t, 0, &lr, NULL, 0)) || !lr.pBits) { Journal("verrou_refuse", (long)l, 0); return NULL; }
    *pas = lr.Pitch;
    return (BYTE *)lr.pBits;
}

typedef struct {
    UINT tailleIdx; DWORD fnvIdx; UINT l, h; const BYTE *indices;
    UINT taillePal; DWORD fnvPal; UINT hPal, ligne; const unsigned int *palette;
} Remplacement;
static const Remplacement REMPLACEMENTS[] = {

    {4276, 0x47FD5C7Cu, 122, 34, &LOGO_INDICES[0][0], 8320, 0x87A64E10u, 8, 0, LOGO_PALETTE},

    {81824, 0x9CE4FDB5u, 552, 148, &BLACK_INDICES[0][0], 8320, 0xC36F5C05u, 8, 5, BLACK_PALETTE},

    {86636, 0x79F85B35u, 486, 178, &PATREON_INDICES[0][0], 4224, 0xE9AB156Du, 4, 3, PATREON_PALETTE},
};
#define NB_REMPLACEMENTS ((int)(sizeof REMPLACEMENTS / sizeof REMPLACEMENTS[0]))
static void EcrireIndices(IDirect3DTexture9 *t, const Remplacement *r)
{
    INT pas; BYTE *p = Verrouiller(t, r->l, r->h, D3DFMT_L8, &pas);
    if (!p) return;
    for (UINT y = 0; y < r->h; y++) memcpy(p + y * pas, r->indices + y * r->l, r->l);
    IDirect3DTexture9_UnlockRect(t, 0);
}
static void EcrireLigne(IDirect3DTexture9 *t, const Remplacement *r)
{
    INT pas; BYTE *p = Verrouiller(t, 256, r->hPal, D3DFMT_A8R8G8B8, &pas);
    if (!p) return;
    memcpy(p + r->ligne * pas, r->palette, 256 * sizeof r->palette[0]);
    IDirect3DTexture9_UnlockRect(t, 0);
}

#define SELREV_FOND_TAILLE 921728u
#define SELREV_FOND_FNV    0x34AC8F9Cu
#define SELREV_PAL_FNV     0x3BE0783Bu
#define SELREV_FOND_L 1280
#define SELREV_FOND_H 720
#define SELREV_X0 380
#define SELREV_X1 900
#define SELREV_Y0 215
#define SELREV_Y1 420
static DWORD g_selPal[256];
static BYTE g_selRouge[256];
static BOOL g_selPaletteVue;
static IDirect3DTexture9 *g_selFondEnAttente;
static short g_selCache[32768];
static CRITICAL_SECTION g_csSel;

static BOOL SelRouge(DWORD c)
{
    const int a = (int)(c >> 24), r = (int)(c >> 16 & 255), g = (int)(c >> 8 & 255), b = (int)(c & 255);
    const int m = g > b ? g : b;
    return a >= 16 && r >= 40 && r >= 2 * m + 10;
}
static BYTE SelPlusProche(int r, int g, int b)
{
    const int cle = (r >> 3) << 10 | (g >> 3) << 5 | (b >> 3);
    if (g_selCache[cle]) return (BYTE)(g_selCache[cle] - 1);
    long meilleur = 0x7FFFFFFF; int choix = 0;
    for (int i = 0; i < 256; i++) {
        const DWORD c = g_selPal[i];
        if ((c >> 24) < 250) continue;
        const int dr = (int)(c >> 16 & 255) - r, dg = (int)(c >> 8 & 255) - g, db = (int)(c & 255) - b;
        const long d = 3L * dr * dr + 4L * dg * dg + 2L * db * db;
        if (d < meilleur) { meilleur = d; choix = i; }
    }
    g_selCache[cle] = (short)(choix + 1);
    return (BYTE)choix;
}
static void SelRetoucher(IDirect3DTexture9 *t)
{
    INT pas; BYTE *p = Verrouiller(t, SELREV_FOND_L, SELREV_FOND_H, D3DFMT_L8, &pas);
    if (!p) return;
    long n = 0; int x0 = SELREV_X1, y0 = SELREV_Y1, x1 = -1, y1 = -1;
    for (int y = SELREV_Y0; y < SELREV_Y1; y++)
        for (int x = SELREV_X0; x < SELREV_X1; x++)
            if (g_selRouge[p[y * pas + x]]) {
                n++;
                if (x < x0) x0 = x;
                if (x > x1) x1 = x;
                if (y < y0) y0 = y;
                if (y > y1) y1 = y;
            }
    Journal("selection_last_round_pixels", n, x1 - x0);
    const int ey0 = y0 - 1, ey1 = y1 + 3;
    if (n < 5000 || x1 - x0 < 300 || x1 - x0 > 520 || y1 - y0 < 80 || y1 - y0 > 200 || 2 * ey1 + 1 - ey0 >= SELREV_FOND_H) {
        Journal("selection_refus_zone_inattendue", y1 - y0, 0); IDirect3DTexture9_UnlockRect(t, 0); return;
    }
    const int ex0 = x0 - 3 < 0 ? 0 : x0 - 3, ex1 = x1 + 3 > SELREV_X1 - 1 ? SELREV_X1 - 1 : x1 + 3;
    for (int y = ey0; y <= ey1; y++)
        memcpy(p + y * pas + ex0, p + (2 * ey1 + 1 - y) * pas + ex0, (size_t)(ex1 - ex0 + 1));
    const int ox = (x0 + x1) / 2 - SELREV_L / 2, oy = (y0 + y1) / 2 - SELREV_H / 2;
    long poses = 0;
    for (int y = 0; y < SELREV_H; y++) {
        const int ty = oy + y;
        if (ty < 0 || ty >= SELREV_FOND_H) continue;
        for (int x = 0; x < SELREV_L; x++) {
            const int tx = ox + x;
            if (tx < 0 || tx >= SELREV_X1) continue;
            const DWORD c = SELREV_PALETTE[SELREV_INDICES[y][x]];
            const int a = (int)(c >> 24);
            if (!a) continue;
            BYTE *q = p + ty * pas + tx; const DWORD f = g_selPal[*q];
            const int r = ((int)(c >> 16 & 255) * a + (int)(f >> 16 & 255) * (255 - a)) / 255;
            const int g = ((int)(c >> 8 & 255) * a + (int)(f >> 8 & 255) * (255 - a)) / 255;
            const int b = ((int)(c & 255) * a + (int)(f & 255) * (255 - a)) / 255;
            *q = SelPlusProche(r, g, b); poses++;
        }
    }
    IDirect3DTexture9_UnlockRect(t, 0);
    Journal("selection_revolution_pose", poses, 0);
}
static void SelPalette(LPCVOID src)
{
    EnterCriticalSection(&g_csSel);
    if (!g_selPaletteVue) {
        const DWORD *ligne = (const DWORD *)((const BYTE *)src + 128) + 2 * 256;
        for (int i = 0; i < 256; i++) { g_selPal[i] = ligne[i]; g_selRouge[i] = (BYTE)SelRouge(ligne[i]); }
        g_selPaletteVue = TRUE;
    }
    if (g_selFondEnAttente) { SelRetoucher(g_selFondEnAttente); IDirect3DTexture9_Release(g_selFondEnAttente); g_selFondEnAttente = NULL; }
    LeaveCriticalSection(&g_csSel);
}
static void SelFond(IDirect3DTexture9 *t)
{
    EnterCriticalSection(&g_csSel);
    if (g_selPaletteVue) SelRetoucher(t);
    else {
        if (g_selFondEnAttente) IDirect3DTexture9_Release(g_selFondEnAttente);
        g_selFondEnAttente = t; IDirect3DTexture9_AddRef(t);
    }
    LeaveCriticalSection(&g_csSel);
}

#define BARRE_FNV        0x98C9B50Fu
#define TEINTE_BORD      215.0f
#define TEINTE_CENTRE    100.0f
#define DEGRADE_PERTE    0xFFC028

typedef struct { DWORD bord, milieu, centre; } Degrade;
static const Degrade DEGRADES[] = {
    {0, 0, 0},
    {0x8C0014, 0xFF141E, 0xFF78AA},
    {0xC8102E, 0xB0148C, 0x6A2CF0},
    {0xD8102A, 0xFF4FA8, 0x3A6CFF},
    {0xB80C24, 0xE0208C, 0x4A3CE8},
    {0xFF3C78, 0x9A2CE0, 0x2A6AFF},
};
#define NB_DEGRADES (int)(sizeof DEGRADES / sizeof DEGRADES[0])
static int g_degrade = 3;

static const DWORD ORIGINE_MESUREE[5] = {0x189FEE, 0x07E0D6, 0x02E196, 0x209D27, 0x26800E};

static void VersTsv(DWORD c, float *h, float *s, float *v)
{
    const float r = (c >> 16 & 255) / 255.0f, g = (c >> 8 & 255) / 255.0f, b = (c & 255) / 255.0f;
    const float mx = r > g ? (r > b ? r : b) : (g > b ? g : b), mn = r < g ? (r < b ? r : b) : (g < b ? g : b), d = mx - mn;
    *v = mx; *s = mx > 0 ? d / mx : 0;
    if (d <= 0) *h = 0;
    else if (mx == r) *h = 60 * ((g - b) / d);
    else if (mx == g) *h = 60 * ((b - r) / d + 2);
    else *h = 60 * ((r - g) / d + 4);
    if (*h < 0) *h += 360;
}
static DWORD DepuisTsv(DWORD alpha, float h, float s, float v)
{
    while (h >= 360) h -= 360;
    while (h < 0) h += 360;
    const float c = v * s, x = c * (1 - (float)__builtin_fabs(__builtin_fmodf(h / 60, 2) - 1)), m = v - c;
    float r, g, b;
    if (h < 60) { r = c; g = x; b = 0; } else if (h < 120) { r = x; g = c; b = 0; } else if (h < 180) { r = 0; g = c; b = x; }
    else if (h < 240) { r = 0; g = x; b = c; } else if (h < 300) { r = x; g = 0; b = c; } else { r = c; g = 0; b = x; }
    const DWORD R = (DWORD)((r + m) * 255 + 0.5f), G = (DWORD)((g + m) * 255 + 0.5f), B = (DWORD)((b + m) * 255 + 0.5f);
    return alpha | R << 16 | G << 8 | B;
}
static float Canal(DWORD c, int dec) { return (c >> dec & 255) / 255.0f; }

static DWORD CouleurDegrade(float t)
{
    const Degrade *d = &DEGRADES[g_degrade];
    const DWORD a = t < 0.5f ? d->bord : d->milieu, b = t < 0.5f ? d->milieu : d->centre;
    const float u = t < 0.5f ? t * 2 : (t - 0.5f) * 2;
    DWORD c = 0;
    for (int dec = 0; dec <= 16; dec += 8) {
        const float v = Canal(a, dec) + (Canal(b, dec) - Canal(a, dec)) * u;
        c |= (DWORD)(v * 255 + 0.5f) << dec;
    }
    return c;
}

static DWORD Appliquer(DWORD o, DWORD c)
{
    float ho, so, vo, hc, sc, vc;
    VersTsv(o, &ho, &so, &vo); VersTsv(c, &hc, &sc, &vc);
    return DepuisTsv(o & 0xFF000000u, hc, sc * so, vc * vo);
}
static float PositionDegrade(float h)
{
    float t = (TEINTE_BORD - h) / (TEINTE_BORD - TEINTE_CENTRE);
    return t < 0 ? 0 : t > 1 ? 1 : t;
}

static float LuminositeBase(float t)
{
    float pt[5], pv[5];
    for (int i = 0; i < 5; i++) { float h, s; VersTsv(ORIGINE_MESUREE[i], &h, &s, &pv[i]); pt[i] = PositionDegrade(h); }
    if (t <= pt[0]) return pv[0];
    for (int i = 1; i < 5; i++)
        if (t <= pt[i]) return pt[i] > pt[i - 1] ? pv[i - 1] + (pv[i] - pv[i - 1]) * (t - pt[i - 1]) / (pt[i] - pt[i - 1]) : pv[i];
    return pv[4];
}
static DWORD Remplissage(DWORD o)
{
    float h, s, v; VersTsv(o, &h, &s, &v);
    const float t = PositionDegrade(h);
    if (g_degrade == 1) return Appliquer(o, CouleurDegrade(t));
    float hc, sc, vc; VersTsv(CouleurDegrade(t), &hc, &sc, &vc);

    const float vb = LuminositeBase(t);
    float k = vb > 0.05f ? 0.85f * v / vb : v;
    if (k > 1) k = 1;
    return DepuisTsv(o & 0xFF000000u, hc, sc * s, vc * k);
}
static DWORD Perte(DWORD o)
{
    float h, s, v; VersTsv(o, &h, &s, &v);
    return s > 0.2f ? Appliquer(o, DEGRADE_PERTE) : o;
}
static void RecolorerBarre(IDirect3DTexture9 *t)
{
    INT pas; BYTE *p = Verrouiller(t, 256, 8, D3DFMT_A8R8G8B8, &pas);
    if (!p) return;
    for (int y = 0; y < 3; y++) {
        DWORD *ligne = (DWORD *)(p + y * pas);
        for (int x = 0; x < 256; x++) ligne[x] = y < 2 ? Remplissage(ligne[x]) : Perte(ligne[x]);
    }
    IDirect3DTexture9_UnlockRect(t, 0);
}

typedef HRESULT (WINAPI *CreerTexture_t)(IDirect3DDevice9 *, LPCVOID, UINT, UINT, UINT, UINT, DWORD, D3DFORMAT,
                                         D3DPOOL, DWORD, DWORD, D3DCOLOR, void *, PALETTEENTRY *, IDirect3DTexture9 **);
static CreerTexture_t o_creerTexture;
static volatile LONG g_nbTextures;
static int g_banniere = 1;
static HRESULT WINAPI hk_creerTexture(IDirect3DDevice9 *d, LPCVOID src, UINT taille, UINT l, UINT h, UINT niv, DWORD usage,
                                      D3DFORMAT f, D3DPOOL pool, DWORD filtre, DWORD filtreMip, D3DCOLOR cle, void *info,
                                      PALETTEENTRY *pal, IDirect3DTexture9 **out)
{
    HRESULT hr = o_creerTexture(d, src, taille, l, h, niv, usage, f, pool, filtre, filtreMip, cle, info, pal, out);
    if (FAILED(hr) || !out || !*out || !src) return hr;
    if (taille != 8320 && taille != 4276 && taille != 81824 && taille != 86636 && taille != 4224 && taille != SELREV_FOND_TAILLE) return hr;
    const DWORD fnv = Fnv1a((const BYTE *)src, taille);
    int trouve = 0;
    if (taille == 8320 && fnv == BARRE_FNV) { if (g_degrade) RecolorerBarre(*out); trouve = 1; }
    else if (taille == 8320 && fnv == SELREV_PAL_FNV) { SelPalette(src); trouve = 1; }
    else if (taille == SELREV_FOND_TAILLE && fnv == SELREV_FOND_FNV) { SelFond(*out); trouve = 1; }
    for (int i = 0; i < NB_REMPLACEMENTS && !trouve; i++) {
        const Remplacement *r = &REMPLACEMENTS[i];
        if (r->indices == &PATREON_INDICES[0][0] && !g_banniere) continue;
        if (taille == r->tailleIdx && fnv == r->fnvIdx) { EcrireIndices(*out, r); trouve = 1; }
        else if (taille == r->taillePal && fnv == r->fnvPal) { EcrireLigne(*out, r); trouve = 1; }
    }
    if (trouve && InterlockedIncrement(&g_nbTextures) <= 8) Journal("texture", (long)taille, 0);
    return hr;
}
static void Textures_Poser(void)
{
    InitializeCriticalSection(&g_csSel);
    void **entree = (void **)(base + RVA_IAT_D3DX_TEX); DWORD ancien;
    if (!*entree || !VirtualProtect(entree, 4, PAGE_READWRITE, &ancien)) { Journal("textures", 0, 0); return; }
    o_creerTexture = (CreerTexture_t)*entree; *entree = (void *)hk_creerTexture;
    VirtualProtect(entree, 4, ancien, &ancien);
    Journal("textures", 1, 0);
}

#define RVA_LANCER_BOUT   0x1DDF10
#define RVA_ETAT_BOUT     0xF986E8
#define RVA_MODE_BOUT     0xF986EC
#define RVA_APPEL_OVERLAY 0x1DE043
#define RVA_OVERLAY_BOUT  0x1DDE60
#define RVA_RETOUR_MENU1  0x6EA6F5
#define RVA_RETOUR_MENU2  0x6EA770
#define URL_PATREON L"https://www.patreon.com/c/DoA5LRcommunitymod"

static volatile LONG g_origineMenu;
static void __cdecl hk_lancerBoutique(int mode)
{
    const BYTE *retour = (const BYTE *)__builtin_return_address(0);
    InterlockedExchange(&g_origineMenu, retour == base + RVA_RETOUR_MENU1 || retour == base + RVA_RETOUR_MENU2);
    *(volatile DWORD *)(base + RVA_ETAT_BOUT) = 1;
    *(volatile DWORD *)(base + RVA_MODE_BOUT) = (DWORD)mode;
}
static DWORD WINAPI OuvrirPatreon(LPVOID p)
{
    (void)p;
    const INT_PTR r = (INT_PTR)ShellExecuteW(NULL, L"open", URL_PATREON, NULL, NULL, SW_SHOWNORMAL);
    Journal("patreon_ouvert", r > 32, (long)r);
    return 0;
}
typedef char (__cdecl *Overlay_t)(void);
static char __cdecl hk_ouvrirOverlay(void)
{
    if (!g_origineMenu) return ((Overlay_t)(base + RVA_OVERLAY_BOUT))();
    HANDLE t = CreateThread(NULL, 0, OuvrirPatreon, NULL, 0, NULL);
    if (t) CloseHandle(t);
    return 1;
}
static BOOL EcrireCode(BYTE *p, const BYTE *octets, SIZE_T n)
{
    DWORD ancien;
    if (!VirtualProtect(p, n, PAGE_EXECUTE_READWRITE, &ancien)) return FALSE;
    memcpy(p, octets, n);
    VirtualProtect(p, n, ancien, &ancien);
    FlushInstructionCache(GetCurrentProcess(), p, n);
    return TRUE;
}
static void Banniere_Poser(void)
{
    static const BYTE lancer[] = {0x55, 0x8B, 0xEC, 0x8B, 0x45, 0x08, 0xC7, 0x05};
    BYTE *l = base + RVA_LANCER_BOUT, *a = base + RVA_APPEL_OVERLAY;
    LONG rel; memcpy(&rel, a + 1, 4);
    if (memcmp(l, lancer, sizeof lancer) || a[0] != 0xE8 || a + 5 + rel != base + RVA_OVERLAY_BOUT) {
        Journal("banniere", 0, 0); return;
    }
    BYTE appel[5] = {0xE8};
    rel = (LONG)((BYTE *)hk_ouvrirOverlay - (a + 5)); memcpy(appel + 1, &rel, 4);
    BYTE saut[5] = {0xE9};
    const LONG relS = (LONG)((BYTE *)hk_lancerBoutique - (l + 5)); memcpy(saut + 1, &relS, 4);
    const BOOL ok = EcrireCode(a, appel, 5) && EcrireCode(l, saut, 5);
    Journal("banniere", ok, 0);
}

static DWORD WINAPI Demarrer(LPVOID p)
{
    (void)p;
    int ok = 0;
    for (int n = 0; n < 600 && !ok; n++) {
        ok = !memcmp(base + RVA_KTOL_PRINTF, KTOL_PRINTF_BYTES, sizeof KTOL_PRINTF_BYTES);
        if (!ok) Sleep(100);
    }
    if (!ok) { Journal("refus_version_game_exe", 0, 0); return 0; }
    g_degrade = GetPrivateProfileIntA("LifeBar", "Gradient", 3, g_ini);
    if (g_degrade < 0 || g_degrade >= NB_DEGRADES) g_degrade = 3;
    Journal("degrade barre de vie", g_degrade, 0);
    g_banniere = GetPrivateProfileIntA("DOA5Extra", "PatreonBanner", 1, g_ini) != 0;
    if (g_banniere) Banniere_Poser();
    Textures_Poser();
    Titres_Poser();
    return 0;
}
BOOL WINAPI DllMain(HINSTANCE h, DWORD raison, LPVOID r)
{
    (void)r;
    if (raison != DLL_PROCESS_ATTACH) return TRUE;
    DisableThreadLibraryCalls(h);
    base = (BYTE *)GetModuleHandleW(NULL);
    g_t0 = GetTickCount();
    InitializeCriticalSection(&g_cs);
    char chemin[MAX_PATH];
    const DWORD l = GetModuleFileNameA(h, chemin, sizeof chemin);
    char *sep = l && l < sizeof chemin ? strrchr(chemin, '\\') : NULL;
    if (!sep || (size_t)(sep + 1 - chemin) + 16 >= sizeof chemin) return TRUE;
    strcpy(sep + 1, "DOA5Extra.ini");
    strcpy(g_ini, chemin);
    MultiByteToWideChar(CP_ACP, 0, g_ini, -1, g_iniW, MAX_PATH);
    if (GetPrivateProfileIntA("DOA5Extra", "Log", 0, g_ini)) {
        strcpy(sep + 1, "DOA5Extra.log");
        g_log = fopen(chemin, "w");
    }
    Journal("debut " VERSION_EXTRA, VERSION_PATRON, 0);
    HANDLE t = CreateThread(NULL, 0, Demarrer, NULL, 0, NULL);
    if (t) CloseHandle(t);

    Punish_Demarrer(h);
    if (GetPrivateProfileIntA("DOA5Extra", "Hitbox", 1, g_ini)) Hitbox_Demarrer();
    if (GetPrivateProfileIntA("DOA5Extra", "OpenBurst", 1, g_ini)) OpenBurst_Demarrer();
    if (GetPrivateProfileIntA("DOA5Extra", "StatefulCostumes", 1, g_ini)) StatefulCostumes_Demarrer();
    if (GetPrivateProfileIntA("DOA5Extra", "BustPress", 1, g_ini)) BustPress_Demarrer();
    if (GetPrivateProfileIntA("DOA5Extra", "PS4Pressure", 1, g_ini)) PS4Pressure_Demarrer();
    if (GetPrivateProfileIntA("DOA5Extra", "ConsoleMusic", 1, g_ini)) ConsoleMusic_Demarrer();
    if (GetPrivateProfileIntA("DOA5Extra", "SoftBody", 0, g_ini)) SoftBody_Demarrer();
    if (GetPrivateProfileIntA("DOA5Extra", "SoftEngineLR", 1, g_ini)) SoftEngineLR_Demarrer();
    if (GetPrivateProfileIntA("DOA5Extra", "SkinSSS", 1, g_ini)) SkinSSS_Demarrer();
    return TRUE;
}
