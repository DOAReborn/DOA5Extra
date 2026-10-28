#include <windows.h>
#include <d3d9.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define RVA_COUPS_ETAT     0xFC7EE8
#define RVA_COUPS_CENTRE   0xFC8238
#define RVA_COUPS_RAYON    0xFC8008
#define RVA_COUPS_CAPSULE  0xFC8418
#define RVA_COUPS_RAYON_C  0xFC8080
#define RVA_CORPS_TYPE     0xFC8170
#define RVA_CORPS_CENTRE   0xFC8958
#define RVA_CORPS_RAYON    0xFC81B0
#define RVA_CORPS_CAPSULE  0xFC8B58
#define RVA_CORPS_RAYON_C  0xFC7F50
#define RVA_COUP_ACTIF     0xFD05C5
#define RVA_ORDRE          0xFD0C04
#define RVA_VUE            0xFD3110
#define RVA_PROJECTION     0xFD3150
#define RVA_MODE_JEU       0x1088644
#define RVA_MODE_COMBAT    0xD33248
#define RVA_LECTEUR        0xF8F208
#define RVA_MATCHING       0xF8189C
#define MODE_TRAINING      10

static BYTE *base;
static volatile LONG mode;
static volatile LONG versionOk;
static int touche = VK_F6;

static FILE *g_log; static DWORD g_t0; static CRITICAL_SECTION g_logCs;
#ifndef DOA5EXTRA
static long g_logTaille;
#endif
#ifdef DOA5EXTRA
#include "../commun/extra.h"
static void Journal(const char *libelle, long a, long b, long c, long d)
{
    char l[200];
    snprintf(l, sizeof l, "%s %ld %ld %ld %ld", libelle, a, b, c, d);
    Extra_Journal("Hitbox", l);
}
#else
static void Journal(const char *libelle, long a, long b, long c, long d)
{
    if (!g_log || g_logTaille > 256 * 1024) return;
    EnterCriticalSection(&g_logCs);
    const int n = fprintf(g_log, "t=%lu Hitbox %s %ld %ld %ld %ld\n", (unsigned long)(GetTickCount() - g_t0), libelle, a, b, c, d);
    if (n > 0) g_logTaille += n;
    fflush(g_log);
    LeaveCriticalSection(&g_logCs);
}
#endif

static DWORD L32(DWORD rva) { return *(volatile DWORD *)(base + rva); }
static BOOL Actif(void)
{
    return L32(RVA_LECTEUR) == 0 && L32(RVA_MATCHING) == 0 &&
           L32(RVA_MODE_JEU) == MODE_TRAINING && L32(RVA_MODE_COMBAT) == MODE_TRAINING;
}

static BOOL VersionAttendue(void)
{
    static const BYTE test[]   = {0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x30, 0x56, 0x57, 0x8B, 0x7D, 0x08, 0x8B};
    static const BYTE coups[]  = {0x55, 0x8B, 0xEC, 0x8B, 0x45, 0x10, 0x53, 0x56, 0x57, 0x8B, 0x7D, 0x08};
    static const BYTE camera[] = {0x53, 0x8B, 0xDC, 0x83, 0xEC, 0x08, 0x83, 0xE4, 0xF0, 0x83, 0xC4, 0x04};
    return !memcmp(base + 0x2EF000, test, sizeof test) && !memcmp(base + 0x2FCA20, coups, sizeof coups) &&
           !memcmp(base + 0x37B1B0, camera, sizeof camera);
}
static const BYTE *Ordre(int p)
{
    const BYTE *o = *(BYTE *const *)(base + RVA_ORDRE + p * 0x6C8);
    return (o && !IsBadReadPtr(o, 16)) ? o : NULL;
}

typedef struct { float x, y, z; } V3;
typedef struct { float x, y, z, rhw; D3DCOLOR c; } Sommet;
#define FVF_SOMMET (D3DFVF_XYZRHW | D3DFVF_DIFFUSE)
static float VP[16];
static V3 droite, haut, avant;
static float largeur, hauteur;
static void Multiplier(const float *a, const float *b, float *r)
{
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            r[i * 4 + j] = a[i * 4] * b[j] + a[i * 4 + 1] * b[4 + j] + a[i * 4 + 2] * b[8 + j] + a[i * 4 + 3] * b[12 + j];
}
static BOOL Ecran(V3 p, float *x, float *y)
{
    const float cx = p.x * VP[0] + p.y * VP[4] + p.z * VP[8] + VP[12];
    const float cy = p.x * VP[1] + p.y * VP[5] + p.z * VP[9] + VP[13];
    const float cw = p.x * VP[3] + p.y * VP[7] + p.z * VP[11] + VP[15];
    if (cw < 0.01f) return FALSE;
    *x = (cx / cw + 1.0f) * 0.5f * largeur;
    *y = (1.0f - cy / cw) * 0.5f * hauteur;
    return TRUE;
}

#define MAX_SOMMETS 6144
static Sommet tris[MAX_SOMMETS], lignes[MAX_SOMMETS]; static int nTris, nLignes;
static IDirect3DDevice9 *devCourant;
static void ViderTris(void)
{
    if (nTris >= 3) IDirect3DDevice9_DrawPrimitiveUP(devCourant, D3DPT_TRIANGLELIST, nTris / 3, tris, sizeof(Sommet));
    nTris = 0;
}
static void ViderLignes(void)
{
    if (nLignes >= 2) IDirect3DDevice9_DrawPrimitiveUP(devCourant, D3DPT_LINELIST, nLignes / 2, lignes, sizeof(Sommet));
    nLignes = 0;
}
static Sommet S(float x, float y, D3DCOLOR c) { Sommet s = {x, y, 0.0f, 1.0f, c}; return s; }
static void Ligne(float x1, float y1, float x2, float y2, D3DCOLOR c, int epais)
{
    static const float dec[3][2] = {{0, 0}, {1, 0}, {0, 1}};
    for (int k = 0; k < epais && k < 3; k++) {
        if (nLignes + 2 > MAX_SOMMETS) ViderLignes();
        lignes[nLignes++] = S(x1 + dec[k][0], y1 + dec[k][1], c); lignes[nLignes++] = S(x2 + dec[k][0], y2 + dec[k][1], c);
    }
}
static void Triangle(float x1, float y1, float x2, float y2, float x3, float y3, D3DCOLOR c)
{
    if (nTris + 3 > MAX_SOMMETS) ViderTris();
    tris[nTris++] = S(x1, y1, c); tris[nTris++] = S(x2, y2, c); tris[nTris++] = S(x3, y3, c);
}
static D3DCOLOR Alpha(D3DCOLOR c, int a) { return (c & 0x00FFFFFF) | ((DWORD)(a < 0 ? 0 : a > 255 ? 255 : a) << 24); }
static V3 Plus(V3 a, V3 b, float k) { V3 r = {a.x + b.x * k, a.y + b.y * k, a.z + b.z * k}; return r; }
#define SEGMENTS 32
static float cosT[SEGMENTS + 1], sinT[SEGMENTS + 1];

static void Disque(V3 c, float r, D3DCOLOR col, int aFond, int aBord, int epais)
{
    float px[SEGMENTS + 1], py[SEGMENTS + 1], cx, cy;
    if (!Ecran(c, &cx, &cy)) return;
    for (int i = 0; i <= SEGMENTS; i++)
        if (!Ecran(Plus(Plus(c, droite, r * cosT[i]), haut, r * sinT[i]), &px[i], &py[i])) return;
    for (int i = 0; i < SEGMENTS; i++) {
        if (aFond) Triangle(cx, cy, px[i], py[i], px[i + 1], py[i + 1], Alpha(col, aFond));
        if (aBord) Ligne(px[i], py[i], px[i + 1], py[i + 1], Alpha(col, aBord), epais);
    }
}

static void Capsule(V3 a, V3 b, float r, D3DCOLOR col, int aFond, int aBord, int epais)
{
    Disque(a, r, col, aFond, aBord, epais); Disque(b, r, col, aFond, aBord, epais);
    V3 d = {b.x - a.x, b.y - a.y, b.z - a.z};
    V3 n = {d.y * avant.z - d.z * avant.y, d.z * avant.x - d.x * avant.z, d.x * avant.y - d.y * avant.x};
    const float l = sqrtf(n.x * n.x + n.y * n.y + n.z * n.z);
    if (l < 1e-6f) return;
    n.x /= l; n.y /= l; n.z /= l;
    float x[4], y[4];
    if (!Ecran(Plus(a, n, r), &x[0], &y[0]) || !Ecran(Plus(b, n, r), &x[1], &y[1]) ||
        !Ecran(Plus(b, n, -r), &x[2], &y[2]) || !Ecran(Plus(a, n, -r), &x[3], &y[3])) return;
    if (aFond) { Triangle(x[0], y[0], x[1], y[1], x[2], y[2], Alpha(col, aFond)); Triangle(x[0], y[0], x[2], y[2], x[3], y[3], Alpha(col, aFond)); }
    if (aBord) { Ligne(x[0], y[0], x[1], y[1], Alpha(col, aBord), epais); Ligne(x[3], y[3], x[2], y[2], Alpha(col, aBord), epais); }
}
static V3 Lire3(DWORD rva) { const float *f = (const float *)(base + rva); V3 v = {f[0], f[1], f[2]}; return v; }
static float LireF(DWORD rva) { return *(const float *)(base + rva); }
static BOOL Rayon(float r) { return r > 0.0f && r < 10.0f; }

static int style = 1;
static int trainee = 1;

#define COUL_HAUT    D3DCOLOR_XRGB(0xFF, 0xD2, 0x3C)
#define COUL_MILIEU  D3DCOLOR_XRGB(0x3C, 0xC8, 0xFF)
#define COUL_BAS     D3DCOLOR_XRGB(0xB4, 0x6E, 0xFF)
#define COUL_COUP    D3DCOLOR_XRGB(0xFF, 0x2D, 0x3C)
#define COUL_BALAYE  D3DCOLOR_XRGB(0xFF, 0x96, 0x1E)
static D3DCOLOR CouleurCorps(int k) { return k == 1 ? COUL_HAUT : (k == 3 || k == 7 || k == 9 || k == 13) ? COUL_BAS : COUL_MILIEU; }

typedef struct { DWORD t; BYTE capsule; V3 a, b; float r; } Trace;
#define MAX_TRACES 96
#define DUREE_TRAINEE 200
static Trace traces[MAX_TRACES]; static int iTrace;
static DWORD derniereTrace;

static void DessinerCoup(int capsule, V3 a, V3 b, float r, int force)
{
    const D3DCOLOR c = capsule ? COUL_BALAYE : COUL_COUP;
    if (!style) { if (capsule) Capsule(a, b, r, c, 0, 255, 2); else Disque(a, r, c, 0, 255, 2); return; }
    const int fond = 0x70 * force / 255, bord = force, halo = 0x50 * force / 255;
    if (capsule) { Capsule(a, b, r * 1.12f, c, 0, halo, 3); Capsule(a, b, r, c, fond, bord, 2); }
    else { Disque(a, r * 1.12f, c, 0, halo, 3); Disque(a, r, c, fond, bord, 2); }
}
static void DessinerCorps(int capsule, V3 a, V3 b, float r, D3DCOLOR c)
{
    if (!style) { if (capsule) Capsule(a, b, r, c, 0, 255, 1); else Disque(a, r, c, 0, 255, 1); return; }
    if (capsule) Capsule(a, b, r, c, 0x1C, 0x8C, 1); else Disque(a, r, c, 0x1C, 0x8C, 1);
}

static DWORD dernierReleve;
static void Dessiner(IDirect3DDevice9 *d)
{
    const float *V = (const float *)(base + RVA_VUE), *P = (const float *)(base + RVA_PROJECTION);
    Multiplier(V, P, VP);
    droite.x = V[0]; droite.y = V[4]; droite.z = V[8];
    haut.x = V[1];   haut.y = V[5];   haut.z = V[9];
    avant.x = V[2];  avant.y = V[6];  avant.z = V[10];
    long nCoups[2] = {0, 0}, nCorps = 0;
    const DWORD now = GetTickCount();
    const BOOL noter = trainee && now - derniereTrace >= 15;
    devCourant = d; nTris = nLignes = 0;
    for (int p = 0; p < 2; p++) {
        const BYTE *o = Ordre(p);
        if (!o) continue;
        if (mode == 1)
            for (int k = 0; k < 16; k++) {
                if (o[k] >= 16) continue;
                const BYTE t = base[RVA_CORPS_TYPE + (p * 16 + o[k]) * 2];
                const int i = p * 16 + k;
                if (t == 1) { const float r = LireF(RVA_CORPS_RAYON + i * 4); const V3 c = Lire3(RVA_CORPS_CENTRE + i * 0x10);
                    if (Rayon(r)) { DessinerCorps(0, c, c, r, CouleurCorps(k)); nCorps++; } }
                else if (t == 3) { const float r = LireF(RVA_CORPS_RAYON_C + i * 4);
                    if (Rayon(r)) { DessinerCorps(1, Lire3(RVA_CORPS_CAPSULE + i * 0x20), Lire3(RVA_CORPS_CAPSULE + i * 0x20 + 0x10), r, CouleurCorps(k)); nCorps++; } }
            }
    }

    if (trainee && style)
        for (int j = 0; j < MAX_TRACES; j++) {
            const DWORD age = now - traces[j].t;
            if (!traces[j].t || age >= DUREE_TRAINEE) continue;
            DessinerCoup(traces[j].capsule, traces[j].a, traces[j].b, traces[j].r, (int)(150 * (DUREE_TRAINEE - age) / DUREE_TRAINEE));
        }
    for (int p = 0; p < 2; p++) {
        const BYTE *o = Ordre(p);
        if (!o || base[RVA_COUP_ACTIF + p * 0x6C8] != 1) continue;
        for (int k = 0; k < 15; k++) {
            if (o[k] >= 15) continue;
            const BYTE e = base[RVA_COUPS_ETAT + (p * 15 + o[k]) * 2];
            const int i = p * 15 + k;
            Trace tr = {now, 0, {0, 0, 0}, {0, 0, 0}, 0};
            if (e == 1) { tr.r = LireF(RVA_COUPS_RAYON + i * 4); tr.a = tr.b = Lire3(RVA_COUPS_CENTRE + i * 0x10); }
            else if (e == 3) { tr.capsule = 1; tr.r = LireF(RVA_COUPS_RAYON_C + i * 4);
                tr.a = Lire3(RVA_COUPS_CAPSULE + i * 0x20); tr.b = Lire3(RVA_COUPS_CAPSULE + i * 0x20 + 0x10); }
            else continue;
            if (!Rayon(tr.r)) continue;
            DessinerCoup(tr.capsule, tr.a, tr.b, tr.r, 255); nCoups[p]++;
            if (noter) { traces[iTrace] = tr; iTrace = (iTrace + 1) % MAX_TRACES; }
        }
    }
    if (noter) derniereTrace = now;
    ViderTris(); ViderLignes();
    if (now - dernierReleve >= 3000) {
        dernierReleve = now;
        float x = -1, y = -1;
        if (Ordre(0)) Ecran(Lire3(RVA_CORPS_CENTRE), &x, &y);
        Journal("releve coups_j1 coups_j2 corps x_y", nCoups[0], nCoups[1], nCorps, (long)x * 10000 + (long)y);
    }
}

typedef HRESULT (WINAPI *Present_t)(IDirect3DDevice9 *, const RECT *, const RECT *, HWND, const RGNDATA *);
typedef HRESULT (WINAPI *CreateDevice_t)(IDirect3D9 *, UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS *, IDirect3DDevice9 **);
typedef struct { void **vt; Present_t present; } Accroche;
static Accroche accroches[4]; static volatile LONG nAccroches;
static CreateDevice_t createOrig;

static void DessinerImage(IDirect3DDevice9 *d)
{
    IDirect3DSurface9 *bb = NULL;
    if (FAILED(IDirect3DDevice9_GetBackBuffer(d, 0, 0, D3DBACKBUFFER_TYPE_MONO, &bb)) || !bb) return;
    D3DSURFACE_DESC sd; IDirect3DSurface9_GetDesc(bb, &sd);
    largeur = (float)sd.Width; hauteur = (float)sd.Height;
    IDirect3DStateBlock9 *sb = NULL;
    if (FAILED(IDirect3DDevice9_CreateStateBlock(d, D3DSBT_ALL, &sb))) { IDirect3DSurface9_Release(bb); return; }
    IDirect3DSurface9 *rt = NULL; IDirect3DDevice9_GetRenderTarget(d, 0, &rt);
    IDirect3DDevice9_SetRenderTarget(d, 0, bb);
    D3DVIEWPORT9 vp = {0, 0, sd.Width, sd.Height, 0.0f, 1.0f}; IDirect3DDevice9_SetViewport(d, &vp);
    IDirect3DDevice9_BeginScene(d);
    IDirect3DDevice9_SetVertexShader(d, NULL); IDirect3DDevice9_SetPixelShader(d, NULL);
    IDirect3DDevice9_SetFVF(d, FVF_SOMMET);
    for (int i = 0; i < 8; i++) IDirect3DDevice9_SetTexture(d, i, NULL);
    IDirect3DDevice9_SetTextureStageState(d, 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    IDirect3DDevice9_SetTextureStageState(d, 0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
    IDirect3DDevice9_SetTextureStageState(d, 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    IDirect3DDevice9_SetTextureStageState(d, 0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
    IDirect3DDevice9_SetTextureStageState(d, 1, D3DTSS_COLOROP, D3DTOP_DISABLE);
    IDirect3DDevice9_SetTextureStageState(d, 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
    IDirect3DDevice9_SetRenderState(d, D3DRS_ZENABLE, D3DZB_FALSE);
    IDirect3DDevice9_SetRenderState(d, D3DRS_ZWRITEENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(d, D3DRS_ALPHABLENDENABLE, TRUE);
    IDirect3DDevice9_SetRenderState(d, D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    IDirect3DDevice9_SetRenderState(d, D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    IDirect3DDevice9_SetRenderState(d, D3DRS_BLENDOP, D3DBLENDOP_ADD);
    IDirect3DDevice9_SetRenderState(d, D3DRS_ALPHATESTENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(d, D3DRS_CULLMODE, D3DCULL_NONE);
    IDirect3DDevice9_SetRenderState(d, D3DRS_LIGHTING, FALSE);
    IDirect3DDevice9_SetRenderState(d, D3DRS_FOGENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(d, D3DRS_STENCILENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(d, D3DRS_SCISSORTESTENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(d, D3DRS_COLORWRITEENABLE, 0xF);
    IDirect3DDevice9_SetRenderState(d, D3DRS_SRGBWRITEENABLE, FALSE);
    Dessiner(d);
    IDirect3DDevice9_EndScene(d);
    IDirect3DStateBlock9_Apply(sb); IDirect3DStateBlock9_Release(sb);
    if (rt) { IDirect3DDevice9_SetRenderTarget(d, 0, rt); IDirect3DSurface9_Release(rt); }
    IDirect3DSurface9_Release(bb);
}
static HRESULT WINAPI PresentHook(IDirect3DDevice9 *d, const RECT *a, const RECT *b, HWND w, const RGNDATA *r)
{
    void **vt = *(void ***)d; Present_t orig = NULL;
    for (LONG i = 0; i < nAccroches; i++) if (accroches[i].vt == vt) orig = accroches[i].present;
    if (versionOk == 0) { versionOk = VersionAttendue() ? 1 : -1; Journal(versionOk == 1 ? "version_ok" : "REFUS_version", 0, 0, 0, 0); }
    if (versionOk == 1 && mode && Actif()) DessinerImage(d);
    return orig ? orig(d, a, b, w, r) : D3D_OK;
}
static void AccrocherDevice(IDirect3DDevice9 *dev)
{
    void **vt = *(void ***)dev; DWORD old;
    for (LONG i = 0; i < nAccroches; i++) if (accroches[i].vt == vt) return;
    if (nAccroches >= 4 || vt[17] == (void *)PresentHook) return;
    if (!VirtualProtect(&vt[17], 4, PAGE_EXECUTE_READWRITE, &old)) return;

    void *avant;
    do { avant = vt[17]; accroches[nAccroches].vt = vt; accroches[nAccroches].present = (Present_t)avant; }
    while (InterlockedCompareExchangePointer(&vt[17], (void *)PresentHook, avant) != avant);
    InterlockedIncrement(&nAccroches);
    VirtualProtect(&vt[17], 4, old, &old);
    Journal("affichage_accroche", nAccroches, 0, 0, 0);
}

static BOOL Lisible4(const void *p)
{
    MEMORY_BASIC_INFORMATION m;
    if (!p || !VirtualQuery(p, &m, sizeof m) || m.State != MEM_COMMIT || (m.Protect & (PAGE_NOACCESS | PAGE_GUARD))) return FALSE;
    return (const BYTE *)p + 4 <= (const BYTE *)m.BaseAddress + m.RegionSize;
}
static void AccrocherDeviceSur(IDirect3DDevice9 *dev)
{
    static volatile LONG verrou;
    while (InterlockedExchange(&verrou, 1)) Sleep(0);
    AccrocherDevice(dev);
    InterlockedExchange(&verrou, 0);
}
static DWORD WINAPI FiletDevice(LPVOID p)
{
    void **vt9 = (void **)p;
    for (int n = 0; n < 1800; n++) {
        BYTE *g = Lisible4(base + 0x2080938) ? *(BYTE **)(base + 0x2080938) : NULL;
        if (g && Lisible4(g + 0xC4) && Lisible4(g + 0xF8)) {
            void *d3d = *(void **)(g + 0xF8), *dev = *(void **)(g + 0xC4);
            if (d3d && dev && Lisible4(d3d) && Lisible4(dev) && *(void ***)d3d == vt9 && Lisible4(*(void ***)dev + 17)) {
                AccrocherDeviceSur((IDirect3DDevice9 *)dev);
                Journal("affichage_filet", 1, 0, 0, 0);
                return 0;
            }
        }
        Sleep(100);
    }
    return 0;
}
static HRESULT WINAPI CreateDeviceHook(IDirect3D9 *d3d, UINT a, D3DDEVTYPE t, HWND w, DWORD f, D3DPRESENT_PARAMETERS *pp, IDirect3DDevice9 **out)
{
    const HRESULT hr = createOrig(d3d, a, t, w, f, pp, out);
    if (SUCCEEDED(hr) && out && *out && t == D3DDEVTYPE_HAL) AccrocherDeviceSur(*out);
    return hr;
}
static void InstallerD3D(void)
{
    IDirect3D9 *d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d) { Journal("REFUS_d3d9", 0, 0, 0, 0); return; }
    void **vt = *(void ***)d3d; DWORD old;
    if (vt[16] != (void *)CreateDeviceHook && VirtualProtect(&vt[16], 4, PAGE_EXECUTE_READWRITE, &old)) {
        void *avant;
        do { avant = vt[16]; createOrig = (CreateDevice_t)avant; }
        while (InterlockedCompareExchangePointer(&vt[16], (void *)CreateDeviceHook, avant) != avant);
        VirtualProtect(&vt[16], 4, old, &old);
    }
    IDirect3D9_Release(d3d);
    HANDLE h = CreateThread(NULL, 0, FiletDevice, vt, 0, NULL);
    if (h) CloseHandle(h);
}

static DWORD WINAPI FilTouche(LPVOID unused)
{
    (void)unused;
    BOOL avant_appui = FALSE;
    for (;;) {
        Sleep(30);
        DWORD pid = 0; GetWindowThreadProcessId(GetForegroundWindow(), &pid);
        const BOOL appui = pid == GetCurrentProcessId() && (GetAsyncKeyState(touche) & 0x8000);
        if (appui && !avant_appui) { mode = (mode + 1) % 3; Journal("mode", mode, Actif(), 0, 0); }
        avant_appui = appui;
    }
    return 0;
}

static DWORD WINAPI Demarrer(LPVOID param)
{
    const HMODULE moi = (HMODULE)param;
    base = (BYTE *)GetModuleHandleW(NULL);
    for (int i = 0; i <= SEGMENTS; i++) { const float a = 6.2831853f * i / SEGMENTS; cosT[i] = cosf(a); sinT[i] = sinf(a); }
    wchar_t ini[MAX_PATH], journal[MAX_PATH];
#ifdef DOA5EXTRA
    (void)moi; (void)journal;
    wcscpy(ini, Extra_IniW());
    touche = (int)GetPrivateProfileIntW(L"Hitbox", L"Touche", VK_F6, ini);
    if (touche <= 0 || touche > 0xFE) touche = VK_F6;
    style = GetPrivateProfileIntW(L"Hitbox", L"Style", 1, ini) ? 1 : 0;
    trainee = GetPrivateProfileIntW(L"Hitbox", L"Trainee", 1, ini) ? 1 : 0;
    if (0) {
#else
    if (GetModuleFileNameW(moi, ini, MAX_PATH)) {
#endif
        wchar_t *s = wcsrchr(ini, L'\\');
        if (s) {
            s[1] = 0; wcscpy(journal, ini);
            wcscat(ini, L"DOA5LR-Hitbox.ini"); wcscat(journal, L"DOA5LR-Hitbox.log");
            touche = (int)GetPrivateProfileIntW(L"Hitbox", L"Touche", VK_F6, ini);
            if (touche <= 0 || touche > 0xFE) touche = VK_F6;
            style = GetPrivateProfileIntW(L"Hitbox", L"Style", 1, ini) ? 1 : 0;
            trainee = GetPrivateProfileIntW(L"Hitbox", L"Trainee", 1, ini) ? 1 : 0;
            InitializeCriticalSection(&g_logCs);
            if (GetPrivateProfileIntW(L"Hitbox", L"Log", 0, ini)) g_log = _wfopen(journal, L"w");
        }
    }
    g_t0 = GetTickCount();
    Journal("demarrage version_0.4 touche style trainee", touche, style, trainee, 0);
    InstallerD3D();
    FilTouche(NULL);
    return 0;
}

#ifdef DOA5EXTRA
void Hitbox_Demarrer(void)
{
    HANDLE t = CreateThread(NULL, 0, Demarrer, NULL, 0, NULL);
    if (t) CloseHandle(t);
}
#else
BOOL WINAPI DllMain(HINSTANCE h, DWORD raison, LPVOID r)
{
    (void)r;
    if (raison == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(h);
        HANDLE t = CreateThread(NULL, 0, Demarrer, h, 0, NULL);
        if (t) CloseHandle(t);
    }
    return TRUE;
}
#endif
