#include <windows.h>
#include <d3d9.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#ifdef PUNISH_TEST
#define VERSION_PUNISH "0.67-test (diagnostic)"
#define LIMITE_JOURNAL (256 * 1024)
#else
#define VERSION_PUNISH "0.67-test"
#define LIMITE_JOURNAL (64 * 1024)
#endif

#define RVA_KTOL_PRINTF  0x846010
static const BYTE KTOL_PRINTF_BYTES[] = {0x55, 0x8B, 0xEC, 0x81, 0xEC, 0x04, 0x02, 0x00, 0x00};
#define RVA_MODE_JEU     0x1088644
#define RVA_MODE_COMBAT  0xD33248
#define RVA_LECTEUR      0xF8F208
#define RVA_MATCHING     0xF8189C
#define RVA_COMBAT_EN_COURS 0xDD94D4
#define RVA_JOUEURS      0xFD0540
#define PAS_JOUEUR       0x6C8
#define OFS_MAINTENU     0x678
#define TOUCHE_H         1
#define OFS_PHASE        0x78
#define PHASE_ACTIVE     2
#define RVA_DEBUT_ACTIF  0xF99698
#define RVA_ETAT_ATTAQUE 0xFCEE98
#define PAS_ATTAQUE      0x488
#define RVA_FRAME_ANIM   0xFCAB24
#define MODE_TRAINING    10
#define MOUVEMENT_PH     363
#define ECART_6PH        2
#define ACTION_PRISE_PH  170

enum { ETAT_NEUTRE = 0, ETAT_ATTAQUE = 3, ETAT_PRISE = 4, ETAT_TOUCHE = 8, ETAT_TOUCHE2 = 9, ETAT_CHOPPE = 16 };

static BYTE *base;

#ifndef DOA5EXTRA
static FILE *g_log;
static long g_octets;
#endif
static DWORD g_t0;
static CRITICAL_SECTION g_csLog;

#ifdef DOA5EXTRA
#include "../commun/extra.h"
static void Journal(const char *libelle, long a, long b, long c)
{
    char l[200];
    snprintf(l, sizeof l, "%s %ld %ld %ld", libelle, a, b, c);
    Extra_Journal("PunishTraining", l);
}
#else
static void Journal(const char *libelle, long a, long b, long c)
{
    if (!g_log || g_octets > LIMITE_JOURNAL) return;
    EnterCriticalSection(&g_csLog);
    const int n = fprintf(g_log, "t=%lu PunishTraining %s %ld %ld %ld\n", (unsigned long)(GetTickCount() - g_t0), libelle, a, b, c);
    if (n > 0) g_octets += n;
    fflush(g_log);
    LeaveCriticalSection(&g_csLog);
}
#endif
#ifdef PUNISH_TEST
#define CONTROLE(libelle, a, b, c) Journal(libelle, a, b, c)
#else
#define CONTROLE(libelle, a, b, c) ((void)0)
#endif

typedef struct {
    BYTE humain, perso, etat, slot28, slot100, b7e, accroupi;
    WORD cpt, cpt2;
    DWORD action, mouvement;
    int frameAnim;
    short sante;
    WORD angle;
} Combattant;
static void Lire(int p, Combattant *c)
{
    const BYTE *j = base + RVA_JOUEURS + p * PAS_JOUEUR;
    memset(c, 0, sizeof *c);
    c->humain = j[0];
    c->perso = j[4];
    c->etat = j[0x70];
    c->slot28 = j[0x28];
    c->slot100 = j[0x100];
    c->b7e = j[0x7E];
    c->accroupi = j[0x7F];
    memcpy(&c->action, j + 0x6C, 4);
    memcpy(&c->mouvement, j + 0x64, 4);
    memcpy(&c->sante, j + 0x2C, 2);
    memcpy(&c->cpt, base + RVA_ETAT_ATTAQUE + p * PAS_ATTAQUE, 2);
    memcpy(&c->cpt2, base + RVA_ETAT_ATTAQUE + p * PAS_ATTAQUE + 2, 2);
    memcpy(&c->frameAnim, base + RVA_FRAME_ANIM + p * 0x3C, 4);
    memcpy(&c->angle, j + 0x24, 2);
}
#define RVA_ANGLE_REFERENCE 0xFB8E24

static BOOL DeDos(int p, const Combattant *c)
{
    WORD ref; memcpy(&ref, base + RVA_ANGLE_REFERENCE, 2);
    const WORD a = (WORD)(c->angle - ref + (p ? 0x8000 : 0));
    return (WORD)(a + 0x4000) > 0x8000;
}
static BOOL HorsLigne(void)
{
    return *(DWORD *)(base + RVA_LECTEUR) == 0 && *(DWORD *)(base + RVA_MATCHING) == 0;
}

static BOOL Training_Libre(void)
{
    return *(DWORD *)(base + RVA_MODE_JEU) == MODE_TRAINING && *(DWORD *)(base + RVA_MODE_COMBAT) == MODE_TRAINING;
}

static BOOL Actif(void);
static BOOL DefiPossible(void) { return HorsLigne() && Training_Libre() && Actif(); }

static BOOL Actif(void)
{
#ifdef PUNISH_TEST
    const DWORD c = *(DWORD *)(base + RVA_COMBAT_EN_COURS);
    return c >= 1 && c <= 3;
#else
    return HorsLigne() && Training_Libre();
#endif
}

#define RVA_SECTIONS        0x2053AD8
#define RVA_SECTIONS_COMMUN 0x2053D48
#define RVA_FICHIERS        0x20539B0
#define RVA_LONGUEURS       0xD03460
static BOOL Lu(DWORD adresse, void *dest, DWORD n)
{
    SIZE_T lu = 0;
    return adresse >= 0x10000 && ReadProcessMemory(GetCurrentProcess(), (LPCVOID)(ULONG_PTR)adresse, dest, n, &lu) && lu == n;
}
static DWORD L32(DWORD adresse) { DWORD v = 0; Lu(adresse, &v, 4); return v; }
static short L16(DWORD adresse) { short v = -1; Lu(adresse, &v, 2); return v; }
static DWORD Section(int slot, int n) { return L32((DWORD)(ULONG_PTR)base + RVA_SECTIONS + (slot * 0x27 + n) * 4); }
static DWORD SectionCommune(int n) { return L32((DWORD)(ULONG_PTR)base + RVA_SECTIONS_COMMUN + n * 4); }

static int NbArguments(short op)
{
    DWORD t = (DWORD)(ULONG_PTR)base + RVA_LONGUEURS;
    for (int i = 0; i < 400; i++, t += 4) {
        const short o = L16(t);
        if (o == -2) return 0;
        if (o == op) return L16(t + 2);
    }
    return 0;
}

static DWORD Suivante(DWORD p)
{
    const short op = L16(p);
    if (op == -1) return 0;
    int k;
    switch (op) {
    case 0xA0: k = 4; break;
    case 0xA1: case 0xAA: k = 0; break;
    case 0xAF: k = 5; break;
    default: return p + 2 * (1 + NbArguments(op));
    }
    short n = L16(p + 2);
    if (n < 0) n &= 0xFF;
    return p + 2 * (k + 2 + n * 2);
}

static DWORD Script(int slot, DWORD mouvement)
{
    DWORD pointeurs, ids;
    if (mouvement < 0x2000) { pointeurs = Section(slot, 0x15); ids = Section(slot, 0x18); }
    else { pointeurs = SectionCommune(0x15); ids = SectionCommune(0x18); }
    mouvement &= 0x1FFF;
    if (!pointeurs) return 0;
    if (!ids) return mouvement < 0x400 ? L32(pointeurs + mouvement * 4) : 0;
    for (int i = 0; i < 0x2000; i++) {
        const DWORD id = L32(ids + i * 4);
        if (id == mouvement) return L32(pointeurs + i * 4);
        if (id == 0xFFFFFFFF) break;
    }
    return 0;
}

static int FrameOp(DWORD script, short cible, int *nbBlocs, int *vitesse, int *debut)
{
    int frame = 0;
    *nbBlocs = 0; *vitesse = -1; *debut = -1;
    for (int i = 0; script && i < 2000; i++, script = Suivante(script)) {
        const short op = L16(script);
        if (op == -1) return -1;
        if (op == 0 && *debut < 0) *debut = L16(script + 4);
        else if (op == 1) { frame = L16(script + 2); (*nbBlocs)++; }
        else if (op == 2) *vitesse = L16(script + 2);
        else if (op == cible) return frame;
    }
    return -1;
}

static int FrameChoppe(DWORD script)
{
    int t = 0, f = 0, acc = 0, g0 = -1;
    BOOL attendu = FALSE;
    for (int i = 0; script && i < 2000; i++, script = Suivante(script)) {
        const short op = L16(script);
        if (op == -1) return -1;
        if (op == 0x21) return g0 > 0 ? g0 : t;
        g0 = -1;
        if (op == 1) {
            const int n = L16(script + 2);
            if (n > f) t += n - f;
            f = n; attendu = TRUE; acc = 0;
        } else if (op == 8) acc = L16(script + 4) == 0 && L16(script + 2) > 1 ? L16(script + 2) : 0;
        else if (op == 0) {
            const int g = L16(script + 4);
            if (acc && g > f) { t += (g - f + acc - 1) / acc; f = g; }
            else { if (attendu) f = g; g0 = g; }
            acc = 0;
        }
        else acc = 0;
    }
    return -1;
}
#ifdef PUNISH_TEST

static void JournalScriptChoppe(DWORD script, DWORD id)
{
    for (int i = 0; script && i < 32; i++, script = Suivante(script)) {
        const short op = L16(script);
        if (op == -1) return;
        Journal("choppe_op", (long)(WORD)op | (long)id << 16, L16(script + 2), L16(script + 4));
        if (op == 0x21) return;
    }
}
#endif

static DWORD Action(int slot, DWORD id)
{
    static const int BANQUES[] = {0, 1, 2, 5, 3, 4};
    const DWORD tranche = id / 8000, index = id % 8000;
    if (tranche > 5) return 0;
    const int banque = BANQUES[tranche];
    if (banque == 5) { const DWORD c = SectionCommune(0x23); return c ? c + index * 0xC : 0; }
    const DWORD table = Section(slot, 0x21), fichier = L32(L32((DWORD)(ULONG_PTR)base + RVA_FICHIERS + slot * 4));
    if (!table || !fichier) return 0;
    return L32(table + banque * 4) + fichier + index * 0xC;
}

#define RVA_FICHIER_COMMUN 0x2053868
static int Propriete(int slot, DWORD id, int cle)
{
    const DWORD a = Action(slot, id);
    if (!a) return -1;
    const DWORD rel = L32(a + 4);
    if (!rel) return -1;
    const DWORD fichier = id / 8000 == 3 ? L32(L32((DWORD)(ULONG_PTR)base + RVA_FICHIER_COMMUN))
                                         : L32(L32((DWORD)(ULONG_PTR)base + RVA_FICHIERS + slot * 4));
    if (!fichier) return -1;
    DWORD q = fichier + rel;
    for (int i = 0; i < 256; i++, q += 8) {
        const int k = (int)L32(q);
        if (k == -1) return -1;
        if (k == cle) return (int)L32(q + 4);
    }
    return -1;
}

static BOOL ProprietePresente(int slot, DWORD id, int cle)
{
    const DWORD a = Action(slot, id);
    if (!a) return FALSE;
    const DWORD rel = L32(a + 4);
    if (!rel) return FALSE;
    const DWORD fichier = id / 8000 == 3 ? L32(L32((DWORD)(ULONG_PTR)base + RVA_FICHIER_COMMUN))
                                         : L32(L32((DWORD)(ULONG_PTR)base + RVA_FICHIERS + slot * 4));
    if (!fichier) return FALSE;
    DWORD q = fichier + rel;
    for (int i = 0; i < 256; i++, q += 8) {
        const int k = (int)L32(q);
        if (k == -1) return FALSE;
        if (k == cle) return TRUE;
    }
    return FALSE;
}
#define PROP_ANNULABLE 0x196
#define PROP_OH        0x44
#define RVA_AVANTAGE   0xF9CC2C

#define NB_SEUILS 128
static struct { BYTE perso, slot; DWORD action; int seuil; } g_seuils[NB_SEUILS];
static int g_nbSeuils, g_prochainSeuil;
static int Seuil(const Combattant *c)
{
    for (int i = 0; i < g_nbSeuils; i++)
        if (g_seuils[i].action == c->action && g_seuils[i].perso == c->perso && g_seuils[i].slot == c->slot100) return g_seuils[i].seuil;
    const int v = Propriete(c->slot100, c->action, PROP_ANNULABLE);
    const int k = g_nbSeuils < NB_SEUILS ? g_nbSeuils++ : (g_prochainSeuil++ % NB_SEUILS);
    g_seuils[k].perso = c->perso; g_seuils[k].slot = c->slot100; g_seuils[k].action = c->action; g_seuils[k].seuil = v;
    return v;
}

static BOOL PeutAgir(const Combattant *c)
{
    if (c->etat == ETAT_NEUTRE) return TRUE;
    const int seuil = Seuil(c);
    return seuil >= 0 && c->cpt2 >= seuil;
}

static int FramesAvantAgir(const Combattant *c)
{
    if (c->etat == ETAT_NEUTRE) return 0;
    const int seuil = Seuil(c);
    return seuil >= 0 ? (seuil > c->cpt2 ? seuil - c->cpt2 : 0) : -1;
}

static int TailleBanque(int slot, int banque)
{
    const DWORD table = Section(slot, 0x21);
    if (!table) return 0;
    const DWORD debut = L32(table + banque * 4);
    DWORD fin = 0;
    for (int b = 0; b < 6; b++) { const DWORD x = L32(table + b * 4); if (x > debut && (!fin || x < fin)) fin = x; }
    const int n = fin ? (int)((fin - debut) / 0xC) : 1200;
    return n > 1200 ? 1200 : n;
}

#define NB_LISTE 64
#define NB_PERSOS_LISTES 4
typedef struct {
    int perso, slot;
    int n, delaiPH;
    DWORD actionPH, action6PH;
    int delai6PH;
    DWORD bas[6]; int nbBas, delaiBas;
    DWORD neutres[16]; int delaiNeutres[16], nbNeutres;
    BOOL condNeutres[16]; int nbSansCondition;
    char netNeutres[16][8];
    struct { DWORD action, mouvement; int delai; } c[NB_LISTE];
} ListeChoppes;
static ListeChoppes g_listes[NB_PERSOS_LISTES] = {{-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}};
static int g_prochaineListe;
static ListeChoppes *g_liste;

static void ChoppesDeCommande(int slot, int slotScript, ListeChoppes *l);
static void ListerChoppes(const Combattant *c)
{
    const int slot = c->b7e ? c->slot100 : c->slot28;
    if (c->b7e || c->etat != ETAT_NEUTRE) return;
    for (int k = 0; k < NB_PERSOS_LISTES; k++)
        if (g_listes[k].perso == c->perso && g_listes[k].slot == slot) { g_liste = &g_listes[k]; return; }
    ListeChoppes *l = &g_listes[g_prochaineListe++ % NB_PERSOS_LISTES];
    l->perso = c->perso; l->slot = slot; l->n = 0; l->delaiPH = 0; l->actionPH = l->action6PH = 0; l->delai6PH = 0; l->nbBas = 0; l->delaiBas = 0; l->nbNeutres = 0; l->nbSansCondition = 0;
    const int n = TailleBanque(slot, 1);
    BYTE r[12];
    for (int i = 0; i < n && l->n < NB_LISTE; i++) {
        const DWORD a = Action(slot, 8000 + i);
        if (!a || !Lu(a, r, 12) || r[2] != ETAT_CHOPPE) continue;
        const DWORD mvt = r[8] | r[9] << 8;
        const int delai = FrameChoppe(Script(c->slot100, mvt));
        if (delai <= 0) continue;
        l->c[l->n].action = 8000 + i; l->c[l->n].mouvement = mvt; l->c[l->n++].delai = delai;
        if (mvt == MOUVEMENT_PH && !l->delaiPH) l->delaiPH = delai;
    }
    ChoppesDeCommande(slot, c->slot100, l);
    g_liste = l;
    Journal("liste_choppes", l->n, c->perso, l->delaiPH);
}
static int DelaiPH(void) { return g_liste ? g_liste->delaiPH : 0; }
static int DelaiBas(void) { return g_liste ? g_liste->delaiBas : 0; }

static BOOL Ecart(int donnee, int mesure) { return donnee - mesure >= 2 || mesure - donnee >= 1; }
static BOOL EstChoppeBasse(DWORD action);
static void ApprendreDelai(DWORD action, int mesure)
{
    if (!g_liste || mesure <= 0) return;
    for (int i = 0; i < g_liste->n; i++)
        if (g_liste->c[i].action == action && Ecart(g_liste->c[i].delai, mesure)) {
            Journal("delai_appris", (long)action, g_liste->c[i].delai, mesure);
            g_liste->c[i].delai = mesure;
        }
    for (int i = 0; i < g_liste->nbNeutres; i++)
        if (g_liste->neutres[i] == action && Ecart(g_liste->delaiNeutres[i], mesure)) { g_liste->delaiNeutres[i] = mesure; }
    if (action == g_liste->action6PH && Ecart(g_liste->delai6PH, mesure)) { g_liste->delai6PH = mesure; }
    if (action == g_liste->actionPH && Ecart(g_liste->delaiPH, mesure)) { g_liste->delaiPH = mesure; }
    if (EstChoppeBasse(action)) {
        int m = 0;
        for (int i = 0; i < g_liste->nbNeutres; i++)
            if (EstChoppeBasse(g_liste->neutres[i]) && g_liste->delaiNeutres[i] > 0 && (!m || g_liste->delaiNeutres[i] < m))
                m = g_liste->delaiNeutres[i];
        if (m) g_liste->delaiBas = m;
        else if (Ecart(g_liste->delaiBas, mesure) && mesure < g_liste->delaiBas) g_liste->delaiBas = mesure;
    }
}
static BOOL EstChoppeBasse(DWORD action)
{
    for (int i = 0; g_liste && i < g_liste->nbBas; i++) if (g_liste->bas[i] == action) return TRUE;
    return FALSE;
}
__attribute__((unused)) static int DelaiListe(DWORD action)
{
    for (int i = 0; g_liste && i < g_liste->n; i++) if (g_liste->c[i].action == action) return g_liste->c[i].delai;
    return -1;
}

#define COND_ACCROUPI 0x1C000C
static void ChoppesDeCommande(int slot, int slotScript, ListeChoppes *l)
{
    const DWORD pages = Section(slot, 0x24), nombres = Section(slot, 0x25);
    const DWORD fichier = L32(L32((DWORD)(ULONG_PTR)base + RVA_FICHIERS + slot * 4));
    BYTE n = 0;
    if (!pages || !nombres || !fichier || !Lu(nombres, &n, 1)) return;
    const DWORD groupe0 = L32(pages);
    for (int i = 0; groupe0 && i < n; i++) {
        const DWORD fiche = fichier + groupe0 + i * 0x44;
        char cmd[8] = {0}, net[8] = {0};
        Lu(fichier + L32(fiche), cmd, sizeof cmd - 1);
        for (int a = 0, b = 0; a < 7 && cmd[a]; a++) if (cmd[a] != ' ') net[b++] = cmd[a];
        const size_t lg = strlen(net);
        const DWORD condition = L32(fiche + 0x30);
        const DWORD requis = L32(fiche + 0x0C);
        CONTROLE("commande_requis", (long)requis, (long)condition, i);
        if (requis && condition != COND_ACCROUPI) continue;
        BOOL seule = TRUE;
        for (size_t q = 0; q + 1 < lg; q++)
            if (strchr("PKFN+.", net[q])) seule = FALSE;
        const BOOL neutre = lg && net[lg - 1] == 'N' && seule;
        const int quoi = !strcmp(net, "N") ? 1 : !strcmp(net, "6N") ? 2
                       : (!strcmp(net, "2N") || !strcmp(net, "1N") || !strcmp(net, "3N")) ? 3 : neutre ? 4 : 0;
        if (!quoi) continue;
        const DWORD ids = fichier + L32(fiche + 8);
        for (int t = 0; t < 6; t++) {
            const DWORD id = L32(ids - 4 * t);
            if (id == 0xFFFF) break;
            BYTE r[12];
            const DWORD a = Action(slot, id);
            if (!a || !Lu(a, r, 12) || r[2] != ETAT_CHOPPE) continue;
            const int delai = FrameChoppe(Script(slotScript, r[8] | r[9] << 8));
            {
                int nb, vitesse, debut;
                FrameOp(Script(slotScript, r[8] | r[9] << 8), 0x21, &nb, &vitesse, &debut);
                CONTROLE("commande_choppe_script", (long)id, vitesse, nb);
#ifdef PUNISH_TEST
                if (nb >= 3) JournalScriptChoppe(Script(slotScript, r[8] | r[9] << 8), id);
#endif
            }
            if (quoi == 1 && !l->actionPH && !condition) { l->actionPH = id; if (delai > 0) l->delaiPH = delai; }
            if (quoi == 2 && !l->action6PH && !condition) { l->action6PH = id; l->delai6PH = delai; }
            const BOOL oh = ProprietePresente(slot, id, PROP_OH);
            CONTROLE("commande_choppe_oh", (long)id, delai, oh);
            if (neutre && !oh && l->nbNeutres < 16) {
                l->neutres[l->nbNeutres] = id; l->delaiNeutres[l->nbNeutres] = delai;
                memcpy(l->netNeutres[l->nbNeutres], net, sizeof net);
                l->condNeutres[l->nbNeutres++] = condition != 0;
                if (!condition) l->nbSansCondition++;
            }
            CONTROLE("commande_choppe", (long)id, delai, (long)condition);
            if (condition == COND_ACCROUPI && l->nbBas < 6) {
                l->bas[l->nbBas++] = id;
                if (delai > 0 && (!l->delaiBas || delai < l->delaiBas)) l->delaiBas = delai;
            }
        }
    }
    CONTROLE("commande_ph", (long)l->actionPH, l->delaiPH, 0);
    CONTROLE("commande_6ph", (long)l->action6PH, l->delai6PH, 0);
    CONTROLE("commande_bas", l->nbBas, l->delaiBas, l->nbBas ? (long)l->bas[0] : 0);
    CONTROLE("commande_neutres", l->nbNeutres, l->nbSansCondition, 0);
}

#define INCONNU (-999)
#define DECALAGE_CONTACT 2
#define INITIATIVE (-99)
#define ATTENTE_INITIATIVE 60
#define RVA_CONTACT 0xFCEEE0
static int DeltaGarde(int j, int *demarrage)
{
    const BYTE *ctx = base + RVA_CONTACT + j * PAS_ATTAQUE;
    const int slotAtt = ctx[0x15], index = ctx[6];
    WORD actionAtt; memcpy(&actionAtt, ctx, 2);
    const DWORD sec2 = Section(slotAtt, 2), sec20 = Section(slotAtt, 0x14);
    signed char delta;
    *demarrage = -1;
    if (sec20) {
        const int i20 = actionAtt < 8000 ? actionAtt : actionAtt < 16000 ? actionAtt - 0x1DEF : -1;
        if (i20 >= 0) *demarrage = (WORD)L16(sec20 + i20 * 0x10);
    }
    if (!sec2 || !Lu(sec2 + index * 0x74 + 0x59, &delta, 1)) return 0x7FFF;
    return delta;
}

enum { GRIS, VERT, ORANGE, ROUGE, BLEU, DORE };
static const COLORREF COULEURS[] = {RGB(205, 205, 205), RGB(40, 170, 70), RGB(230, 140, 20), RGB(205, 40, 40), RGB(60, 140, 235),
                                    RGB(235, 190, 50)};
typedef struct { int couleur; WCHAR ligne1[64], ligne2[128]; DWORD t; } Verdict;
static Verdict g_verdict;
static volatile LONG g_versionVerdict;
static CRITICAL_SECTION g_csVerdict;
#define DUREE_AFFICHAGE 1100
#define ENTREE_MS 70

static int g_mode = 1;
static void Afficher(int couleur, const WCHAR *l1, const WCHAR *l2)
{
    EnterCriticalSection(&g_csVerdict);
    g_verdict.couleur = couleur;
    lstrcpynW(g_verdict.ligne1, l1, 64);
    lstrcpynW(g_verdict.ligne2, l2, 128);
    g_verdict.t = GetTickCount() | 1;
    LeaveCriticalSection(&g_csVerdict);
    InterlockedIncrement(&g_versionVerdict);
}

#define NB_COUPS 64
static struct { DWORD coup; BYTE perso; int dis; } g_coups[NB_COUPS];
static int g_nbCoups, g_prochainCoup;
static void RetenirDesavantage(BYTE perso, DWORD coup, int dis)
{
    for (int i = 0; i < g_nbCoups; i++) if (g_coups[i].coup == coup && g_coups[i].perso == perso) { g_coups[i].dis = dis; return; }
    const int k = g_nbCoups < NB_COUPS ? g_nbCoups++ : (g_prochainCoup++ % NB_COUPS);
    g_coups[k].coup = coup; g_coups[k].perso = perso; g_coups[k].dis = dis;
}
static int DesavantageConnu(BYTE perso, DWORD coup)
{
    for (int i = 0; i < g_nbCoups; i++) if (g_coups[i].coup == coup && g_coups[i].perso == perso) return g_coups[i].dis;
    return -1;
}

#define RVA_PORTEE 0xF99664
#define NB_PORTEES 32
static struct { DWORD action; BYTE perso; float portee; } g_portees[NB_PORTEES];
static int g_nbPortees;
static float Portee(BYTE perso, DWORD action)
{
    for (int i = 0; i < g_nbPortees; i++) if (g_portees[i].perso == perso && g_portees[i].action == action) return g_portees[i].portee;
    return 0;
}
static void RetenirPortee(BYTE perso, DWORD action, float portee)
{
    if (portee <= 0) return;
    for (int i = 0; i < g_nbPortees; i++)
        if (g_portees[i].perso == perso && g_portees[i].action == action) { if (portee > g_portees[i].portee) g_portees[i].portee = portee; return; }
    if (g_nbPortees < NB_PORTEES) { g_portees[g_nbPortees].perso = perso; g_portees[g_nbPortees].action = action; g_portees[g_nbPortees++].portee = portee; }
}

static BOOL Debout(int x);
static BOOL DummyAccroupi(void);
static float g_distCandidat;

#define NB_EXCLUES 64
static struct { BYTE dummy; DWORD coup, choppe; } g_exclues[NB_EXCLUES];
static int g_nbExclues;
static BYTE g_exclDummy; static DWORD g_exclCoup;
static BOOL Exclue(DWORD choppe)
{
    for (int i = 0; i < g_nbExclues; i++)
        if (g_exclues[i].dummy == g_exclDummy && g_exclues[i].coup == g_exclCoup && g_exclues[i].choppe == choppe) return TRUE;
    return FALSE;
}
static void Exclure(BYTE dummy, DWORD coup, DWORD choppe)
{
    g_exclDummy = dummy; g_exclCoup = coup;
    if (Exclue(choppe) || g_nbExclues >= NB_EXCLUES) return;
    g_exclues[g_nbExclues].dummy = dummy; g_exclues[g_nbExclues].coup = coup; g_exclues[g_nbExclues++].choppe = choppe;
}
static BOOL Candidate(DWORD action, int delai, int dis)
{
    if (delai <= 0 || delai > dis) return FALSE;
    if (Exclue(action)) return FALSE;
    if (g_liste && g_distCandidat > 0) {
        const float r = Portee((BYTE)g_liste->perso, action);
        if (r > 0 && g_distCandidat > r) return FALSE;
    }
    if (g_liste && action == g_liste->actionPH) return FALSE;
    if (DummyAccroupi()) return EstChoppeBasse(action);
    return EstChoppeBasse(action) != Debout(delai);
}

static int MeilleureChoppe(int dis, int *indice)
{
    int m = 0;
    *indice = -1;
    for (int i = 0; g_liste && i < g_liste->nbNeutres; i++) {
        if (g_liste->condNeutres[i] && g_liste->nbSansCondition && !EstChoppeBasse(g_liste->neutres[i]))
            continue;
        if (Candidate(g_liste->neutres[i], g_liste->delaiNeutres[i], dis) && g_liste->delaiNeutres[i] > m) {
            m = g_liste->delaiNeutres[i]; *indice = i;
        }
    }
    return m;
}
static int DelaiMeilleure(int dis) { int i; return MeilleureChoppe(dis, &i); }

enum { P_RIEN, P_GARDE, P_APRES, P_CHOPPE, P_PRISE, P_FRAPPE };
typedef struct {
    int phase;
    DWORD coup;
    BYTE persoDummy, persoJoueur;
    short santeJoueur;
    long iF0, iD, iDepart, iDummyLibre;
    int disPredit;
    BOOL connu;
    int cptContact;
    BOOL gardeOrdinaire;
    BOOL accroupi;
    long iDebout;
    BOOL dos;
    BOOL gardeCassee;
    DWORD coupContact;
    int dis, retard, delai;
    DWORD choppe, mvtChoppe, actionPrise;
    float porteeDepart;
    float distFin;
    DWORD frappe, reactionVue;
    long iContact;
    long iActif;
    short santeDummy;
    int stun;
    float porteeFrappe;
    BOOL ph;
    int couleur;
} Sequence;
static Sequence s;
static LONG g_nbGardes; static DWORD g_derniereGarde;
static LONG g_toucheGarde;
static BOOL g_defiRattache;
static BOOL DummyAccroupi(void) { return s.accroupi; }
static long g_image;
static int g_joueur;
static int g_cptDebut[2];

static int Predire(const Combattant *d)
{
    const int seuil = FramesAvantAgir(d);
    int demarrage, stat = INCONNU;
    const int delta = DeltaGarde(g_joueur, &demarrage);
    if (s.gardeOrdinaire && delta != 0x7FFF) {

        const int retard = demarrage > 0 && s.cptContact > demarrage + DECALAGE_CONTACT ? s.cptContact - demarrage - DECALAGE_CONTACT : 0;
        stat = delta - retard;
    }
    CONTROLE("prediction", seuil, stat, (long)d->action);
    CONTROLE("prediction_detail", delta, demarrage, s.cptContact);
    if (seuil >= 0) return seuil;
    return stat;
}

static BOOL Debout(int x) { return s.iDebout && s.iDebout - s.iF0 <= x; }
static int Delai6PH(void) { return g_liste && g_liste->delai6PH > 0 ? g_liste->delai6PH : DelaiPH() ? DelaiPH() + ECART_6PH : 0; }
#define RVA_DISTANCE 0xFB8E20
static float Distance(void) { float f; memcpy(&f, base + RVA_DISTANCE, 4); return f; }
static float PorteeJeu(void) { float f; memcpy(&f, base + RVA_PORTEE + g_joueur * 0x144, 4); return f; }

static void ApprendrePortee(void)
{
    const float p = PorteeJeu();
    if (p != s.porteeDepart) RetenirPortee(s.persoJoueur, s.choppe, p);
    CONTROLE("portee", (long)(p * 100), (long)(s.porteeDepart * 100), (long)(Distance() * 100));
}

static void NoterDefi(BOOL connu, int couleur, BOOL retenu);
static void Juger(void)
{
    WCHAR l1[64], l2[128];
    const int total = s.retard + s.delai;
    if (!s.connu) {
        s.couleur = GRIS;
        swprintf(l1, 64, L"THROW i%d", s.delai - 1);
        lstrcpynW(l2, L"?   disadvantage not known yet for this move", 128);
    } else if (s.dis <= 0) {
        s.couleur = ROUGE;
        lstrcpynW(l1, L"NOT PUNISHABLE", 64);
        swprintf(l2, 128, L"%d   you were at %d on block", s.dis, s.dis);
    } else if (total <= s.dis) {
        s.couleur = s.ph ? ORANGE : VERT;
        if (s.couleur == ORANGE) {
            lstrcpynW(l1, L"BREAKABLE", 64);
            swprintf(l2, 128, L"-%d   P+H in time, but it can be broken", s.dis);
        } else {
            const int m = DelaiMeilleure(s.dis);
            const int prevu = DelaiListe(s.choppe) > 0 ? DelaiListe(s.choppe) : s.delai;
            if (m && prevu >= m) {
                s.couleur = DORE;
                lstrcpynW(l1, L"BEST PUNISH", 64);
                swprintf(l2, 128, L"-%d   throw i%d: the slowest (strongest) that fits", s.dis, s.delai - 1);
            } else {
                lstrcpynW(l1, L"GUARANTEED PUNISH", 64);
                if (m) swprintf(l2, 128, L"-%d   best: i%d", s.dis, m - 1);
                else swprintf(l2, 128, L"-%d   throw i%d   %d F to spare", s.dis, s.delai - 1, s.dis - total);
            }
        }
    } else {
        s.couleur = ROUGE;
        if (s.delai > s.dis) lstrcpynW(l1, L"THROW TOO SLOW", 64);
        else swprintf(l1, 64, L"TOO LATE BY %d F", total - s.dis);
        swprintf(l2, 128, L"-%d   throw i%d   started %d F late", s.dis, s.delai - 1, s.retard);
    }
    Afficher(s.couleur, l1, l2);
    Journal("verdict", s.couleur, s.dis, total);
    NoterDefi(s.connu, s.couleur, FALSE);
    CONTROLE("verdict_detail", s.retard, s.delai, (long)s.choppe);
    CONTROLE("delai_predit", DelaiListe(s.choppe), s.delai, 0);
}

static void Finir(int couleur, const WCHAR *l1, const WCHAR *l2, const char *libelle, BOOL retenu)
{
    if (l1) { Afficher(couleur, l1, l2); Journal(libelle, s.dis, 0, 0); NoterDefi(s.connu || s.iDummyLibre != 0, couleur, retenu); }
    s.phase = P_RIEN;
}

static BOOL EtatGarde(BYTE e)
{
    switch (e) {
    case ETAT_NEUTRE: case 2: case ETAT_ATTAQUE: case ETAT_PRISE: case 5:
    case 10: case 11: case 12: case 13: case ETAT_CHOPPE:
        return FALSE;
    default:
        return TRUE;
    }
}
#define DUREE_CHUTE 40

static void JugerFrappe(const Combattant *j, const Combattant *d, long i)
{
    WCHAR l1[64], l2[128];
    const float r = PorteeJeu();
    if (r > s.porteeFrappe) s.porteeFrappe = r;
    if (!s.iActif && base[RVA_JOUEURS + g_joueur * PAS_JOUEUR + OFS_PHASE] == PHASE_ACTIVE) s.iActif = i;
    if (!s.iContact) {
        if (d->sante < s.santeDummy) {
            s.iContact = i;
            CONTROLE("frappe_contact", (long)(i - s.iDepart), s.iActif ? (long)(s.iActif - s.iDepart) : -1, s.iD ? (long)(s.iD - s.iF0) : -1);
            { int dem; memcpy(&dem, base + RVA_DEBUT_ACTIF + g_joueur * 0x144, 4); CONTROLE("frappe_demarrage", dem, (long)s.frappe, s.retard); }
        } else if (j->sante < s.santeJoueur || d->etat == 7 || (j->etat != ETAT_ATTAQUE && j->etat != 2) || i - s.iDepart > 90) {
            const float dist = Distance();
            CONTROLE("frappe_ratee", (long)(dist * 100), (long)(s.porteeFrappe * 100), d->etat);
            if (d->etat == 7) lstrcpynW(l2, L"-   blocked: too late", 128);
            else if (s.porteeFrappe > 0 && dist > s.porteeFrappe)
                swprintf(l2, 128, L"%d.%02dm   out of range (reach %d.%02dm): pushback", (int)dist, (int)(dist * 100) % 100,
                         (int)s.porteeFrappe, (int)(s.porteeFrappe * 100) % 100);
            else lstrcpynW(l2, L"-   the strike did not connect", 128);
            Finir(ROUGE, L"STRIKE MISSED", l2, "frappe_manquee", FALSE);
        }
        return;
    }

    if (d->etat == ETAT_TOUCHE) s.stun |= 1;
    if (!s.stun && i - s.iContact >= DUREE_CHUTE) s.stun |= 2;
    if (d->action != s.reactionVue) {
        s.reactionVue = d->action;
        CONTROLE("frappe_reaction", d->etat | (long)(i - s.iContact) << 8, (long)d->action, s.stun);
    }
    if (!s.stun && d->etat != ETAT_NEUTRE && !PeutAgir(d)) return;
    CONTROLE("frappe_fin", d->etat | (long)(i - s.iContact) << 8, (long)d->action, PeutAgir(d));

    const long iFrappe = s.iActif && s.iActif <= s.iContact ? s.iActif : s.iContact;
    const BOOL tard = s.iD != 0 && s.iD < iFrappe;
    const int dis = s.iD ? (int)(s.iD - s.iF0) : s.disPredit != INCONNU ? s.disPredit : -1;
    WCHAR d1[16];
    if (dis > 0) swprintf(d1, 16, L"-%d", dis); else lstrcpynW(d1, L"-", 16);
    if (tard) {
        lstrcpynW(l1, L"STRIKE TOO LATE", 64);
        swprintf(l2, 128, L"%ls   %d F too late", d1, (int)(iFrappe - s.iD));
    } else if (s.stun) {
        lstrcpynW(l1, L"STRIKE PUNISH", 64);
        swprintf(l2, 128, L"%ls   %ls", d1, s.stun & 1 ? L"critical stun" : L"launch / knockdown");
    } else {
        lstrcpynW(l1, L"NO STUN", 64);
        swprintf(l2, 128, L"%ls   hit, but no critical stun: not a punish", d1);
    }
    CONTROLE("frappe_verdict", tard, s.stun, (long)s.frappe);
    Finir(tard ? ROUGE : s.stun ? VERT : GRIS, l1, l2, "frappe_jugee", FALSE);
}

static BOOL GardeCassee(const Combattant *j)
{
    return ProprietePresente(j->slot100, j->action, 125) && ProprietePresente(j->slot100, j->action, 336);
}

static long g_dernierH = -1000;
static BOOL GardeTenue(long i) { return i - g_dernierH <= 3; }
#ifdef PUNISH_TEST

static void JournalProprietes(int slot, DWORD id)
{
    const DWORD a = Action(slot, id);
    if (!a) return;
    const DWORD rel = L32(a + 4);
    const DWORD fichier = id / 8000 == 3 ? L32(L32((DWORD)(ULONG_PTR)base + RVA_FICHIER_COMMUN))
                                         : L32(L32((DWORD)(ULONG_PTR)base + RVA_FICHIERS + slot * 4));
    if (!rel || !fichier) return;
    DWORD q = fichier + rel;
    for (int i = 0; i < 24; i++, q += 8) {
        const int k = (int)L32(q);
        if (k == -1) return;
        Journal("reaction_prop", (long)id, k, (long)L32(q + 4));
    }
}
#endif

static void Observer(const Combattant *j, const Combattant *pj, const Combattant *d, const Combattant *pd, long i)
{
    WCHAR l2[128];
    { WORD t; memcpy(&t, base + RVA_JOUEURS + g_joueur * PAS_JOUEUR + OFS_MAINTENU, 2); if (t & TOUCHE_H) g_dernierH = i; }

    const BOOL garde = EtatGarde(j->etat) && !PeutAgir(j) && (d->etat == ETAT_ATTAQUE || d->etat == 2 || d->etat == 5)
                       && j->sante >= pj->sante && (j->etat == 7 || GardeTenue(i));

    if (garde && !EtatGarde(pj->etat) && PeutAgir(pj)) {
        memset(&s, 0, sizeof s);
        s.phase = P_GARDE; s.dis = -1; s.disPredit = INCONNU; s.santeJoueur = pj->sante;
    }
    switch (s.phase) {
    case P_GARDE: {
        if (EtatGarde(j->etat) && !PeutAgir(j) &&
            (!EtatGarde(pj->etat) || j->cpt < pj->cpt || j->action != pj->action)) {
            s.cptContact = d->cpt - g_cptDebut[1 - g_joueur];
            s.gardeOrdinaire = j->etat == 7;
            s.coupContact = d->action;
#ifdef PUNISH_TEST
            if (!s.gardeOrdinaire) JournalProprietes(j->slot100, j->action);
#endif
            if (!s.gardeOrdinaire && (!GardeTenue(i) || !GardeCassee(j))) {
                CONTROLE("touche_sans_garde", j->etat, (long)j->action, GardeTenue(i));
                if (GardeTenue(i)) g_toucheGarde++;
                s.phase = P_RIEN;
                break;
            }
            if (!s.gardeOrdinaire) s.gardeCassee = TRUE;
            CONTROLE("contact", j->etat, (long)j->action, j->sante - s.santeJoueur);
        }
        const BOOL libre = PeutAgir(d);
        if (!libre) { s.coup = d->action; s.persoDummy = d->perso; s.iDummyLibre = 0; }
        else if (!s.iDummyLibre) s.iDummyLibre = i;
        s.persoJoueur = j->perso;
        if (j->sante < s.santeJoueur) { s.phase = P_RIEN; break; }
        if (!EtatGarde(j->etat) || PeutAgir(j)) {
            s.iF0 = i; s.phase = P_APRES;
            g_derniereGarde = s.coupContact ? s.coupContact : s.coup; g_nbGardes++;
            s.accroupi = d->accroupi != 0;
            s.iDebout = s.accroupi ? 0 : i;
            CONTROLE("distance_fin_garde", (long)(Distance() * 100), s.accroupi, s.dos);
            s.distFin = Distance(); g_distCandidat = s.distFin;
            g_exclDummy = s.persoDummy; g_exclCoup = s.coup;
            s.dos = DeDos(1 - g_joueur, d);
            s.disPredit = Predire(d);
            int avJeu; memcpy(&avJeu, base + RVA_AVANTAGE + g_joueur * 4, 4);
            CONTROLE("avantage_jeu", avJeu, s.iDummyLibre ? -(int)(i - s.iDummyLibre) : 999, (long)s.coup);
            if (s.iDummyLibre == 0 && avJeu < 0 && avJeu > -60) s.iDummyLibre = i + avJeu;
            if (s.gardeCassee) {
                s.dis = s.iDummyLibre ? -(int)(i - s.iDummyLibre) : INITIATIVE;
                s.connu = TRUE;
                if (s.dis != INITIATIVE && s.dis < 0) swprintf(l2, 128, L"+%d   opponent +%d: do not press", -s.dis, -s.dis);
                else lstrcpynW(l2, L"+   opponent keeps the initiative: do not press", 128);
                Finir(BLEU, L"GUARD BREAK", l2, "garde_cassee", TRUE);
            } else if (s.iDummyLibre != 0) {
                s.dis = -(int)(i - s.iDummyLibre);
                if (s.dis == 0) Finir(GRIS, L"EVEN ON BLOCK", L"0   not punishable", "egal", TRUE);
                else {
                    swprintf(l2, 128, L"+%d   opponent +%d on block: do not press", -s.dis, -s.dis);
                    Finir(BLEU, L"FRAME TRAP", l2, "non_punissable", TRUE);
                }
            }
        }
        break;
    }
    case P_APRES:
    case P_CHOPPE:
    case P_FRAPPE:
        if (s.iD == 0 && !s.iDebout && !d->accroupi) s.iDebout = i;
        if (s.iD == 0 && DeDos(1 - g_joueur, d)) s.dos = TRUE;
        if (s.iD == 0 && !(s.phase == P_FRAPPE && s.iContact) && PeutAgir(d)) {
            s.iD = i; s.dis = (int)(i - s.iF0); s.connu = TRUE;
            if (s.gardeOrdinaire) RetenirDesavantage(s.persoDummy, s.coup, s.dis);
            Journal("desavantage", s.dis, s.disPredit, s.accroupi | s.dos << 1 | s.gardeOrdinaire << 2);
            { int avJeu; memcpy(&avJeu, base + RVA_AVANTAGE + g_joueur * 4, 4); CONTROLE("avantage_final", avJeu, s.dis, (long)s.coup); }
        }
        if (s.phase == P_APRES) {
            if (j->etat == ETAT_CHOPPE && pj->etat != ETAT_CHOPPE && ProprietePresente(j->slot100, j->action, PROP_OH)) {
                CONTROLE("oh", (long)j->action, s.dis, 0);
                if (s.iD) swprintf(l2, 128, L"-%d   not a throw: punish with a throw", s.dis);
                else lstrcpynW(l2, L"-   not a throw: punish with a throw", 128);
                Finir(GRIS, L"OFFENSIVE HOLD", l2, "oh", FALSE);
            } else if (j->etat == ETAT_CHOPPE && pj->etat != ETAT_CHOPPE) {
                s.iDepart = i; s.choppe = j->action; s.mvtChoppe = j->mouvement; s.phase = P_CHOPPE;
                s.porteeDepart = PorteeJeu();
                CONTROLE("choppe_distance", (long)(Distance() * 100), (long)s.choppe, (long)(Portee(j->perso, j->action) * 100));
                s.retard = (int)(i - s.iF0) - 1;
                if (s.retard < 0) s.retard = 0;
            } else if ((j->etat == ETAT_ATTAQUE || j->etat == 2) && pj->etat != ETAT_ATTAQUE && pj->etat != 2) {
                s.phase = P_FRAPPE; s.frappe = j->action; s.iDepart = i; s.iContact = 0; s.santeDummy = pd->sante;
                s.stun = 0; s.reactionVue = 0; s.porteeFrappe = 0; s.iActif = 0;
                s.retard = (int)(i - s.iF0) - 1;
                if (s.retard < 0) s.retard = 0;
                CONTROLE("frappe_depart", (long)s.frappe, s.retard, (long)(Distance() * 100));
            } else if (j->sante < pj->sante) {
                Finir(ROUGE, L"GOT HIT", L"", "touche", FALSE);
            } else if (s.iD != 0 && i - s.iD > 30) {

                const int ph = DelaiPH(), d6 = Delai6PH(), bas = DelaiBas();
                const float dist = Distance();
                const float r6 = g_liste ? Portee(s.persoJoueur, g_liste->action6PH) : 0, rph = g_liste ? Portee(s.persoJoueur, g_liste->actionPH) : 0;
                const BOOL loin6 = r6 > 0 && dist > r6, loinph = rph > 0 && dist > rph;
                CONTROLE("distance_attente", (long)(dist * 100), (long)(r6 * 100), (long)(rph * 100));
                const BOOL h6 = d6 && d6 <= s.dis && Debout(d6) && !loin6, hph = ph && ph <= s.dis && Debout(ph) && !loinph;
                const BOOL hbas = bas && bas <= s.dis && !Debout(bas);
                if (h6 || hbas || (s.dos && hph)) {
                    if (hbas && !h6) swprintf(l2, 128, L"-%d   low throw (i%d) was possible", s.dis, bas - 1);
                    else if (s.dos && !h6) swprintf(l2, 128, L"-%d   back turned: P+H (i%d) was unbreakable", s.dis, ph - 1);
                    else swprintf(l2, 128, L"-%d   6P+H (i%d) was possible", s.dis, d6 - 1);
                    Finir(ROUGE, L"NO PUNISH", l2, "aucune", FALSE);
                } else if (hph) {
                    swprintf(l2, 128, L"-%d   only P+H fits, and it can be broken", s.dis);
                    Finir(GRIS, L"ONLY A BREAKABLE PUNISH", l2, "seulement_ph", TRUE);
                } else {
                    if (!Debout(s.dis)) swprintf(l2, 128, L"-%d   crouching: low throw needs %d F", s.dis, bas);
                    else if ((loin6 || loinph) && ph <= s.dis)
                        swprintf(l2, 128, L"-%d   pushback: out of throw range (%d.%02dm)", s.dis, (int)dist, (int)(dist * 100) % 100);
                    else swprintf(l2, 128, L"-%d   P+H needs %d F", s.dis, ph);
                    Finir(GRIS, L"SAFE AGAINST THROWS", l2, "sauf", TRUE);
                }
            } else if (s.iD == 0 && i - s.iF0 > ATTENTE_INITIATIVE && d->etat != ETAT_NEUTRE) {

                int avJeu; memcpy(&avJeu, base + RVA_AVANTAGE + g_joueur * 4, 4);
                CONTROLE("initiative", avJeu, d->etat, (long)d->action);
                const BOOL chiffre = avJeu < 0 && avJeu > -60;
                s.dis = chiffre ? avJeu : INITIATIVE; s.connu = TRUE;
                if (chiffre) swprintf(l2, 128, L"+%d   opponent +%d on block: do not press", -avJeu, -avJeu);
                else lstrcpynW(l2, L"+   opponent keeps the initiative: block", 128);
                Finir(BLEU, L"FRAME TRAP", l2, "initiative", TRUE);
            } else if (s.iD == 0 && i - s.iF0 > 180) {
                Finir(GRIS, NULL, NULL, NULL, FALSE);
            }
        } else if (s.phase == P_FRAPPE) {
            JugerFrappe(j, d, i);
        } else {
            if (j->etat == ETAT_PRISE) {

                s.delai = pj->cpt > 0 ? pj->cpt + (j->cpt > 1 ? j->cpt - 1 : 0) : (int)(i - s.iDepart);
                if (!s.connu && s.disPredit != INCONNU) { s.dis = s.disPredit; s.connu = TRUE; }
                if (!s.connu && (s.dis = DesavantageConnu(s.persoDummy, s.coup)) >= 0) s.connu = TRUE;
                s.ph = !s.dos && ((g_liste && g_liste->actionPH && s.choppe == g_liste->actionPH) ||
                       j->action == ACTION_PRISE_PH || s.mvtChoppe == MOUVEMENT_PH);
                if (s.ph && g_liste && g_liste->perso == j->perso && (!g_liste->delaiPH || s.delai < g_liste->delaiPH))
                    g_liste->delaiPH = s.delai;
                CONTROLE("prise", (long)j->action, (long)s.choppe, s.ph);
                ApprendreDelai(s.choppe, s.delai);
                s.actionPrise = j->action;
                ApprendrePortee();
                s.phase = P_PRISE;
                Juger();
            } else if (j->etat != ETAT_CHOPPE) {
                const int prevu = DelaiListe(s.choppe), contact = s.retard + (prevu > 0 ? prevu : 0);
                const BOOL basse = EstChoppeBasse(s.choppe);
                CONTROLE("ratee_distance", (long)(Distance() * 100), contact, basse);
                ApprendrePortee();
                const float portee = Portee(j->perso, s.choppe), dist = Distance();
                if (!basse && !Debout(contact))
                    swprintf(l2, 128, L"crouching   opponent still crouching: use a low throw (i%d)", DelaiBas() - 1);
                else if (basse && Debout(contact))
                    lstrcpynW(l2, L"standing   opponent already standing: low throws whiff", 128);
                else if (portee > 0 && dist > portee)
                    swprintf(l2, 128, L"%d.%02dm   out of range (reach %d.%02dm): pushback", (int)dist, (int)(dist * 100) % 100,
                             (int)portee, (int)(portee * 100) % 100);
                else if (s.connu) swprintf(l2, 128, L"-%d   the throw did not connect (range?)", s.dis);
                else lstrcpynW(l2, L"-   the throw did not connect", 128);

                const BOOL horsPortee = !basse && portee > 0 && dist > portee;
                if (!s.connu && s.disPredit != INCONNU) { s.dis = s.disPredit; s.connu = TRUE; }
                if (!s.connu && (s.dis = DesavantageConnu(s.persoDummy, s.coup)) >= 0) s.connu = TRUE;

                const BOOL inexpliquee = prevu > 0 && s.connu && s.dis > 0 && contact <= s.dis && (basse ? !Debout(contact) : Debout(contact));
                if (horsPortee || inexpliquee) {
                    Exclure(s.persoDummy, s.coup, s.choppe);
                    swprintf(l2, 128, L"-%d   this throw can't connect after this move (pushback)", s.dis > 0 ? s.dis : 0);
                }
                CONTROLE("ratee_portee", horsPortee | inexpliquee << 1, (long)(portee * 100), (long)(s.distFin * 100));
                Finir(ROUGE, L"THROW MISSED", l2, "ratee", horsPortee || inexpliquee);
            }
        }
        break;
    case P_PRISE:
        if (j->etat == ETAT_NEUTRE && pj->etat != ETAT_NEUTRE) s.phase = P_RIEN;
        break;
    default:
        break;
    }
}

#define RVA_APPEL_IA_DUMMY 0x29422E
#define RVA_IA_DUMMY       0x2A9DA0
#define DEMANDE_ACTION     0x74
#define NB_FICHES          220
#define NB_COUPS_LISTE     120
#define NB_CHAINE          6
#define PUNISSABLE_MIN     5
#define ATTENTE_EXO        120
#define DUREE_MAX_EXO      150
#define NB_TRACES_EXO      3

typedef struct { char net[24], txt[24]; DWORD id, adr; BYTE page, index, cat; } Fiche;
static Fiche g_fiches[NB_FICHES];
static int g_nbFiches;

typedef struct {
    int fiche, n, delta; DWORD ids[NB_CHAINE];
    BOOL fait;
    BYTE injouable;
    int dis;
    int meilleure;
    char netMeilleure[8];
    float distance;
} CoupListe;
static CoupListe g_coupsListe[NB_COUPS_LISTE];
static int g_nbCoupsListe, g_persoListe = -1, g_choix;

#define NB_PROG 48
typedef struct { WORD bits; BYTE frames, attendre; } Entree;
static struct { BOOL actif; int n, version, nProg, perso; DWORD requis, demo, ids[NB_CHAINE]; int fin[NB_CHAINE]; Entree prog[NB_PROG]; } g_exo;
static volatile LONG g_exoJoue;
static volatile LONG g_essais;
static volatile LONG g_exoRate;
static volatile LONG g_exoForcage;
static volatile LONG g_exoForceEchec;
static volatile LONG g_exoAtteint;
static volatile LONG g_exoPosture;
static volatile LONG g_exoPause;
static CRITICAL_SECTION g_csExo;
static BOOL EstFrappe(BYTE cat) { return cat == ETAT_ATTAQUE || cat == 2; }

static BOOL Debut(const char *a, const char *b)
{
    const size_t la = strlen(a);
    return la && la < strlen(b) && !strncmp(a, b, la) && b[la] != '+';
}

static int Precedente(int f)
{
    int m = -1;
    for (int i = 0; i < g_nbFiches; i++)
        if (i != f && EstFrappe(g_fiches[i].cat) && Debut(g_fiches[i].net, g_fiches[f].net) &&
            (m < 0 || strlen(g_fiches[i].net) > strlen(g_fiches[m].net))) m = i;
    return m;
}

static int DeltaAction(int slot, DWORD id)
{
    BYTE r[12]; signed char delta;
    const DWORD a = Action(slot, id), sec2 = Section(slot, 2);
    if (!a || !sec2 || !Lu(a, r, 12) || !Lu(sec2 + r[3] * 0x74 + 0x59, &delta, 1)) return 0x7FFF;
    return delta;
}

static int FinActives(int slot, DWORD id)
{
    const DWORD sec = Section(slot, 0x14);
    const long i = id < 8000 ? (long)id : id < 16000 ? (long)id - 0x1DEF : -1;
    if (!sec || i < 0) return 0;
    const int demarrage = (WORD)L16(sec + i * 0x10), actives = (WORD)L16(sec + i * 0x10 + 2);
    return demarrage + actives;
}

static void ChercherBranche(int slot, DWORD x, DWORD y)
{
    BYTE rx[12], ry[12];
    const DWORD ax = Action(slot, x), ay = Action(slot, y);
    if (!ax || !ay || !Lu(ax, rx, 12) || !Lu(ay, ry, 12)) return;
    const WORD my = ry[8] | ry[9] << 8;
    DWORD sc = Script(slot, rx[8] | rx[9] << 8);
    int frame = 0, trouve = 0;
    for (int i = 0; sc && i < 2000 && trouve < 6; i++, sc = Suivante(sc)) {
        const short op = L16(sc);
        if (op == -1) break;
        if (op == 1) frame = L16(sc + 2);
        const int na = op == 0xA0 || op == 0xA1 || op == 0xAA || op == 0xAF ? 0 : NbArguments(op);
        for (int a = 0; a < na && a < 8; a++) {
            const WORD v = (WORD)L16(sc + 2 + 2 * a);
            if (v == (WORD)y || v == (WORD)(y - 8000) || v == my) { Journal("exo_branche", op, frame, a | (v == my) << 8); trouve++; }
        }
    }
    Journal("exo_branche_fin", (long)x, (long)y, FinActives(slot, x));
}

static BOOL RequisDos(DWORD t) { return t == 1 || t == 0x27 || t == 0x2B || t == 0x45 || (t >= 0x56 && t <= 0x59); }

static BOOL RequisAdversaire(DWORD t)
{
    static const BYTE l[] = {4, 0x28, 0x29, 0x2B, 0x2D, 0x54, 0x61, 99, 10, 0x49, 0x4A, 0x4B, 0x15, 0x17, 0x27, 0x66, 0x67,
                             0x26, 0x68, 0x4C, 0x4D, 0x4E};
    for (size_t i = 0; i < sizeof l; i++) if (t == l[i]) return TRUE;
    return FALSE;
}
static BOOL RequisGere(DWORD t, int perso)
{
    if (RequisAdversaire(t)) return FALSE;
    if (t == 0 || RequisDos(t)) return TRUE;
    if (t == 9) return perso == 7 || perso == 45;
    if ((t >= 0x41 && t <= 0x43) || (t >= 0x66 && t <= 0x68)) return perso == 0x13;
    if (t == 0x5E) return perso == 0x14;
    return TRUE;
}

static void ListerCoups(const Combattant *d)
{
    const int slot = d->b7e ? d->slot100 : d->slot28;
    const DWORD pages = Section(slot, 0x24), nombres = Section(slot, 0x25);
    const DWORD fichier = L32(L32((DWORD)(ULONG_PTR)base + RVA_FICHIERS + slot * 4));
    g_nbFiches = 0; g_nbCoupsListe = 0; g_persoListe = d->perso; g_choix = 0;
    if (!pages || !nombres || !fichier) return;
    for (int p = 0; p < 15; p++) {
        BYTE n = 0;
        const DWORD groupe = L32(pages + 4 * p);
        if (!groupe || !Lu(nombres + p, &n, 1)) continue;
        for (int i = 0; i < n && g_nbFiches < NB_FICHES; i++) {
            const DWORD fiche = fichier + groupe + i * 0x44;
            const DWORD id = L32(fichier + L32(fiche + 8));
            BYTE r[12];
            const DWORD a = Action(slot, id);
            if (id == 0xFFFF || !id || !a || !Lu(a, r, 12)) continue;
            Fiche *f = &g_fiches[g_nbFiches];
            char cmd[24] = {0};
            Lu(fichier + L32(fiche), cmd, sizeof cmd - 1);
            int b = 0;
            for (int k = 0; cmd[k] && b < 23; k++) if (cmd[k] != ' ') f->net[b++] = cmd[k];
            f->net[b] = 0;
            memcpy(f->txt, cmd, sizeof f->txt);
            f->id = id; f->adr = fiche; f->page = (BYTE)p; f->index = (BYTE)i; f->cat = r[2];
            if (b) g_nbFiches++;
        }
    }
    for (int f = 0; f < g_nbFiches && g_nbCoupsListe < NB_COUPS_LISTE; f++) {
        if (!EstFrappe(g_fiches[f].cat)) continue;
        if (L32(g_fiches[f].adr + 0x28) == 0xFFFFFFFF) {
            CONTROLE("exo_sous_entree", (long)g_fiches[f].id, g_fiches[f].page, (long)(WORD)L16(g_fiches[f].adr + 0x24));
            continue;
        }
        if (!RequisGere(L32(g_fiches[f].adr + 0x0C), d->perso)) { CONTROLE("exo_requis_ignore", (long)L32(g_fiches[f].adr + 0x0C), g_fiches[f].page, 0); continue; }
        BOOL suite = FALSE;
        for (int i = 0; i < g_nbFiches && !suite; i++) suite = EstFrappe(g_fiches[i].cat) && Debut(g_fiches[f].net, g_fiches[i].net);
        if (suite) continue;
        const int delta = DeltaAction(slot, g_fiches[f].id);
        if (delta == 0x7FFF || delta < PUNISSABLE_MIN) continue;
        int chaine[NB_CHAINE], n = 0;
        for (int c = f; c >= 0 && n < NB_CHAINE; c = Precedente(c)) chaine[n++] = c;
        CoupListe *cl = &g_coupsListe[g_nbCoupsListe++];
        memset(cl, 0, sizeof *cl);
        cl->fiche = f; cl->delta = delta; cl->n = n; cl->dis = INCONNU; cl->meilleure = -1;
        for (int k = 0; k < n; k++) cl->ids[k] = g_fiches[chaine[n - 1 - k]].id;
    }
    Journal("exo_liste", g_nbCoupsListe, d->perso, g_nbFiches);
}

#define T_H 0x01
#define T_P 0x02
#define T_K 0x04
#define T_BAS 0x10
#define T_HAUT 0x20
#define T_AVANT 0x40
#define T_ARRIERE 0x80
#define DELAI_SUITE 4
static WORD Direction(char c)
{
    const int d = c - '0';
    WORD b = 0;
    if (d <= 3) b |= T_BAS; else if (d >= 7) b |= T_HAUT;
    if (d % 3 == 0) b |= T_AVANT; else if (d % 3 == 1) b |= T_ARRIERE;
    return b;
}
static int Ajouter(Entree *p, int n, WORD bits, int frames, int attendre)
{
    if (n >= NB_PROG) return NB_PROG + 1;
    p[n].bits = bits; p[n].frames = (BYTE)frames; p[n].attendre = (BYTE)attendre;
    return n + 1;
}

static int Etape(Entree *p, int n, WORD dir, WORD btn)
{
    if (n) n = Ajouter(p, n, 0, 0, 1);
    if (dir) n = Ajouter(p, n, dir, 1, 0);
    n = Ajouter(p, n, dir | btn, 2, 0);
    return Ajouter(p, n, 0, 1, 0);
}

static int Compiler(const char *net, Entree *p)
{
    int n = 0;
    WORD dir = 0, btn = 0;
    BOOL plus = FALSE, direction = FALSE;
    for (int i = 0; net[i]; i++) {
        const char c = net[i] == 'y' ? '2' : net[i];
        if (c >= '1' && c <= '9') {
            if (btn) { n = Etape(p, n, dir, btn); btn = 0; dir = 0; direction = FALSE; }
            if (direction) { n = Ajouter(p, n, dir, 2, 0); n = Ajouter(p, n, 0, 1, 0); }
            dir = c == '5' ? 0 : Direction(c); direction = TRUE; plus = FALSE;
        } else if (c == 'P' || c == 'K' || c == 'H' || c == 'F' || c == 'N') {
            if (btn && !plus) { n = Etape(p, n, dir, btn); btn = 0; dir = 0; direction = FALSE; }
            btn |= c == 'P' ? T_P : c == 'K' ? T_K : c == 'H' || c == 'F' ? T_H : (T_P | T_H);
            plus = FALSE;
        } else if (c == '+') plus = TRUE;
        else return -1;
    }
    if (!btn) return -1;
    n = Etape(p, n, dir, btn);
    return n > NB_PROG ? -1 : n;
}

static void ChoisirCoup(int sens, int slot)
{
    if (!g_nbCoupsListe) { Afficher(GRIS, L"EXERCISE", L"-   no punishable move found (dummy must be neutral)"); return; }
    g_choix = (g_choix + sens + g_nbCoupsListe) % g_nbCoupsListe;
    const CoupListe *c = &g_coupsListe[g_choix];
    const Fiche *f = &g_fiches[c->fiche];
    WCHAR l1[64], l2[128], w[24];
    int k = 0;
    for (; k < 23 && f->net[k]; k++) w[k] = f->net[k] >= 0x20 && f->net[k] < 0x7F ? (WCHAR)f->net[k] : L'?';
    w[k] = 0;
    swprintf(l1, 64, L"MOVE %d/%d", g_choix + 1, g_nbCoupsListe);
    Entree prog[NB_PROG];
    const int np = Compiler(f->net, prog);
    const BOOL demo = L16(f->adr + 0x40) || L16(f->adr + 0x24);
    if (np < 0 && !demo) swprintf(l2, 128, L"%ls   -%d on block   page %d   (old method)", w, c->delta, f->page + 1);
    else swprintf(l2, 128, L"%ls   -%d on block   page %d", w, c->delta, f->page + 1);
    Afficher(BLEU, l1, l2);
    EnterCriticalSection(&g_csExo);
    g_exo.n = c->n; memcpy(g_exo.ids, c->ids, sizeof g_exo.ids); g_exo.version++; g_exo.perso = g_persoListe;
    g_exo.requis = L32(f->adr + 0x0C);
    g_exo.demo = (WORD)L16(f->adr + 0x40);
    if (!g_exo.demo) g_exo.demo = (WORD)L16(f->adr + 0x24);
    g_exo.nProg = np; if (np > 0) memcpy(g_exo.prog, prog, np * sizeof prog[0]);
    LeaveCriticalSection(&g_csExo);
    Journal("exo_prog", np, f->page, (long)c->ids[c->n - 1]);
    Journal("exo_demo", (long)(WORD)L16(f->adr + 0x40), (long)(WORD)L16(f->adr + 0x24), (long)L32(f->adr + 0x0C));
    for (int k = 0x0C; k < 0x44; k += 12)
        Journal("exo_fiche", (long)L32(f->adr + k), (long)L32(f->adr + k + 4), (long)L32(f->adr + k + 8));
    Journal("exo_choix", f->page, f->index, c->n);
    for (int t = 0; t < c->n; t++) Journal("exo_id", t, (long)c->ids[t], FinActives(slot, c->ids[t]));
    for (int t = 0; t + 1 < c->n; t++) ChercherBranche(slot, c->ids[t], c->ids[t + 1]);
    EnterCriticalSection(&g_csExo);
    for (int t = 0; t < c->n; t++) g_exo.fin[t] = FinActives(slot, c->ids[t]);
    LeaveCriticalSection(&g_csExo);
}

#define RVA_FORCER_ACTION 0x34D770
#define DISTANCE_EXO      1.2f
#define PAUSE_EXO         30
typedef void (__cdecl *ForcerAction_t)(DWORD, int, DWORD);
static void RemettreEnPlace(int dummy, DWORD requis)
{
    BYTE *jd = base + RVA_JOUEURS + dummy * PAS_JOUEUR, *jp = base + RVA_JOUEURS + (1 - dummy) * PAS_JOUEUR;
    float xd, zd, xp, zp;
    memcpy(&xd, jd + 0xC, 4); memcpy(&zd, jd + 0x14, 4); memcpy(&xp, jp + 0xC, 4); memcpy(&zp, jp + 0x14, 4);
    const float dx = xd - xp, dz = zd - zp, l = sqrtf(dx * dx + dz * dz);
    Journal("exo_place", (long)(l * 100), 0, 0);
    if (l > 0.05f && l < 20.0f) {
        xd = xp + dx / l * DISTANCE_EXO; zd = zp + dz / l * DISTANCE_EXO;
        memcpy(jd + 0xC, &xd, 4); memcpy(jd + 0x14, &zd, 4);
    }
    WORD ref; memcpy(&ref, base + RVA_ANGLE_REFERENCE, 2);
    WORD angle = (WORD)(ref + (dummy ? 0x8000 : 0));
    const int perso = jd[4];
    DWORD action = 0;
    if (RequisDos(requis)) angle += 0x8000;
    else if (requis == 9) action = perso == 7 ? 0x1F55 : 0x1F6E;
    else if ((requis >= 0x41 && requis <= 0x43) || (requis >= 0x66 && requis <= 0x68)) {
        action = 0x1F52; if (requis == 0x43 || requis == 0x67) angle += 0x8000;
    } else if (requis == 0x5E) action = 0x1F62;
    jd[0x5D2] = (BYTE)(angle != (WORD)(ref + (dummy ? 0x8000 : 0)));
    const DWORD a32 = angle;
    memcpy(jd + 0x1C, &angle, 2); memcpy(jd + 0x20, &a32, 4); memcpy(jd + 0x24, &angle, 2);
    ((ForcerAction_t)(base + RVA_FORCER_ACTION))(action, dummy, 0);
    Journal("exo_requis", (long)requis, (long)action, jd[0x5D2]);
}

#define OFS_HISTORIQUE 0x4F0
#define OFS_INDEX_HIST 0x550
#define OFS_COTE       0x5D0
#define RVA_RECONNAISSEUR 0x29BA60
typedef void (__cdecl *Reconnaisseur_t)(DWORD, DWORD, BYTE *);
static void EcrireEntrees(BYTE *j, WORD rel)
{
    WORD bits = rel & 0xFF3F;
    if (j[OFS_COTE] == 1) { if (rel & T_AVANT) bits |= T_ARRIERE; if (rel & T_ARRIERE) bits |= T_AVANT; }
    else bits |= rel & (T_AVANT | T_ARRIERE);
    WORD prec; memcpy(&prec, j + OFS_MAINTENU, 2);
    const WORD appuye = bits & ~prec, relache = prec & ~bits;
    memcpy(j + OFS_MAINTENU, &bits, 2); memcpy(j + OFS_MAINTENU + 2, &appuye, 2); memcpy(j + OFS_MAINTENU + 4, &relache, 2);
    BYTE *h = j + OFS_HISTORIQUE;
    BYTE idx = j[OFS_INDEX_HIST] & 0xF;
    WORD cur; memcpy(&cur, h + idx * 6 + 2, 2);
    if (bits != cur) {
        idx = (idx + 1) & 0xF; j[OFS_INDEX_HIST] = idx;
        WORD e[3] = {0, bits, relache};
        if ((bits & 0x8F0) != (appuye & 0x8F0)) e[2] |= bits & 0x8F0;
        memcpy(h + idx * 6, e, 6);
        const WORD fin[3] = {999, 0, 0};
        memcpy(h + ((idx + 1) & 0xF) * 6, fin, 6);
    }
    WORD nb; memcpy(&nb, h + idx * 6, 2);
    if (nb < 999) { nb++; memcpy(h + idx * 6, &nb, 2); }
}

#define RVA_DEMO_DEMARRER 0x2A2360
#define RVA_DEMO_AVANCER  0x2A6600
typedef void (__cdecl *DemoDemarrer_t)(DWORD, DWORD);
typedef BYTE (__cdecl *DemoAvancer_t)(DWORD, DWORD);

static BOOL ExerciceManette(int dummy)
{
    static int pc = -1, reste, attente, delai, apres, version = -1, traces, nProg, persoExo = -1;
    static long frames;
    static DWORD actionRef, fin, requis, demo;
    static BOOL demoFinie;
    static DWORD actionApres;
    static BOOL atteint;
    static BOOL demoVide, bouge;
    static int forcage;
    static Entree prog[NB_PROG];
    if (dummy < 0 || dummy > 1 || !HorsLigne() || !Training_Libre()) { pc = -1; return FALSE; }
    BOOL actif; int v;
    EnterCriticalSection(&g_csExo);
    actif = g_exo.actif; v = g_exo.version;
    if (v != version) { nProg = g_exo.nProg; if (nProg > 0) memcpy(prog, g_exo.prog, nProg * sizeof prog[0]); fin = g_exo.n ? g_exo.ids[g_exo.n - 1] : 0; persoExo = g_exo.perso; requis = g_exo.requis; demo = g_exo.demo; }
    LeaveCriticalSection(&g_csExo);
    if (v != version) { version = v; pc = -1; attente = 0; traces = 0; demoVide = FALSE; }
    if (demoVide) demo = 0;
    BYTE *j = base + RVA_JOUEURS + dummy * PAS_JOUEUR, *o = base + RVA_JOUEURS + (1 - dummy) * PAS_JOUEUR;
    if (!actif || (nProg <= 0 && !demo) || j[4] != persoExo) { pc = -1; attente = 0; return FALSE; }
    DWORD action; memcpy(&action, j + 0x6C, 4);
    const BYTE etat = j[0x70];
    if (pc < 0) {
        if (g_exoPause) { attente = 0; return FALSE; }
        attente = attente >= ATTENTE_EXO || (etat == ETAT_NEUTRE && o[0x70] == ETAT_NEUTRE) ? attente + 1 : 0;
        if (attente == ATTENTE_EXO) RemettreEnPlace(dummy, requis);
        if (attente < ATTENTE_EXO + PAUSE_EXO) return FALSE;
        pc = 0; reste = demo ? 0 : prog[0].frames; frames = 0; attente = 0; delai = 0; apres = 0; actionRef = action; atteint = FALSE; bouge = FALSE;
        Journal("exo_depart", nProg, (long)action, (long)demo);
        InterlockedExchange(&g_exoJoue, 0);
        forcage = 0; InterlockedExchange(&g_exoForcage, 0); InterlockedExchange(&g_exoForceEchec, 0);
        InterlockedExchange(&g_exoAtteint, 0); InterlockedExchange(&g_exoPosture, 0);
        InterlockedIncrement(&g_essais);
        if (demo) { ((DemoDemarrer_t)(base + RVA_DEMO_DEMARRER))(dummy, demo); demoFinie = FALSE; }
    }
    frames++;
    if (etat == ETAT_ATTAQUE || etat == 2 || etat == 5) InterlockedExchange(&g_exoJoue, (LONG)action);
    if (action == fin && !atteint) { atteint = TRUE; InterlockedExchange(&g_exoAtteint, 1); }
    if (action != actionRef) bouge = TRUE;
    if (demo) {
        if (!demoFinie) {
            demoFinie = !((DemoAvancer_t)(base + RVA_DEMO_AVANCER))(dummy, 1 - dummy);
            if (demoFinie) actionApres = action;
            if (traces < NB_TRACES_EXO) {
                DWORD demande; memcpy(&demande, j + DEMANDE_ACTION, 4);
                CONTROLE("exo_demo_trace", frames, (long)action, etat | (long)(demande & 0xFFFF) << 8);
            }
        } else if (traces < NB_TRACES_EXO && action != actionApres) {
            actionApres = action;
            CONTROLE("exo_demo_apres", frames, (long)action, etat);
        }

        const LONG derniere = g_exoJoue;
        DWORD demandeDemo; memcpy(&demandeDemo, j + DEMANDE_ACTION, 4);
        const LONG ecartFin = derniere > (LONG)fin ? derniere - (LONG)fin : (LONG)fin - derniere;
        const BOOL variante = derniere && ecartFin <= 2;
        if (variante && !atteint) { atteint = TRUE; InterlockedExchange(&g_exoAtteint, 1); }
        if (!atteint && etat == 13 && derniere) InterlockedExchange(&g_exoPosture, 1);
        if (!atteint && etat == 13 && derniere && (demandeDemo & 0xFFFF) == 0xFFFF && forcage >= 0 && fin) {
            if (forcage == 0) { InterlockedExchange(&g_exoForcage, 1); Journal("exo_force", (long)fin, (long)action, derniere); }
            if (++forcage <= 30) memcpy(j + DEMANDE_ACTION, &fin, 4);
            else forcage = -1;
        }
        if (atteint && g_exoForcage) { InterlockedExchange(&g_exoForcage, 0); Journal("exo_force_ok", (long)fin, forcage, 0); }
        if (demoFinie && ((++apres > 10 && etat == ETAT_NEUTRE) || frames > DUREE_MAX_EXO)) {
            if (g_exoForcage && !atteint) { InterlockedExchange(&g_exoForceEchec, 1); Journal("exo_force_echec", (long)fin, forcage, 0); }
            InterlockedExchange(&g_exoForcage, 0);
            if (!atteint) InterlockedIncrement(&g_exoRate);
            Journal("exo_fin", atteint ? 1 : 3, frames, (long)action); traces++; pc = -1;
            if (!atteint && !bouge && nProg > 0) { demoVide = TRUE; Journal("exo_demo_vide", (long)demo, nProg, (long)fin); }
            return FALSE;
        }
        if (frames > DUREE_MAX_EXO + 60) { Journal("exo_fin", 5, frames, (long)action); traces++; pc = -1; return FALSE; }
        return TRUE;
    }
    WORD bits = 0;
    if (pc < nProg) {
        const Entree *e = &prog[pc];
        if (e->attendre) {
            if (action != actionRef) delai++;
            if (delai >= DELAI_SUITE) { pc++; delai = 0; if (pc < nProg) reste = prog[pc].frames; }
            else if (frames > DUREE_MAX_EXO) { Journal("exo_fin", 2, frames, pc); traces++; pc = -1; return FALSE; }
        } else {
            bits = e->bits;
            if (bits & (T_H | T_P | T_K)) actionRef = action;
            if (--reste <= 0) { pc++; if (pc < nProg) reste = prog[pc].frames; }
        }
    } else if ((++apres > 10 && etat == ETAT_NEUTRE) || frames > DUREE_MAX_EXO) {
        if (!atteint) InterlockedIncrement(&g_exoRate);
        Journal("exo_fin", atteint ? 1 : 3, frames, (long)action); traces++; pc = -1; return FALSE;
    }
    EcrireEntrees(j, bits);
    if (traces < NB_TRACES_EXO && frames <= DUREE_MAX_EXO) {
        DWORD demande; memcpy(&demande, j + DEMANDE_ACTION, 4);
        CONTROLE("exo_entree", frames, (long)action, bits | (long)j[OFS_COTE] << 8 | (long)(demande & 0xFFFF) << 16);
    }
    ((Reconnaisseur_t)(base + RVA_RECONNAISSEUR))(dummy, 1 - dummy, j + OFS_HISTORIQUE);
    return TRUE;
}

static void Exercice(int dummy)
{
    static int etape = -1, attente, version = -1, traces;
    static long frames;
    if (dummy < 0 || dummy > 1 || !HorsLigne() || !Training_Libre()) { etape = -1; return; }
    static DWORD actionPrec; static int depuis;
    int n, fin[NB_CHAINE]; DWORD ids[NB_CHAINE]; BOOL actif; int v;
    EnterCriticalSection(&g_csExo);
    actif = g_exo.actif; n = g_exo.n; v = g_exo.version; memcpy(ids, g_exo.ids, sizeof ids); memcpy(fin, g_exo.fin, sizeof fin);
    const int persoExo = g_exo.perso; const DWORD requis = g_exo.requis;
    LeaveCriticalSection(&g_csExo);
    if (v != version) { version = v; etape = -1; attente = 0; traces = 0; }
    int np; DWORD demo; EnterCriticalSection(&g_csExo); np = g_exo.nProg; demo = g_exo.demo; LeaveCriticalSection(&g_csExo);
    if (demo) np = 1;
    BYTE *j = base + RVA_JOUEURS + dummy * PAS_JOUEUR, *o = base + RVA_JOUEURS + (1 - dummy) * PAS_JOUEUR;
    if (!actif || n <= 0 || np > 0 || j[4] != persoExo) { etape = -1; attente = 0; return; }
    DWORD action; memcpy(&action, j + 0x6C, 4);
    const BYTE etat = j[0x70];
    if (action != actionPrec) { actionPrec = action; depuis = 0; }
    depuis++;
    if (etape < 0) {
        if (g_exoPause) { attente = 0; return; }
        attente = attente >= ATTENTE_EXO || (etat == ETAT_NEUTRE && o[0x70] == ETAT_NEUTRE) ? attente + 1 : 0;
        if (attente == ATTENTE_EXO) RemettreEnPlace(dummy, requis);
        if (attente < ATTENTE_EXO + PAUSE_EXO) return;
        etape = 0; frames = 0; attente = 0;
        Journal("exo_depart", n, (long)ids[0], traces);
    }
    frames++;
    if (traces < NB_TRACES_EXO && frames <= DUREE_MAX_EXO) {
        DWORD demande; memcpy(&demande, j + DEMANDE_ACTION, 4);
        CONTROLE("exo_trace", frames, (long)action, etat | (long)(demande & 0xFFFF) << 8);
    }
    if (action == ids[etape]) {
        if (etape) Journal("exo_enchaine", (long)ids[etape - 1], (long)action, frames);
        if (++etape == n) { Journal("exo_fin", 1, frames, n); traces++; etape = -1; return; }
    }
    if (frames > DUREE_MAX_EXO) { Journal("exo_fin", 2, frames, etape); traces++; etape = -1; return; }

    if (etape > 0 && action == ids[etape - 1] && fin[etape - 1] > 0 && depuis <= fin[etape - 1] + 1) return;
    const DWORD demande = ids[etape];
    BYTE r[12];
    const DWORD a = Action(j[0x28], demande);
    if (!a || !Lu(a, r, 12) || !EstFrappe(r[2])) { Journal("exo_fin", 4, frames, (long)demande); traces++; etape = -1; return; }
    memcpy(j + DEMANDE_ACTION, &demande, 4);
}
typedef BYTE (__cdecl *IADummy_t)(int *, BYTE *);
static IADummy_t o_iaDummy;

#define RVA_JOUER_SON   0x557C40
#define SON_ETAPE       0x10002
#define SON_REUSSI      0x1001F
typedef int (__cdecl *JouerSon_t)(int, int);
static volatile LONG g_sonDemande;
static volatile LONG g_goodT;
static BYTE __cdecl hk_iaDummy(int *demande, BYTE *paire)
{
    const LONG son = InterlockedExchange(&g_sonDemande, 0);
    if (son) ((JouerSon_t)(base + RVA_JOUER_SON))(son, 0);
    if (ExerciceManette(paire[0])) return 1;
    const BYTE r = o_iaDummy(demande, paire);
    Exercice(paire[0]);
    return r;
}

static BOOL AccrocherIADummy(void)
{
    static const BYTE attendu[] = {0xE8, 0x6D, 0x5B, 0x01, 0x00};
    BYTE *site = base + RVA_APPEL_IA_DUMMY;
    if (memcmp(site, attendu, sizeof attendu)) return FALSE;
    o_iaDummy = (IADummy_t)(base + RVA_IA_DUMMY);
    DWORD ancien;
    if (!VirtualProtect(site + 1, 4, PAGE_EXECUTE_READWRITE, &ancien)) return FALSE;
    const LONG rel = (LONG)((BYTE *)hk_iaDummy - (site + 5));
    memcpy(site + 1, &rel, 4);
    VirtualProtect(site + 1, 4, ancien, &ancien);
    FlushInstructionCache(GetCurrentProcess(), site, 5);
    return TRUE;
}

static volatile LONG g_pret;
#define TOUCHE_BASCULE VK_F9
#define TOUCHE_EXERCICE VK_F8
static BOOL JeuAuPremierPlan(void)
{
    DWORD pid = 0;
    HWND w = GetForegroundWindow();
    if (!w) return FALSE;
    GetWindowThreadProcessId(w, &pid);
    return pid == GetCurrentProcessId();
}

#define NB_SAUVES 4096
static struct { BYTE joueur, dummy; WORD action; } g_sauves[NB_SAUVES];
static int g_nbSauves;
static char g_cheminSave[MAX_PATH];
static int g_persoJoueurListe = -1;
static BOOL Sauve(int joueur, int dummy, DWORD action)
{
    for (int i = 0; i < g_nbSauves; i++)
        if (g_sauves[i].joueur == joueur && g_sauves[i].dummy == dummy && g_sauves[i].action == action) return TRUE;
    return FALSE;
}
static void MemoriserDis(int dummy, DWORD action, int dis);
static void MemoriserInjouable(int dummy, DWORD action, int type);
static void ChargerSauvegarde(void)
{
    FILE *f = g_cheminSave[0] ? fopen(g_cheminSave, "r") : NULL;
    if (!f) return;
    int a, b, c;
    char ligne[64];
    while (fgets(ligne, sizeof ligne, f)) {
        if (sscanf(ligne, "I %d %d %d", &a, &b, &c) == 3) {
            if (a >= 0 && a < 256 && b > 0 && b < 0x10000 && c == 2) MemoriserInjouable(a, (DWORD)b, c);
        } else if (sscanf(ligne, "D %d %d %d", &a, &b, &c) == 3) {
            if (a >= 0 && a < 256 && b > 0 && b < 0x10000 && c > -100 && c < 100) MemoriserDis(a, (DWORD)b, c);
        } else if (sscanf(ligne, "%d %d %d", &a, &b, &c) == 3 && g_nbSauves < NB_SAUVES) {
            if (a >= 0 && a < 256 && b >= 0 && b < 256 && c > 0 && c < 0x10000 && !Sauve(a, b, (DWORD)c)) {
                g_sauves[g_nbSauves].joueur = (BYTE)a; g_sauves[g_nbSauves].dummy = (BYTE)b; g_sauves[g_nbSauves++].action = (WORD)c;
            }
        }
    }
    fclose(f);
}

#define NB_DIS_SAUVES 2048
static struct { BYTE dummy; WORD action; short dis; } g_disSauves[NB_DIS_SAUVES];
static int g_nbDisSauves;
static int ChercherDis(int dummy, DWORD action)
{
    for (int i = 0; i < g_nbDisSauves; i++) if (g_disSauves[i].dummy == dummy && g_disSauves[i].action == action) return i;
    return -1;
}
static void MemoriserDis(int dummy, DWORD action, int dis)
{
    const int k = ChercherDis(dummy, action);
    if (k >= 0) { g_disSauves[k].dis = (short)dis; return; }
    if (g_nbDisSauves >= NB_DIS_SAUVES) return;
    g_disSauves[g_nbDisSauves].dummy = (BYTE)dummy; g_disSauves[g_nbDisSauves].action = (WORD)action;
    g_disSauves[g_nbDisSauves++].dis = (short)dis;
}
static void SauverDis(int dummy, DWORD action, int dis)
{
    const int k = ChercherDis(dummy, action);
    if (k >= 0 && g_disSauves[k].dis == dis) return;
    MemoriserDis(dummy, action, dis);
    FILE *f = g_cheminSave[0] ? fopen(g_cheminSave, "a") : NULL;
    if (!f) return;
    fprintf(f, "D %d %lu %d\n", dummy, (unsigned long)action, dis);
    fclose(f);
}

#define NB_INJOUABLES 512
static struct { BYTE dummy, type; WORD action; } g_injouables[NB_INJOUABLES];
static int g_nbInjouables;
static int Injouable(int dummy, DWORD action)
{
    for (int i = 0; i < g_nbInjouables; i++) if (g_injouables[i].dummy == dummy && g_injouables[i].action == action) return g_injouables[i].type;
    return 0;
}
static void MemoriserInjouable(int dummy, DWORD action, int type)
{
    if (Injouable(dummy, action) || g_nbInjouables >= NB_INJOUABLES) return;
    g_injouables[g_nbInjouables].dummy = (BYTE)dummy; g_injouables[g_nbInjouables].action = (WORD)action;
    g_injouables[g_nbInjouables++].type = (BYTE)type;
}
static void SauverInjouable(int dummy, DWORD action, int type)
{
    if (Injouable(dummy, action)) return;
    MemoriserInjouable(dummy, action, type);
    FILE *f = g_cheminSave[0] ? fopen(g_cheminSave, "a") : NULL;
    if (!f) return;
    fprintf(f, "I %d %lu %d\n", dummy, (unsigned long)action, type);
    fclose(f);
}
static void Sauver(int joueur, int dummy, DWORD action)
{
    if (Sauve(joueur, dummy, action) || g_nbSauves >= NB_SAUVES) return;
    g_sauves[g_nbSauves].joueur = (BYTE)joueur; g_sauves[g_nbSauves].dummy = (BYTE)dummy; g_sauves[g_nbSauves++].action = (WORD)action;
    FILE *f = g_cheminSave[0] ? fopen(g_cheminSave, "a") : NULL;
    if (!f) { Journal("defi_sauvegarde_impossible", 0, 0, 0); return; }
    fprintf(f, "%d %d %lu\n", joueur, dummy, (unsigned long)action);
    fclose(f);
}
static void MarquerFaits(int joueur)
{
    g_persoJoueurListe = joueur;
    for (int k = 0; k < g_nbCoupsListe; k++) {
        CoupListe *c = &g_coupsListe[k];
        c->fait = Sauve(joueur, g_persoListe, c->ids[c->n - 1]);
        if (!c->injouable) c->injouable = (BYTE)Injouable(g_persoListe, c->ids[c->n - 1]);
        const int k2 = ChercherDis(g_persoListe, c->ids[c->n - 1]);
        if (c->dis == INCONNU && k2 >= 0) {
            c->dis = g_disSauves[k2].dis;
            if (c->dis <= 0) c->meilleure = 0;
        }
    }
}
static int NbFaits(void) { int n = 0; for (int k = 0; k < g_nbCoupsListe; k++) n += g_coupsListe[k].fait; return n; }

static BOOL Impossible(const CoupListe *c) { return c->dis != INCONNU && c->meilleure == 0; }

static volatile LONG g_menuOuvert;
static volatile LONG g_bloquerManettes;
static int g_curseur;
static BOOL g_exoAvant;
static DWORD g_avancerA;
static volatile LONG g_versionListe;
static DWORD g_fermeA;
static BOOL ExerciceActif(void) { EnterCriticalSection(&g_csExo); const BOOL a = g_exo.actif; LeaveCriticalSection(&g_csExo); return a; }
static void ExerciceMettre(BOOL a) { EnterCriticalSection(&g_csExo); g_exo.actif = a; LeaveCriticalSection(&g_csExo); }

static void NoterDefi(BOOL connu, int couleur, BOOL retenu)
{
    if (s.persoDummy != g_persoListe || !s.coup) return;
    const BOOL exo = ExerciceActif() && !g_menuOuvert;

    const DWORD derniere = (DWORD)g_exoJoue;
    const BOOL joue = exo && g_choix < g_nbCoupsListe && derniere && (s.coup == derniere || s.coupContact == derniere)
                      && (g_exoAtteint || !g_exoPosture);
    for (int k = 0; k < g_nbCoupsListe; k++) {
        CoupListe *c = &g_coupsListe[k];
        if (c->ids[c->n - 1] != s.coup && !(joue && k == g_choix)) continue;
        if (exo && k == g_choix) g_defiRattache = TRUE;
        const DWORD id = c->ids[c->n - 1];

        const BOOL piege = couleur == BLEU && s.dis <= 0;
        if (connu && (s.gardeOrdinaire || piege)) {
            int i = -1;
            c->dis = s.dis;
            if (s.distFin > 0) c->distance = s.distFin;
            if (s.dis != INITIATIVE) SauverDis(s.persoDummy, id, s.dis);
            c->meilleure = s.dis > 0 ? MeilleureChoppe(s.dis, &i) : 0;
            c->netMeilleure[0] = 0;
            if (c->meilleure > 0 && i >= 0) memcpy(c->netMeilleure, g_liste->netNeutres[i], sizeof c->netMeilleure);
        }
        const BOOL reussi = couleur == DORE || (retenu && connu && (s.gardeOrdinaire || piege) && Impossible(c));
        if (exo && reussi && !c->fait) {
            c->fait = TRUE;
            Sauver(s.persoJoueur, s.persoDummy, id);
        }
    }
    InterlockedIncrement(&g_versionListe);
    if (!exo || g_choix >= g_nbCoupsListe) return;
    const CoupListe *c = &g_coupsListe[g_choix];
    if (c->ids[c->n - 1] != s.coup && !joue) return;
    if (c->fait && (couleur == DORE || (retenu && Impossible(c)))) {
        WCHAR l2[128];
        if (couleur == DORE) swprintf(l2, 128, L"-%d   %d/%d cleared", s.dis, NbFaits(), g_nbCoupsListe);
        else swprintf(l2, 128, L"safe: blocked, no punish   %d/%d cleared", NbFaits(), g_nbCoupsListe);
        Afficher(DORE, L"CHALLENGE CLEARED", l2);
        Journal("defi_reussi", g_choix, NbFaits(), g_nbCoupsListe);
        InterlockedExchange(&g_sonDemande, SON_REUSSI);
        InterlockedExchange(&g_goodT, (LONG)(GetTickCount() | 1));
        g_avancerA = GetTickCount() + 1600;
        InterlockedExchange(&g_exoPause, 1);
    }
}

static int Suivant(int depuis)
{
    for (int d = 1; d <= g_nbCoupsListe; d++) {
        const int k = (depuis + d) % g_nbCoupsListe;
        if (!g_coupsListe[k].fait && !g_coupsListe[k].injouable) return k;
    }
    return -1;
}

static int Dummy(Combattant cur[2])
{
    Lire(0, &cur[0]); Lire(1, &cur[1]);
    return cur[1].humain != 1 ? 1 : cur[0].humain != 1 ? 0 : -1;
}
static void Demarrer_Coup(int k, const Combattant *d)
{
    InterlockedExchange(&g_exoPause, 0);
    InterlockedExchange(&g_exoRate, 0); g_toucheGarde = 0; g_defiRattache = FALSE;
    g_choix = k;
    ExerciceMettre(TRUE);
    ChoisirCoup(0, d->b7e ? d->slot100 : d->slot28);
    Journal("defi_choix", k, g_coupsListe[k].fait, NbFaits());
    InterlockedIncrement(&g_versionListe);
}
static void Avancer(void)
{
    Combattant cur[2];
    const int dm = Dummy(cur);
    if (dm < 0 || !g_nbCoupsListe) return;
    const int k = Suivant(g_choix);
    if (k < 0) {
        InterlockedExchange(&g_exoPause, 0);
        ExerciceMettre(FALSE);
        Afficher(DORE, L"ALL CLEARED", L"F8: list");
        Journal("defi_tout", NbFaits(), g_nbCoupsListe, 0);
        InterlockedIncrement(&g_versionListe);
        return;
    }
    Demarrer_Coup(k, &cur[dm]);
}

#define RVA_MANETTES_BRUT  0x2057928
#define PAD_HAUT    0x10001
#define PAD_BAS     0x20002
#define PAD_OK      0x10
#define PAD_RETOUR  0x20
#define PAD_ARRET   0x80
#define PAD_LISTE   0x400
#define PAD_LISTE_TOUS (PAD_HAUT | PAD_BAS | PAD_OK | PAD_RETOUR | PAD_ARRET | PAD_LISTE)
static DWORD BoutonsManettes(void)
{
    DWORD t = 0;
    for (int d = 0; d < 5; d++) t |= L32((DWORD)(ULONG_PTR)base + RVA_MANETTES_BRUT + d * 0x2C);
    return t;
}

#define RVA_APPEL_COPIER_MANETTES 0x3F6829
#define RVA_COPIER_MANETTES       0x3F52B0
#define RVA_ENTREES_PLACES        0x10878F8
typedef void (__cdecl *CopierManettes_t)(void);
static CopierManettes_t o_copierManettes;
static void __cdecl hk_copierManettes(void)
{
    o_copierManettes();
    if (g_bloquerManettes) memset(base + RVA_ENTREES_PLACES, 0, 4 * 0x2C);
}

static BYTE *CibleAppel(const BYTE *site) { LONG rel; memcpy(&rel, site + 1, 4); return (BYTE *)site + 5 + rel; }
static void EcrireAppel(BYTE *site, const void *cible)
{
    DWORD ancien;
    if (!VirtualProtect(site + 1, 4, PAGE_EXECUTE_READWRITE, &ancien)) return;
    const LONG rel = (LONG)((const BYTE *)cible - (site + 5));
    memcpy(site + 1, &rel, 4);
    VirtualProtect(site + 1, 4, ancien, &ancien);
    FlushInstructionCache(GetCurrentProcess(), site, 5);
}
static BOOL AccrocherManettes(void)
{
    BYTE *site = base + RVA_APPEL_COPIER_MANETTES;
    if (site[0] != 0xE8) return FALSE;
    BYTE *cible = CibleAppel(site);
    if (cible == (BYTE *)hk_copierManettes) return TRUE;
    MEMORY_BASIC_INFORMATION m;
    const BOOL origine = cible == base + RVA_COPIER_MANETTES;
    const BOOL autre = !origine && VirtualQuery(cible, &m, sizeof m) && m.State == MEM_COMMIT && (HMODULE)m.AllocationBase != GetModuleHandleW(NULL);
    if (!origine && !autre) return FALSE;
    o_copierManettes = (CopierManettes_t)cible;
    EcrireAppel(site, hk_copierManettes);
    return TRUE;
}
static void DecrocherManettes(void)
{
    BYTE *site = base + RVA_APPEL_COPIER_MANETTES;
    if (site[0] == 0xE8 && o_copierManettes && CibleAppel(site) == (BYTE *)hk_copierManettes) EcrireAppel(site, o_copierManettes);
}
static void OuvrirListe(void)
{
    Combattant cur[2];
    const int dm = Dummy(cur);
    if (dm < 0) { Afficher(GRIS, L"PUNISH CHALLENGE", L"-   no dummy"); return; }
    if (g_persoListe != cur[dm].perso) {
        if (cur[dm].etat != ETAT_NEUTRE || cur[dm].b7e) { Afficher(GRIS, L"PUNISH CHALLENGE", L"-   wait for the dummy to stand still"); return; }
        ListerCoups(&cur[dm]);
        g_persoJoueurListe = -1;
    }
    if (!g_nbCoupsListe) { Afficher(GRIS, L"PUNISH CHALLENGE", L"-   no punishable move found"); return; }
    if (g_persoJoueurListe != cur[1 - dm].perso) MarquerFaits(cur[1 - dm].perso);
    g_exoAvant = ExerciceActif();
    ExerciceMettre(FALSE);
    g_curseur = g_exoAvant ? g_choix : (Suivant(-1) >= 0 ? Suivant(-1) : 0);
    g_avancerA = 0;
    if (AccrocherManettes()) InterlockedExchange(&g_bloquerManettes, 1);
    else Journal("crochet_manettes_refuse", 0, 0, 0);
    InterlockedExchange(&g_menuOuvert, 1);
    InterlockedIncrement(&g_versionListe);
    Journal("defi_liste", g_nbCoupsListe, NbFaits(), g_persoListe);
}

static void FermerListe(int quoi)
{
    InterlockedExchange(&g_menuOuvert, 0);
    g_fermeA = GetTickCount();
    Combattant cur[2];
    const int dm = Dummy(cur);
    if (quoi == 1 && dm >= 0 && g_curseur < g_nbCoupsListe) Demarrer_Coup(g_curseur, &cur[dm]);
    else if (quoi == 2) { ExerciceMettre(FALSE); Afficher(GRIS, L"CHALLENGE OFF", L"F8: list"); }
    else ExerciceMettre(g_exoAvant);
    Journal("defi_fermer", quoi, g_curseur, 0);
    InterlockedIncrement(&g_versionListe);
}
static BOOL Appui(int vk, BOOL *avant, BOOL premier)
{
    const BOOL bas = (GetAsyncKeyState(vk) & 0x8000) != 0;
    const BOOL r = bas && !*avant && premier;
    *avant = bas;
    return r;
}

static void CommandesDefi(void)
{
    static BOOL kF8, kHaut, kBas, kPrec, kSuiv, kEntree, kRetour;
    static DWORD padAvant, repeterA;
    static int notes;
    const BOOL premier = JeuAuPremierPlan(), training = DefiPossible();
    const DWORD pad = premier ? BoutonsManettes() : 0, appuye = pad & ~padAvant;
    padAvant = pad;
    for (DWORD b = 0x100; b <= 0x800 && notes < 20; b <<= 1)
        if (appuye & b) { notes++; Journal("bouton", (long)b, g_menuOuvert, 0); }
    if (!g_menuOuvert && g_bloquerManettes) {
        const BOOL relache = !(pad & PAD_LISTE_TOUS), delai = GetTickCount() - g_fermeA >= 1000;
        if (relache || delai) { InterlockedExchange(&g_bloquerManettes, 0); DecrocherManettes(); Journal("manettes_rendues", relache, delai, (long)(pad & 0xFFFF)); }
    }
    const BOOL f8 = Appui(TOUCHE_EXERCICE, &kF8, premier), prec = Appui(VK_PRIOR, &kPrec, premier), suiv = Appui(VK_NEXT, &kSuiv, premier);
    const BOOL haut = Appui(VK_UP, &kHaut, premier), bas = Appui(VK_DOWN, &kBas, premier);
    const BOOL entree = Appui(VK_RETURN, &kEntree, premier), retour = Appui(VK_BACK, &kRetour, premier);
    if (!training) {
        if (g_menuOuvert) FermerListe(0);
        if (ExerciceActif()) { ExerciceMettre(FALSE); Journal("defi_hors_training", 0, 0, 0); }
        g_avancerA = 0;
        return;
    }
    if (!g_menuOuvert) {
        if (f8 || (appuye & PAD_LISTE)) { OuvrirListe(); return; }
        if ((prec || suiv) && g_nbCoupsListe) {
            Combattant cur[2];
            const int dm = Dummy(cur);
            if (dm >= 0) Demarrer_Coup((g_choix + (prec ? -1 : 1) + g_nbCoupsListe) % g_nbCoupsListe, &cur[dm]);
        }
        if (g_avancerA && (LONG)(GetTickCount() - g_avancerA) >= 0) { g_avancerA = 0; Avancer(); }

        if (ExerciceActif() && !g_avancerA && !g_defiRattache && g_choix < g_nbCoupsListe) {
            const int pb = g_exoRate >= 2 ? 1 : g_toucheGarde >= 2 ? 2 : 0;
            if (pb) {
                g_coupsListe[g_choix].injouable = (BYTE)pb;
                if (pb == 2) SauverInjouable(g_persoListe, g_coupsListe[g_choix].ids[g_coupsListe[g_choix].n - 1], pb);
                Journal("defi_injouable", g_choix, pb, 0);
                if (pb == 2) {
                    CoupListe *c = &g_coupsListe[g_choix];
                    c->fait = TRUE;
                    Sauver(g_persoJoueurListe, g_persoListe, c->ids[c->n - 1]);
                    Afficher(DORE, L"UNBLOCKABLE", L"-   can't be blocked: cleared");
                    InterlockedExchange(&g_sonDemande, SON_REUSSI);
                } else if (g_exoForceEchec) {
                    CoupListe *c = &g_coupsListe[g_choix];
                    c->fait = TRUE;
                    Sauver(g_persoJoueurListe, g_persoListe, c->ids[c->n - 1]);
                    Afficher(DORE, L"NOT APPLICABLE ON BLOCK", L"-   the follow-up only exists on hit: cleared");
                    InterlockedExchange(&g_sonDemande, SON_REUSSI);
                } else Afficher(GRIS, L"CAN'T BE PLAYED", L"-   skipped");
                InterlockedIncrement(&g_versionListe);
                Avancer();
            }
        }
        return;
    }

    int pas = 0;
    const DWORD dirs = pad & (PAD_HAUT | PAD_BAS), maintenant = GetTickCount();
    if (appuye & dirs) { pas = (appuye & PAD_BAS) ? 1 : -1; repeterA = maintenant + 380; }
    else if (dirs && (LONG)(maintenant - repeterA) >= 0) { pas = (dirs & PAD_BAS) ? 1 : -1; repeterA = maintenant + 70; }
    if (haut) pas = -1;
    if (bas) pas = 1;
    if (prec) pas = -10;
    if (suiv) pas = 10;
    if (pas) {
        g_curseur += pas;
        if (g_curseur < 0) g_curseur = pas == -1 ? g_nbCoupsListe - 1 : 0;
        if (g_curseur >= g_nbCoupsListe) g_curseur = pas == 1 ? 0 : g_nbCoupsListe - 1;
        InterlockedIncrement(&g_versionListe);
    }
    if (entree || (appuye & PAD_OK)) FermerListe(1);
    else if (retour || (appuye & PAD_ARRET)) FermerListe(2);
    else if (f8 || (appuye & (PAD_RETOUR | PAD_LISTE))) FermerListe(0);
}

static void Echantillonner(void)
{
    static Combattant prec[2], cur[2];
    static BOOL actif = FALSE;
    if (!g_pret) return;
    {
        static BOOL avant;
        const BOOL bas = (GetAsyncKeyState(TOUCHE_BASCULE) & 0x8000) != 0;
        if (bas && !avant && JeuAuPremierPlan()) {
            g_mode = g_mode == 1 ? 2 : g_mode == 2 ? 0 : 1;
            s.phase = P_RIEN;
            Afficher(GRIS, g_mode == 1 ? L"PUNISH TRAINING" : g_mode == 2 ? L"PUNISH TRAINING +" : L"PUNISH TRAINING OFF",
                     g_mode == 1 ? L"minimal   F9: details" : g_mode == 2 ? L"details   F9: off" : L"F9: on");
            Journal("bascule", g_mode, 0, 0);
        }
        avant = bas;
    }
    CommandesDefi();
    if (!Actif()) {
        if (actif) {
            actif = FALSE; s.phase = P_RIEN; Journal("fin", 0, 0, 0);

            EnterCriticalSection(&g_csExo); g_exo.actif = FALSE; g_exo.n = 0; g_exo.nProg = 0; g_exo.version++; LeaveCriticalSection(&g_csExo);
            g_persoListe = -1; g_nbCoupsListe = 0; g_nbFiches = 0;
            InterlockedExchange(&g_menuOuvert, 0); InterlockedExchange(&g_bloquerManettes, 0); DecrocherManettes(); g_avancerA = 0;
        }
        return;
    }
    Lire(0, &cur[0]); Lire(1, &cur[1]);
    if (!actif) {
        actif = TRUE; prec[0] = cur[0]; prec[1] = cur[1];
        Journal("debut_combat", cur[0].humain, cur[1].humain, !HorsLigne());
        CONTROLE("combat_en_cours", (long)*(DWORD *)(base + RVA_COMBAT_EN_COURS), (long)*(DWORD *)(base + RVA_LECTEUR), 0);
        return;
    }

    int pas = 0;
    for (int k = 0; k < 2; k++) {
        int dk = 0;
        if (cur[k].cpt > prec[k].cpt) dk = cur[k].cpt - prec[k].cpt;
        else if (cur[k].cpt != prec[k].cpt) dk = cur[k].cpt;
        if (dk > pas) pas = dk;
    }
    if (pas > 10) pas = 1;
    if (pas == 0) return;
    g_image += pas;
    for (int k = 0; k < 2; k++) if (cur[k].action != prec[k].action) {
        static long debutAction[2];
        if ((prec[k].etat == ETAT_ATTAQUE || prec[k].etat == 2) && (cur[k].etat == ETAT_ATTAQUE || cur[k].etat == 2))
            CONTROLE(cur[k].humain == 1 ? "chaine_joueur" : "chaine_dummy", (long)prec[k].action, (long)cur[k].action, g_image - debutAction[k]);
        debutAction[k] = g_image;
        g_cptDebut[k] = cur[k].cpt - 1;
    }

    const int j = cur[0].humain == 1 ? 0 : cur[1].humain == 1 ? 1 : cur[0].humain ? 0 : cur[1].humain ? 1 : -1;
    if (j >= 0) {
        static WORD avant;
        WORD t; memcpy(&t, base + RVA_JOUEURS + j * PAS_JOUEUR + OFS_MAINTENU, 2);
        if (t != avant) CONTROLE("touches_joueur", t, base[RVA_JOUEURS + j * PAS_JOUEUR + OFS_COTE], DeDos(j, &cur[j]));
        avant = t;
    }
    if (j >= 0 && g_mode) {
        g_joueur = j;
        ListerChoppes(&cur[j]);
        Observer(&cur[j], &prec[j], &cur[1 - j], &prec[1 - j], g_image);
    }
    prec[0] = cur[0]; prec[1] = cur[1];
}

static int panel_x = 150, panel_y = 382, panel_w = 640, panel_h = 66;
static IDirect3DTexture9 *g_tex; static int tex_w, tex_h;
static LONG g_versionDessinee = -1;
static HDC dib_dc; static HBITMAP dib; static DWORD *dib_bits; static int dib_w, dib_h;
static HFONT police1, police2;
static BYTE *g_masque, *g_flou, *g_tampon;

static void Texte(HFONT f, int x, int y, int w, int h, const WCHAR *t)
{
    RECT r = {x, y, x + w, y + h}; SelectObject(dib_dc, f); SetTextColor(dib_dc, RGB(255, 255, 255));
    DrawTextW(dib_dc, t, -1, &r, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
}

static void Flouter(const BYTE *src, BYTE *dst, BYTE *tmp, int w, int h, int r)
{
    const int n = 2 * r + 1;
    for (int y = 0; y < h; y++) {
        int somme = 0;
        for (int x = -r; x <= r; x++) somme += src[y * w + (x < 0 ? 0 : x >= w ? w - 1 : x)];
        for (int x = 0; x < w; x++) {
            tmp[y * w + x] = (BYTE)(somme / n);
            const int sortant = x - r, entrant = x + r + 1;
            somme += src[y * w + (entrant >= w ? w - 1 : entrant)] - src[y * w + (sortant < 0 ? 0 : sortant)];
        }
    }
    for (int x = 0; x < w; x++) {
        int somme = 0;
        for (int y = -r; y <= r; y++) somme += tmp[(y < 0 ? 0 : y >= h ? h - 1 : y) * w + x];
        for (int y = 0; y < h; y++) {
            dst[y * w + x] = (BYTE)(somme / n);
            const int sortant = y - r, entrant = y + r + 1;
            somme += tmp[(entrant >= h ? h - 1 : entrant) * w + x] - tmp[(sortant < 0 ? 0 : sortant) * w + x];
        }
    }
}

static void Peindre(DWORD *dst, int pitch, int w, int h)
{
    Verdict v;
    EnterCriticalSection(&g_csVerdict); v = g_verdict; LeaveCriticalSection(&g_csVerdict);
    if (g_mode == 1) { WCHAR *sep = wcsstr(v.ligne2, L"   "); if (sep) *sep = 0; }
    memset(dib_bits, 0, (size_t)w * h * 4);
    SetBkMode(dib_dc, TRANSPARENT);
    SetTextCharacterExtra(dib_dc, -(h / 60));
    const int marge = h / 20, haut1 = h * 60 / 100;
    Texte(police1, marge, 0, w - marge, haut1, v.ligne1);
    SetTextCharacterExtra(dib_dc, h / 90);
    Texte(police2, marge + h / 20, haut1 - h / 12, w - marge, h - haut1 + h / 12, v.ligne2);
    GdiFlush();
    for (int i = 0; i < w * h; i++) g_masque[i] = (BYTE)(dib_bits[i] & 0xFF);

    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            int m = 0;
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++) {
                    const int yy = y + dy, xx = x + dx;
                    if (yy >= 0 && yy < h && xx >= 0 && xx < w && g_masque[yy * w + xx] > m) m = g_masque[yy * w + xx];
                }
            g_flou[y * w + x] = (BYTE)m;
        }
    for (int i = 0; i < w * h; i++) dib_bits[i] = g_flou[i];
    Flouter(g_masque, g_flou, g_tampon, w, h, h / 16 > 1 ? h / 16 : 1);
    Flouter(g_flou, g_flou, g_tampon, w, h, h / 20 > 1 ? h / 20 : 1);
    const COLORREF c = COULEURS[v.couleur];
    const int cr = GetRValue(c), cg = GetGValue(c), cb = GetBValue(c);

    int yh = haut1, yb = 0;
    for (int y = 0; y < haut1; y++)
        for (int x = 0; x < w; x++) if (g_masque[y * w + x] > 64) { if (y < yh) yh = y; if (y > yb) yb = y; break; }
    if (yb <= yh) { yh = 0; yb = haut1 - 1; }
    for (int y = 0; y < h; y++) {
        const BOOL titre = y < haut1;

        const int t100 = y <= yh ? 0 : y >= yb ? 100 : (y - yh) * 100 / (yb - yh);
        const int fh = 125 - t100 * 70 / 100;
        int kr = cr * fh / 100, kg = cg * fh / 100, kb = cb * fh / 100;
        if (kr > 255) kr = 255;
        if (kg > 255) kg = 255;
        if (kb > 255) kb = 255;
        if (!titre) kr = kg = kb = 245;
        const int lr = 255 - (255 - cr) / 3, lg = 255 - (255 - cg) / 3, lb = 255 - (255 - cb) / 3;
        const int hr = titre ? cr : 90, hg = titre ? cg : 90, hb = titre ? cb : 90, force = titre ? 6 : 2;
        DWORD *ligne = (DWORD *)((BYTE *)dst + y * pitch);
        for (int x = 0; x < w; x++) {
            const int m = g_masque[y * w + x], d = titre ? (int)(dib_bits[y * w + x] & 0xFF) : m;
            int g = g_flou[y * w + x] * force; if (g > 255) g = 255;

            const int lis = d > m ? d - m : 0;
            int a = m > g ? m : g; if (lis > a) a = lis;
            if (!a) { ligne[x] = 0; continue; }
            const int gm = g * (255 - (m > lis ? m : lis)) / 255, lm = lis * (255 - m) / 255;
            const int tot = m + gm + lm ? m + gm + lm : 1;
            const int r = (kr * m + hr * gm + lr * lm) / tot, gg = (kg * m + hg * gm + lg * lm) / tot,
                      bb = (kb * m + hb * gm + lb * lm) / tot;
            ligne[x] = (DWORD)a << 24 | (DWORD)r << 16 | (DWORD)gg << 8 | (DWORD)bb;
        }
    }
}
static BOOL MettreAJourTexture(IDirect3DDevice9 *dev, int w, int h)
{
    if (w != dib_w || h != dib_h) {
        if (dib) { DeleteObject(dib); dib = NULL; }
        free(g_masque); free(g_flou); free(g_tampon);
        g_masque = malloc((size_t)w * h); g_flou = malloc((size_t)w * h); g_tampon = malloc((size_t)w * h);
        if (!g_masque || !g_flou || !g_tampon) { dib_w = dib_h = 0; return FALSE; }
        if (!dib_dc) dib_dc = CreateCompatibleDC(NULL);
        BITMAPINFO bi = {{sizeof(BITMAPINFOHEADER), w, -h, 1, 32, BI_RGB}};
        dib = CreateDIBSection(dib_dc, &bi, DIB_RGB_COLORS, (void **)&dib_bits, NULL, 0);
        if (!dib) return FALSE;
        SelectObject(dib_dc, dib); dib_w = w; dib_h = h;
        if (police1) DeleteObject(police1);
        if (police2) DeleteObject(police2);

        police1 = CreateFontW(-h * 44 / 100, h * 44 / 100 * 78 / 100, 0, 0, FW_BLACK, TRUE, 0, 0, DEFAULT_CHARSET, 0, 0,
                              ANTIALIASED_QUALITY, 0, L"Bahnschrift");
        police2 = CreateFontW(-h * 32 / 100, h * 32 / 100 * 60 / 100, 0, 0, FW_BOLD, FALSE, 0, 0, DEFAULT_CHARSET, 0, 0,
                              ANTIALIASED_QUALITY, 0, L"Bahnschrift");
    }
    if (!g_tex || tex_w != w || tex_h != h) {
        if (g_tex) { IDirect3DTexture9_Release(g_tex); g_tex = NULL; }
        if (FAILED(IDirect3DDevice9_CreateTexture(dev, w, h, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &g_tex, NULL))) return FALSE;
        tex_w = w; tex_h = h;
    }
    D3DLOCKED_RECT lr;
    if (FAILED(IDirect3DTexture9_LockRect(g_tex, 0, &lr, NULL, 0))) return FALSE;
    Peindre((DWORD *)lr.pBits, lr.Pitch, w, h);
    IDirect3DTexture9_UnlockRect(g_tex, 0);
    return TRUE;
}

static void DessinerTexture(IDirect3DDevice9 *dev, IDirect3DSurface9 *bb, const D3DSURFACE_DESC *sd, IDirect3DTexture9 *tex,
                            float x0, float y0, float x1, float y1, DWORD alpha)
{
    IDirect3DStateBlock9 *sb = NULL;
    if (FAILED(IDirect3DDevice9_CreateStateBlock(dev, D3DSBT_ALL, &sb))) return;
    IDirect3DSurface9 *rt = NULL; IDirect3DDevice9_GetRenderTarget(dev, 0, &rt);
    IDirect3DDevice9_SetRenderTarget(dev, 0, bb);
    D3DVIEWPORT9 vp = {0, 0, sd->Width, sd->Height, 0, 1}; IDirect3DDevice9_SetViewport(dev, &vp);
    IDirect3DDevice9_BeginScene(dev);
    IDirect3DDevice9_SetVertexShader(dev, NULL); IDirect3DDevice9_SetPixelShader(dev, NULL);
    IDirect3DDevice9_SetFVF(dev, D3DFVF_XYZRHW | D3DFVF_TEX1);
    IDirect3DDevice9_SetTexture(dev, 0, (IDirect3DBaseTexture9 *)tex);
    for (int i = 1; i < 8; i++) IDirect3DDevice9_SetTexture(dev, i, NULL);
    IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_ALPHAARG2, D3DTA_TFACTOR);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_TEXTUREFACTOR, alpha << 24 | 0xFFFFFF);
    IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_TEXCOORDINDEX, 0);
    IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
    IDirect3DDevice9_SetTextureStageState(dev, 1, D3DTSS_COLOROP, D3DTOP_DISABLE);
    IDirect3DDevice9_SetTextureStageState(dev, 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
    IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
    IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
    IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
    IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
    IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_SRGBTEXTURE, 0);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_SRGBWRITEENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_ALPHABLENDENABLE, TRUE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_BLENDOP, D3DBLENDOP_ADD);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_ALPHATESTENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_ZENABLE, D3DZB_FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_ZWRITEENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_STENCILENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_SCISSORTESTENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_CULLMODE, D3DCULL_NONE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_FOGENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_LIGHTING, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_COLORWRITEENABLE, 0xf);
    struct { float x, y, z, w, u, v; } q[4] = {{x0, y0, 0, 1, 0, 0}, {x1, y0, 0, 1, 1, 0}, {x0, y1, 0, 1, 0, 1}, {x1, y1, 0, 1, 1, 1}};
    IDirect3DDevice9_DrawPrimitiveUP(dev, D3DPT_TRIANGLESTRIP, 2, q, sizeof(q[0]));
    IDirect3DDevice9_EndScene(dev);
    IDirect3DDevice9_SetRenderTarget(dev, 0, rt); if (rt) IDirect3DSurface9_Release(rt);
    IDirect3DStateBlock9_Apply(sb); IDirect3DStateBlock9_Release(sb);
}
static void Dessiner(IDirect3DDevice9 *dev)
{
    DWORD t;
    EnterCriticalSection(&g_csVerdict); t = g_verdict.t; LeaveCriticalSection(&g_csVerdict);
    if (t == 0 || GetTickCount() - t > DUREE_AFFICHAGE || !Actif()) return;
    IDirect3DSurface9 *bb = NULL;
    if (FAILED(IDirect3DDevice9_GetBackBuffer(dev, 0, 0, D3DBACKBUFFER_TYPE_MONO, &bb)) || !bb) return;
    D3DSURFACE_DESC sd; IDirect3DSurface9_GetDesc(bb, &sd);
    float gh = (float)sd.Height, gw = gh * 16 / 9, gx = 0, gy = 0;
    if (gw > sd.Width) { gw = (float)sd.Width; gh = gw * 9 / 16; gy = (sd.Height - gh) / 2; } else gx = (sd.Width - gw) / 2;
    const float e = gh / 1080;
    const int w = (int)(panel_w * e), h = (int)(panel_h * e);
    if (w < 16 || h < 16) { IDirect3DSurface9_Release(bb); return; }
    const LONG version = g_versionVerdict;
    if (version != g_versionDessinee || !g_tex || tex_w != w || tex_h != h) {
        if (!MettreAJourTexture(dev, w, h)) { IDirect3DSurface9_Release(bb); return; }
        g_versionDessinee = version;
    }

    const DWORD age = GetTickCount() - t;

    const float p = age < ENTREE_MS ? (float)age / ENTREE_MS
                  : age > DUREE_AFFICHAGE - ENTREE_MS ? (float)(DUREE_AFFICHAGE - age) / ENTREE_MS : 1.0f;
    const float sens = age < ENTREE_MS ? 1.0f : -1.0f;
    const float x0 = (float)(int)(gx + panel_x * e + sens * (1 - p) * w * 0.35f) - 0.5f, y0 = (float)(int)(gy + panel_y * e) - 0.5f,
                x1 = x0 + w * (1 + (1 - p) * 0.45f), y1 = y0 + h;
    DessinerTexture(dev, bb, &sd, g_tex, x0, y0, x1, y1, (DWORD)(255 * (0.35f + 0.65f * p)));
    IDirect3DSurface9_Release(bb);
}

#define LISTE_X 1250
#define LISTE_Y 150
#define LISTE_W 620
#define LIGNE_H 34
#define LIGNES_VISIBLES 12
static IDirect3DTexture9 *g_texListe; static int texl_w, texl_h;
static LONG g_versionListeDessinee = -1;
static HDC dcl; static HBITMAP dibl; static DWORD *bitsl; static int dibl_w, dibl_h;
static HFONT policeTitre, policeLigne;

typedef struct { int x, y, d; char b; } Icone;
#define NB_ICONES 96
static Icone g_icones[NB_ICONES];
static int g_nbIcones;
static HFONT policeIcone;
static HDC g_dcN; static DWORD *g_bitsN;
static COLORREF CouleurBouton(char b)
{
    return b == 'P' ? RGB(236, 150, 40) : b == 'K' ? RGB(210, 45, 60) : RGB(60, 170, 80);
}
static void Fleche(int cx, int cy, int t, int dir)
{
    static const signed char dx[10] = {0, -1, 0, 1, -1, 0, 1, -1, 0, 1}, dy[10] = {0, 1, 1, 1, 0, 0, 0, -1, -1, -1};
    float ux = dx[dir], uy = dy[dir];
    const float n = sqrtf(ux * ux + uy * uy);
    if (n == 0) return;
    ux /= n; uy /= n;
    const float vx = -uy, vy = ux, L = t * 0.42f, e = t * 0.09f, tw = t * 0.26f, tl = t * 0.30f;
    const float pts[7][2] = {{-L, -e}, {L - tl, -e}, {L - tl, -tw}, {L, 0}, {L - tl, tw}, {L - tl, e}, {-L, e}};
    POINT p[7];
    for (int k = 0; k < 7; k++) {
        p[k].x = (LONG)(cx + pts[k][0] * ux + pts[k][1] * vx + 0.5f);
        p[k].y = (LONG)(cy + pts[k][0] * uy + pts[k][1] * vy + 0.5f);
    }
    Polygon(g_dcN, p, 7);
}

static void DemiCercle(int cx, int cy, int t, BOOL arriere)
{
    const float R = t * 0.32f, e = t * 0.09f, pi = 3.14159265f;
    const float a0 = arriere ? 0 : pi, a1 = arriere ? pi : 0, pasA = (a1 - a0) / 10;
    for (int k = 0; k < 9; k++) {
        const float u = a0 + pasA * k, v = a0 + pasA * (k + 1);
        POINT p[4] = {{(LONG)(cx + (R - e) * cosf(u)), (LONG)(cy + (R - e) * sinf(u))},
                      {(LONG)(cx + (R + e) * cosf(u)), (LONG)(cy + (R + e) * sinf(u))},
                      {(LONG)(cx + (R + e) * cosf(v)), (LONG)(cy + (R + e) * sinf(v))},
                      {(LONG)(cx + (R - e) * cosf(v)), (LONG)(cy + (R - e) * sinf(v))}};
        Polygon(g_dcN, p, 4);
    }
    const float ax = cx + R * cosf(a1), ay = cy + R * sinf(a1) - t * 0.04f, tw = t * 0.22f, tl = t * 0.26f;
    POINT tete[3] = {{(LONG)(ax - tw), (LONG)(ay + tl * 0.2f)}, {(LONG)(ax + tw), (LONG)(ay + tl * 0.2f)}, {(LONG)ax, (LONG)(ay - tl)}};
    Polygon(g_dcN, tete, 3);
}

static int DessinerNotation(const char *src, int x, int y, int rh, BOOL dessiner)
{
    const int t = rh * 62 / 100, pas = t + rh / 12;
    int x0 = x;
    for (int i = 0; src[i]; i++) {
        const char c = src[i] == 'y' ? '2' : src[i];
        if (c >= '1' && c <= '9') {
            if (c != '5') { if (dessiner) Fleche(x + t / 2, y + rh / 2, t, c - '0'); x += pas; }
        } else if (c == 'h' || c == 'Y' || (c == 'H' && src[i + 1] != '+' && (i == 0 || src[i - 1] != '+'))) {

            if (dessiner) DemiCercle(x + t / 2, y + rh / 2, t, c == 'h');
            x += pas;
        } else if (c == 'P' || c == 'K' || c == 'H' || c == 'F' || c == 'N') {
            const char b[2] = {c == 'N' ? 'P' : c == 'F' ? 'H' : c, 'H'};
            for (int k = 0; k < (c == 'N' ? 2 : 1); k++) {
                if (c == 'N' && k == 1) {
                    SIZE sz; GetTextExtentPoint32W(g_dcN, L"+", 1, &sz);
                    if (dessiner) TextOutW(g_dcN, x, y + (rh - sz.cy) / 2, L"+", 1);
                    x += sz.cx + rh / 16;
                }
                if (dessiner && g_nbIcones < NB_ICONES) {
                    Icone *ic = &g_icones[g_nbIcones++];
                    ic->x = x; ic->y = y + (rh - t) / 2; ic->d = t; ic->b = b[k];
                }
                x += pas;
            }
        } else if (c == ' ') {
            x += rh / 5;
        } else if (c > 0x20 && c < 0x7F) {
            const WCHAR wc = (WCHAR)c;
            SIZE sz; GetTextExtentPoint32W(g_dcN, &wc, 1, &sz);
            if (dessiner) TextOutW(g_dcN, x, y + (rh - sz.cy) / 2, &wc, 1);
            x += sz.cx + rh / 16;
        }
    }
    return x - x0;
}

static void PeindreIcones(DWORD *dst, int pitch, int w, int h, HFONT police)
{
    SelectObject(g_dcN, police);
    SetTextColor(g_dcN, RGB(255, 255, 255));
    for (int n = 0; n < g_nbIcones; n++) {
        const Icone *ic = &g_icones[n];
        if (ic->x < 0 || ic->y < 0 || ic->x + ic->d > w || ic->y + ic->d > h) continue;
        for (int y = ic->y; y < ic->y + ic->d; y++) memset(&g_bitsN[y * w + ic->x], 0, (size_t)ic->d * 4);
        const WCHAR lettre = (WCHAR)ic->b;
        RECT r = {ic->x, ic->y, ic->x + ic->d, ic->y + ic->d};
        DrawTextW(g_dcN, &lettre, 1, &r, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
        GdiFlush();
        const COLORREF col = CouleurBouton(ic->b);
        const float rr = ic->d / 2.0f, cx = ic->x + rr, cy = ic->y + rr;
        for (int y = ic->y; y < ic->y + ic->d; y++) {
            DWORD *ligne = (DWORD *)((BYTE *)dst + y * pitch);
            for (int x = ic->x; x < ic->x + ic->d; x++) {
                const float ddx = x + 0.5f - cx, ddy = y + 0.5f - cy, dist = sqrtf(ddx * ddx + ddy * ddy);
                float a = rr - dist; a = a < 0 ? 0 : a > 1 ? 1 : a;
                if (a <= 0) continue;
                float bord = dist - (rr - ic->d * 0.09f); bord = bord < 0 ? 0 : bord > 1 ? 1 : bord;
                int r0 = GetRValue(col), g0 = GetGValue(col), b0 = GetBValue(col);
                r0 += (int)((255 - r0) * bord * 0.75f); g0 += (int)((255 - g0) * bord * 0.75f); b0 += (int)((255 - b0) * bord * 0.75f);
                const DWORD px = g_bitsN[y * w + x];
                int cov = px & 0xFF; if (((px >> 8) & 0xFF) > cov) cov = (px >> 8) & 0xFF;
                r0 += (255 - r0) * cov / 255; g0 += (255 - g0) * cov / 255; b0 += (255 - b0) * cov / 255;
                const DWORD fond = ligne[x];
                const int fr = (fond >> 16) & 0xFF, fg = (fond >> 8) & 0xFF, fb = fond & 0xFF, fa = fond >> 24;
                const int R = (int)(fr + (r0 - fr) * a), G = (int)(fg + (g0 - fg) * a), B = (int)(fb + (b0 - fb) * a);
                const int A = (int)(fa + (255 - fa) * a);
                ligne[x] = (DWORD)A << 24 | (DWORD)R << 16 | (DWORD)G << 8 | (DWORD)B;
            }
        }
    }
}

static int LignesListe(int *premiere)
{
    if (!g_menuOuvert) { *premiere = g_choix; return g_choix < g_nbCoupsListe ? 1 : 0; }
    int p = g_curseur - LIGNES_VISIBLES / 2;
    if (p > g_nbCoupsListe - LIGNES_VISIBLES) p = g_nbCoupsListe - LIGNES_VISIBLES;
    if (p < 0) p = 0;
    *premiere = p;
    return g_nbCoupsListe - p < LIGNES_VISIBLES ? g_nbCoupsListe - p : LIGNES_VISIBLES;
}
static void PeindreListe(DWORD *dst, int pitch, int w, int h, int nl, int premiere)
{
    const int rh = h / (nl + 2);
    memset(bitsl, 0, (size_t)w * h * 4);
    g_nbIcones = 0; g_dcN = dcl; g_bitsN = bitsl;
    SelectObject(dcl, GetStockObject(WHITE_BRUSH));
    SelectObject(dcl, GetStockObject(WHITE_PEN));
    SetBkMode(dcl, TRANSPARENT);
    SetTextColor(dcl, RGB(255, 255, 255));
    WCHAR t[128];
    RECT r = {rh / 3, 0, w - rh / 3, rh};
    SelectObject(dcl, policeTitre);
    if (g_menuOuvert) lstrcpynW(t, L"PUNISH CHALLENGE", 128);
    else swprintf(t, 128, L"CHALLENGE %d/%d", g_choix + 1, g_nbCoupsListe);
    DrawTextW(dcl, t, -1, &r, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    swprintf(t, 128, L"%d/%d cleared", NbFaits(), g_nbCoupsListe);
    DrawTextW(dcl, t, -1, &r, DT_RIGHT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    SelectObject(dcl, policeLigne);
    COLORREF couleurs[LIGNES_VISIBLES + 2];
    BOOL surligne[LIGNES_VISIBLES + 2] = {0};
    couleurs[0] = RGB(255, 255, 255);
    for (int l = 0; l < nl; l++) {
        const int k = premiere + l;
        const CoupListe *c = &g_coupsListe[k];
        const int y = (l + 1) * rh;
        RECT boite = {rh / 3, y + rh / 4, rh / 3 + rh / 2, y + rh * 3 / 4};
        if (c->fait) FillRect(dcl, &boite, GetStockObject(WHITE_BRUSH)); else FrameRect(dcl, &boite, GetStockObject(WHITE_BRUSH));
        RECT rt = {rh, y, w - rh / 3, y + rh};
        DessinerNotation(g_fiches[c->fiche].txt, rh, y, rh, TRUE);
        if (c->injouable) lstrcpynW(t, c->injouable == 1 ? L"can't be played" : L"unblockable", 128);
        else if (c->dis == INITIATIVE) lstrcpynW(t, L"+   frame trap: block", 128);
        else if (c->dis != INCONNU && c->dis < 0) swprintf(t, 128, L"+%d   frame trap: block", -c->dis);
        else if (Impossible(c)) swprintf(t, 128, c->dis == 0 ? L"0   safe: block" : L"-%d   no guaranteed throw: block", c->dis);
        else if (c->meilleure > 0) {
            WCHAR g[16], d[16];
            swprintf(g, 16, L"-%d   ", c->dis); swprintf(d, 16, L"   i%d", c->meilleure - 1);
            SIZE sg, sd; GetTextExtentPoint32W(dcl, g, lstrlenW(g), &sg); GetTextExtentPoint32W(dcl, d, lstrlenW(d), &sd);
            const int wn = DessinerNotation(c->netMeilleure, 0, y, rh, FALSE);
            int x = rt.right - sd.cx - wn - sg.cx;
            TextOutW(dcl, x, y + (rh - sg.cy) / 2, g, lstrlenW(g)); x += sg.cx;
            x += DessinerNotation(c->netMeilleure, x, y, rh, TRUE);
            TextOutW(dcl, x, y + (rh - sd.cy) / 2, d, lstrlenW(d));
            t[0] = 0;
        }
        else swprintf(t, 128, L"-%d", c->dis != INCONNU ? c->dis : c->delta);
        if (t[0]) DrawTextW(dcl, t, -1, &rt, DT_RIGHT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
        couleurs[l + 1] = c->fait ? COULEURS[DORE]
                        : (ExerciceActif() || !g_menuOuvert) && k == g_choix ? RGB(130, 195, 255) : RGB(235, 235, 235);
        surligne[l + 1] = g_menuOuvert && k == g_curseur;
    }
    RECT ra = {rh / 3, (nl + 1) * rh, w - rh / 3, (nl + 2) * rh};
    DrawTextW(dcl, g_menuOuvert ? L"Cross/A: start   Circle/B: back   Triangle/Y: stop" : L"F8: list", -1, &ra,
              DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    couleurs[nl + 1] = RGB(170, 170, 170);
    GdiFlush();
    for (int y = 0; y < h; y++) {
        int l = y / rh; if (l > nl + 1) l = nl + 1;
        const COLORREF c = couleurs[l];
        const int fr = l == 0 ? 0 : surligne[l] ? 90 : 14, fg = l == 0 ? 0 : surligne[l] ? 72 : 16, fb = l == 0 ? 0 : surligne[l] ? 18 : 22;
        const int fa = l == 0 ? 0xD0 : surligne[l] ? 0xE0 : 0xA8;
        DWORD *ligne = (DWORD *)((BYTE *)dst + y * pitch);
        for (int x = 0; x < w; x++) {
            const DWORD px = bitsl[y * w + x];
            int cov = px & 0xFF; if (((px >> 8) & 0xFF) > cov) cov = (px >> 8) & 0xFF; if (((px >> 16) & 0xFF) > cov) cov = (px >> 16) & 0xFF;
            const int rr = (fr * (255 - cov) + GetRValue(c) * cov) / 255, gg = (fg * (255 - cov) + GetGValue(c) * cov) / 255,
                      bb = (fb * (255 - cov) + GetBValue(c) * cov) / 255, aa = fa + (255 - fa) * cov / 255;
            ligne[x] = (DWORD)aa << 24 | (DWORD)rr << 16 | (DWORD)gg << 8 | (DWORD)bb;
        }
    }
    PeindreIcones(dst, pitch, w, h, policeIcone);
}
static BOOL TextureListe(IDirect3DDevice9 *dev, int w, int h, int nl, int premiere)
{
    if (w != dibl_w || h != dibl_h) {
        if (dibl) { DeleteObject(dibl); dibl = NULL; }
        if (!dcl) dcl = CreateCompatibleDC(NULL);
        BITMAPINFO bi = {{sizeof(BITMAPINFOHEADER), w, -h, 1, 32, BI_RGB}};
        dibl = CreateDIBSection(dcl, &bi, DIB_RGB_COLORS, (void **)&bitsl, NULL, 0);
        if (!dibl) { dibl_w = dibl_h = 0; return FALSE; }
        SelectObject(dcl, dibl); dibl_w = w; dibl_h = h;
        const int rh = h / (nl + 2);
        if (policeTitre) DeleteObject(policeTitre);
        if (policeLigne) DeleteObject(policeLigne);
        policeTitre = CreateFontW(-rh * 70 / 100, 0, 0, 0, FW_BLACK, TRUE, 0, 0, DEFAULT_CHARSET, 0, 0, ANTIALIASED_QUALITY, 0, L"Bahnschrift");
        policeLigne = CreateFontW(-rh * 58 / 100, 0, 0, 0, FW_SEMIBOLD, FALSE, 0, 0, DEFAULT_CHARSET, 0, 0, ANTIALIASED_QUALITY, 0, L"Bahnschrift");
        if (policeIcone) DeleteObject(policeIcone);
        policeIcone = CreateFontW(-rh * 44 / 100, 0, 0, 0, FW_BOLD, FALSE, 0, 0, DEFAULT_CHARSET, 0, 0, ANTIALIASED_QUALITY, 0, L"Bahnschrift");
    }
    if (!g_texListe || texl_w != w || texl_h != h) {
        if (g_texListe) { IDirect3DTexture9_Release(g_texListe); g_texListe = NULL; }
        if (FAILED(IDirect3DDevice9_CreateTexture(dev, w, h, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &g_texListe, NULL))) return FALSE;
        texl_w = w; texl_h = h;
    }
    D3DLOCKED_RECT lr;
    if (FAILED(IDirect3DTexture9_LockRect(g_texListe, 0, &lr, NULL, 0))) return FALSE;
    PeindreListe((DWORD *)lr.pBits, lr.Pitch, w, h, nl, premiere);
    IDirect3DTexture9_UnlockRect(g_texListe, 0);
    return TRUE;
}
static void DessinerListe(IDirect3DDevice9 *dev)
{
    if (!g_nbCoupsListe || !DefiPossible() || !g_menuOuvert) return;
    int premiere;
    const int nl = LignesListe(&premiere);
    if (!nl) return;
    IDirect3DSurface9 *bb = NULL;
    if (FAILED(IDirect3DDevice9_GetBackBuffer(dev, 0, 0, D3DBACKBUFFER_TYPE_MONO, &bb)) || !bb) return;
    D3DSURFACE_DESC sd; IDirect3DSurface9_GetDesc(bb, &sd);
    float gh = (float)sd.Height, gw = gh * 16 / 9, gx = 0, gy = 0;
    if (gw > sd.Width) { gw = (float)sd.Width; gh = gw * 9 / 16; gy = (sd.Height - gh) / 2; } else gx = (sd.Width - gw) / 2;
    const float e = gh / 1080;
    const int w = (int)(LISTE_W * e), h = (int)(LIGNE_H * (nl + 2) * e);
    if (w < 16 || h < 16) { IDirect3DSurface9_Release(bb); return; }
    const LONG version = g_versionListe + (LONG)g_menuOuvert * 0x10000 + premiere * 0x100;
    if (version != g_versionListeDessinee || !g_texListe || texl_w != w || texl_h != h) {
        if (!TextureListe(dev, w, h, nl, premiere)) { IDirect3DSurface9_Release(bb); return; }
        g_versionListeDessinee = version;
    }
    const float x0 = (float)(int)(gx + LISTE_X * e) - 0.5f, y0 = (float)(int)(gy + LISTE_Y * e) - 0.5f;
    DessinerTexture(dev, bb, &sd, g_texListe, x0, y0, x0 + w, y0 + h, 255);
    IDirect3DSurface9_Release(bb);
}

#define BARRE_Y 872
#define BARRE_H 58
#define GOOD_W 640
#define GOOD_H 170
#define DUREE_GOOD 1300
static IDirect3DTexture9 *g_texBarre, *g_texGood; static int texb_w, texb_h, texg_w, texg_h;
static LONG g_cleBarre = -1;
static HDC dcb; static HBITMAP dibb; static DWORD *bitsb; static int dibb_w, dibb_h;
static HFONT policeBarre, policeBarreIcone, policeCleared, policeGood;
static BOOL g_goodPeint; static int good_w;
static BYTE *g_dilate, *g_dilateTmp; static int dilate_n;

static BOOL SurfaceBarre(int w, int h)
{
    if (w == dibb_w && h == dibb_h) return TRUE;
    if (dibb) { DeleteObject(dibb); dibb = NULL; }
    if (!dcb) dcb = CreateCompatibleDC(NULL);
    BITMAPINFO bi = {{sizeof(BITMAPINFOHEADER), w, -h, 1, 32, BI_RGB}};
    dibb = CreateDIBSection(dcb, &bi, DIB_RGB_COLORS, (void **)&bitsb, NULL, 0);
    if (!dibb) { dibb_w = dibb_h = 0; return FALSE; }
    SelectObject(dcb, dibb); dibb_w = w; dibb_h = h;
    return TRUE;
}
static void PeindreBarre(DWORD *dst, int pitch, int w, int h, const CoupListe *c, BOOL cleared)
{
    memset(bitsb, 0, (size_t)w * h * 4);
    g_nbIcones = 0; g_dcN = dcb; g_bitsN = bitsb;
    SelectObject(dcb, GetStockObject(WHITE_BRUSH)); SelectObject(dcb, GetStockObject(WHITE_PEN));
    SetBkMode(dcb, TRANSPARENT); SetTextColor(dcb, RGB(255, 255, 255));
    SelectObject(dcb, policeBarre);
    const int u = h, x0 = w * 128 / 1920;
    int x = x0 + DessinerNotation(g_fiches[c->fiche].txt, x0, 0, u, TRUE);
    WCHAR t[64];
    swprintf(t, 64, L"   (%d/%d)", g_choix + 1, g_nbCoupsListe);
    SIZE sz; GetTextExtentPoint32W(dcb, t, lstrlenW(t), &sz);
    TextOutW(dcb, x, (u - sz.cy) / 2, t, lstrlenW(t));
    const int xc = w * 693 / 1920, cc = u * 52 / 100, yc = (u - cc) / 2;
    RECT boite = {xc, yc, xc + cc, yc + cc};
    FrameRect(dcb, &boite, GetStockObject(WHITE_BRUSH));
    if (c->fait) {
        POINT v[6] = {{xc + cc / 5, yc + cc / 2}, {xc + cc * 2 / 5, yc + cc * 3 / 4}, {xc + cc * 9 / 10, yc + cc / 6},
                      {xc + cc * 9 / 10, yc + cc / 3}, {xc + cc * 2 / 5, yc + cc * 9 / 10}, {xc + cc / 5, yc + cc * 2 / 3}};
        Polygon(dcb, v, 6);
    }
    const int xi = xc + cc + u / 2;
    if (cleared) {
        SelectObject(dcb, policeCleared);
        GetTextExtentPoint32W(dcb, L"CLEARED!", 8, &sz);
        TextOutW(dcb, xi, (u - sz.cy) / 2, L"CLEARED!", 8);
    } else {
        const WCHAR *consigne = Impossible(c) ? L"BLOCK IT, DON'T PRESS!" : L"PUNISH IT WITH THE BEST THROW!";
        GetTextExtentPoint32W(dcb, consigne, lstrlenW(consigne), &sz);
        TextOutW(dcb, xi, (u - sz.cy) / 2, consigne, lstrlenW(consigne));
    }
    GdiFlush();
    const COLORREF vert = RGB(110, 210, 80), rouge = RGB(220, 30, 40), blanc = RGB(250, 250, 250);
    for (int y = 0; y < h; y++) {
        DWORD *ligne = (DWORD *)((BYTE *)dst + y * pitch);
        for (int px = 0; px < w; px++) {
            const DWORD m = bitsb[y * w + px];
            int cov = m & 0xFF; if (((m >> 8) & 0xFF) > cov) cov = (m >> 8) & 0xFF; if (((m >> 16) & 0xFF) > cov) cov = (m >> 16) & 0xFF;
            const BOOL dansCase = px >= xc && px < xc + cc;
            const COLORREF col = dansCase ? (c->fait && !(px == xc || px == xc + cc - 1 || y == yc || y == yc + cc - 1) ? rouge : blanc)
                               : px >= xi && cleared ? blanc : vert;
            const int fa = 0x9C;
            const int rr = GetRValue(col) * cov / 255, gg = GetGValue(col) * cov / 255, bb = GetBValue(col) * cov / 255;
            const int aa = fa + (255 - fa) * cov / 255;
            ligne[px] = (DWORD)aa << 24 | (DWORD)rr << 16 | (DWORD)gg << 8 | (DWORD)bb;
        }
    }
    PeindreIcones(dst, pitch, w, h, policeBarreIcone);
}

#define ETAPES_X 143
#define ETAPES_Y 470
#define ETAPES_W 600
#define ETAPE_H 54
static IDirect3DTexture9 *g_texEtapes; static int texe_w, texe_h;
static LONG g_cleEtapes = -1;
static HFONT policeEtape, policeEtapeIcone;
static void Puce(int cx, int cy, int r, BOOL pleine)
{
    SelectObject(dcb, GetStockObject(WHITE_BRUSH)); SelectObject(dcb, GetStockObject(WHITE_PEN));
    Ellipse(dcb, cx - r, cy - r, cx + r, cy + r);
    if (!pleine) {
        SelectObject(dcb, GetStockObject(BLACK_BRUSH)); SelectObject(dcb, GetStockObject(BLACK_PEN));
        Ellipse(dcb, cx - r + r / 3 + 1, cy - r + r / 3 + 1, cx + r - r / 3 - 1, cy + r - r / 3 - 1);
        SelectObject(dcb, GetStockObject(WHITE_BRUSH)); SelectObject(dcb, GetStockObject(WHITE_PEN));
    }
}
static void TexteDroite(const WCHAR *t, int droite, int y, int u)
{
    SIZE sz; GetTextExtentPoint32W(dcb, t, lstrlenW(t), &sz);
    TextOutW(dcb, droite - sz.cx, y + (u - sz.cy) / 2, t, lstrlenW(t));
}
static void TexteGauche(const WCHAR *t, int x, int y, int u)
{
    SIZE sz; GetTextExtentPoint32W(dcb, t, lstrlenW(t), &sz);
    TextOutW(dcb, x, y + (u - sz.cy) / 2, t, lstrlenW(t));
}

static void PeindreEtapes(DWORD *dst, int pitch, int w, int h, const CoupListe *c, BOOL e1, BOOL e2)
{
    const int u = h / 2;
    memset(bitsb, 0, (size_t)w * h * 4);
    g_nbIcones = 0; g_dcN = dcb; g_bitsN = bitsb;
    SetBkMode(dcb, TRANSPARENT); SetTextColor(dcb, RGB(255, 255, 255));
    SelectObject(dcb, policeEtape);
    const BOOL mesure = c->dis != INCONNU;
    const int dis = mesure ? c->dis : 0;
    const int xg = u * 3 / 2, droite = w - u / 3;
    WCHAR t[48];

    Puce(u * 3 / 4, u / 2, u * 22 / 100, e1);
    SelectObject(dcb, GetStockObject(WHITE_BRUSH)); SelectObject(dcb, GetStockObject(WHITE_PEN));
    int x = xg;
    TexteGauche(L"BLOCK ", x, 0, u);
    { SIZE sz; GetTextExtentPoint32W(dcb, L"BLOCK ", 6, &sz); x += sz.cx; }
    DessinerNotation(g_fiches[c->fiche].txt, x, 0, u, TRUE);
    if (!mesure) lstrcpynW(t, L"?", 48);
    else if (dis == INITIATIVE) lstrcpynW(t, L"+", 48);
    else if (dis < 0) swprintf(t, 48, L"+%d", -dis);
    else swprintf(t, 48, dis ? L"-%d" : L"%d", dis);
    TexteDroite(t, droite, 0, u);

    const BOOL sur = c->meilleure >= 0;
    if (sur) {
        Puce(u * 3 / 4, u + u / 2, u * 22 / 100, e2);
        SelectObject(dcb, GetStockObject(WHITE_BRUSH)); SelectObject(dcb, GetStockObject(WHITE_PEN));
        if (c->meilleure == 0)
            TexteGauche(dis < 0 ? L"FRAME TRAP: DON'T PRESS" : dis == 0 ? L"SAFE: DON'T PRESS" : L"NO GUARANTEED THROW: DON'T PRESS", xg, u, u);
        else if (c->netMeilleure[0]) {
            TexteGauche(L"PUNISH ", xg, u, u);
            SIZE sz; GetTextExtentPoint32W(dcb, L"PUNISH ", 7, &sz);
            DessinerNotation(c->netMeilleure, xg + sz.cx, u, u, TRUE);
            swprintf(t, 48, L"i%d", c->meilleure - 1);
            TexteDroite(t, droite, u, u);
        }
    }
    GdiFlush();
    for (int y = 0; y < h; y++) {
        DWORD *ligne = (DWORD *)((BYTE *)dst + y * pitch);
        const BOOL bord = (y % u) == 0 || (y % u) == u - 1;
        if (y >= u && !sur) { memset(ligne, 0, (size_t)w * 4); continue; }
        for (int px = 0; px < w; px++) {
            const DWORD mm = bitsb[y * w + px];
            int cov = mm & 0xFF; if (((mm >> 8) & 0xFF) > cov) cov = (mm >> 8) & 0xFF; if (((mm >> 16) & 0xFF) > cov) cov = (mm >> 16) & 0xFF;
            const int fa = bord ? 0x70 : 0xC0 - 0x90 * px / w;
            const int fond = bord ? 120 : 10;
            const int k = fond + (250 - fond) * cov / 255;
            const int aa = fa + (255 - fa) * cov / 255;
            ligne[px] = (DWORD)aa << 24 | (DWORD)k << 16 | (DWORD)k << 8 | (DWORD)k;
        }
    }
    PeindreIcones(dst, pitch, w, h, policeEtapeIcone);
}
static void PeindreGood(DWORD *dst, int pitch, int w, int h)
{
    memset(bitsb, 0, (size_t)w * h * 4);
    SetBkMode(dcb, TRANSPARENT); SetTextColor(dcb, RGB(255, 255, 255));
    SelectObject(dcb, policeGood);
    RECT r = {0, 0, w, h};
    DrawTextW(dcb, L"GOOD!", 5, &r, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    GdiFlush();
    if (dilate_n < w * h) {
        free(g_dilate); free(g_dilateTmp);
        g_dilate = malloc((size_t)w * h); g_dilateTmp = malloc((size_t)w * h);
        if (!g_dilate || !g_dilateTmp) { dilate_n = 0; return; }
        dilate_n = w * h;
    }
    const int rc = h / 22 > 1 ? h / 22 : 1;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            int d = 0;
            for (int k = x - rc; k <= x + rc; k++) if (k >= 0 && k < w && (int)(bitsb[y * w + k] & 0xFF) > d) d = bitsb[y * w + k] & 0xFF;
            g_dilateTmp[y * w + x] = (BYTE)d;
        }
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            int d = 0;
            for (int k = y - rc; k <= y + rc; k++) if (k >= 0 && k < h && g_dilateTmp[k * w + x] > d) d = g_dilateTmp[k * w + x];
            g_dilate[y * w + x] = (BYTE)d;
        }
    for (int y = 0; y < h; y++) {
        DWORD *ligne = (DWORD *)((BYTE *)dst + y * pitch);
        for (int x = 0; x < w; x++) {
            const int m = bitsb[y * w + x] & 0xFF, d = g_dilate[y * w + x] > m ? g_dilate[y * w + x] : m;
            const int a = d, v = d ? 255 * m / d : 0;
            const int k = 20 + (235 * v) / 255;
            ligne[x] = (DWORD)a << 24 | (DWORD)k << 16 | (DWORD)k << 8 | (DWORD)k;
        }
    }
}
static BOOL TextureArgb(IDirect3DDevice9 *dev, IDirect3DTexture9 **tex, int *tw, int *th, int w, int h)
{
    if (*tex && *tw == w && *th == h) return TRUE;
    if (*tex) { IDirect3DTexture9_Release(*tex); *tex = NULL; }
    if (FAILED(IDirect3DDevice9_CreateTexture(dev, w, h, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, tex, NULL))) return FALSE;
    *tw = w; *th = h;
    return TRUE;
}
static void DessinerEcranDefi(IDirect3DDevice9 *dev)
{
    if (!g_nbCoupsListe || !DefiPossible() || g_menuOuvert || g_choix >= g_nbCoupsListe) return;
    const DWORD tg = (DWORD)g_goodT;
    const BOOL good = tg && GetTickCount() - tg < DUREE_GOOD;
    if (!ExerciceActif() && !g_avancerA && !good) return;
    IDirect3DSurface9 *bb = NULL;
    if (FAILED(IDirect3DDevice9_GetBackBuffer(dev, 0, 0, D3DBACKBUFFER_TYPE_MONO, &bb)) || !bb) return;
    D3DSURFACE_DESC sd; IDirect3DSurface9_GetDesc(bb, &sd);
    float gh = (float)sd.Height, gw = gh * 16 / 9, gx = 0, gy = 0;
    if (gw > sd.Width) { gw = (float)sd.Width; gh = gw * 9 / 16; gy = (sd.Height - gh) / 2; } else gx = (sd.Width - gw) / 2;
    const float e = gh / 1080;

    const CoupListe *c = &g_coupsListe[g_choix];
    const BOOL cleared = c->fait && (g_avancerA || good);
    const int wb = (int)(1920 * e), hb = (int)(BARRE_H * e);
    const LONG cle = (g_versionListe & 0xFFFF) << 12 | g_choix << 2 | cleared << 1 | c->fait;
    if (wb >= 16 && hb >= 8) {
        if (cle != g_cleBarre || !g_texBarre || texb_w != wb || texb_h != hb) {
            if (SurfaceBarre(wb, hb) && TextureArgb(dev, &g_texBarre, &texb_w, &texb_h, wb, hb)) {
                static int policesH;
                if (policesH != hb) {
                    if (policeBarre) DeleteObject(policeBarre);
                    if (policeBarreIcone) DeleteObject(policeBarreIcone);
                    if (policeCleared) DeleteObject(policeCleared);
                    policeBarre = CreateFontW(-hb * 50 / 100, 0, 0, 0, FW_SEMIBOLD, FALSE, 0, 0, DEFAULT_CHARSET, 0, 0, ANTIALIASED_QUALITY, 0, L"Bahnschrift");
                    policeBarreIcone = CreateFontW(-hb * 38 / 100, 0, 0, 0, FW_BOLD, FALSE, 0, 0, DEFAULT_CHARSET, 0, 0, ANTIALIASED_QUALITY, 0, L"Bahnschrift");
                    policeCleared = CreateFontW(-hb * 72 / 100, hb * 72 / 100 * 70 / 100, 0, 0, FW_BLACK, TRUE, 0, 0, DEFAULT_CHARSET, 0, 0, ANTIALIASED_QUALITY, 0, L"Bahnschrift");
                    policesH = hb;
                }
                D3DLOCKED_RECT lr;
                if (SUCCEEDED(IDirect3DTexture9_LockRect(g_texBarre, 0, &lr, NULL, 0))) {
                    PeindreBarre((DWORD *)lr.pBits, lr.Pitch, wb, hb, c, cleared);
                    IDirect3DTexture9_UnlockRect(g_texBarre, 0);
                    g_cleBarre = cle;
                }
            }
        }
        if (g_texBarre) {
            const float x0 = (float)(int)gx - 0.5f, y0 = (float)(int)(gy + BARRE_Y * e) - 0.5f;
            DessinerTexture(dev, bb, &sd, g_texBarre, x0, y0, x0 + wb, y0 + hb, 255);
        }
    }

    {
        static LONG essaiVu, gardesVu; static BOOL etape1;
        const LONG essais = g_essais;
        if (essais != essaiVu) { essaiVu = essais; etape1 = FALSE; }
        if (g_nbGardes != gardesVu) {
            gardesVu = g_nbGardes;
            if (!etape1 && ExerciceActif() && (g_derniereGarde == c->ids[c->n - 1] || g_derniereGarde == (DWORD)g_exoJoue)) {
                etape1 = TRUE;
                InterlockedExchange(&g_sonDemande, SON_ETAPE);
            }
        }
        const BOOL e1 = etape1 || cleared, e2 = cleared;
        const int we = (int)(ETAPES_W * e), he = (int)(2 * ETAPE_H * e);
        const LONG cleE = (g_versionListe & 0xFFFF) << 12 | g_choix << 2 | e1 << 1 | e2;
        if (we >= 16 && he >= 16) {
            if (cleE != g_cleEtapes || !g_texEtapes || texe_w != we || texe_h != he) {
                if (SurfaceBarre(we, he) && TextureArgb(dev, &g_texEtapes, &texe_w, &texe_h, we, he)) {
                    static int policesE;
                    if (policesE != he) {
                        if (policeEtape) DeleteObject(policeEtape);
                        if (policeEtapeIcone) DeleteObject(policeEtapeIcone);
                        policeEtape = CreateFontW(-he / 2 * 50 / 100, 0, 0, 0, FW_SEMIBOLD, FALSE, 0, 0, DEFAULT_CHARSET, 0, 0, ANTIALIASED_QUALITY, 0, L"Bahnschrift");
                        policeEtapeIcone = CreateFontW(-he / 2 * 38 / 100, 0, 0, 0, FW_BOLD, FALSE, 0, 0, DEFAULT_CHARSET, 0, 0, ANTIALIASED_QUALITY, 0, L"Bahnschrift");
                        policesE = he;
                    }
                    D3DLOCKED_RECT lr;
                    if (SUCCEEDED(IDirect3DTexture9_LockRect(g_texEtapes, 0, &lr, NULL, 0))) {
                        PeindreEtapes((DWORD *)lr.pBits, lr.Pitch, we, he, c, e1, e2);
                        IDirect3DTexture9_UnlockRect(g_texEtapes, 0);
                        g_cleEtapes = cleE;
                    }
                }
            }
            if (g_texEtapes) {
                const float x0 = (float)(int)(gx + ETAPES_X * e) - 0.5f, y0 = (float)(int)(gy + ETAPES_Y * e) - 0.5f;
                DessinerTexture(dev, bb, &sd, g_texEtapes, x0, y0, x0 + we, y0 + he, 255);
            }
        }
    }

    if (good) {
        const int wg = (int)(GOOD_W * e), hg = (int)(GOOD_H * e);
        if (wg >= 16 && hg >= 16 && (!g_goodPeint || !g_texGood || texg_w != wg || texg_h != hg || good_w != wg)) {
            if (SurfaceBarre(wg, hg) && TextureArgb(dev, &g_texGood, &texg_w, &texg_h, wg, hg)) {
                if (policeGood) DeleteObject(policeGood);
                policeGood = CreateFontW(-hg * 80 / 100, hg * 80 / 100 * 62 / 100, 0, 0, FW_BLACK, TRUE, 0, 0, DEFAULT_CHARSET, 0, 0, ANTIALIASED_QUALITY, 0, L"Bahnschrift");
                D3DLOCKED_RECT lr;
                if (SUCCEEDED(IDirect3DTexture9_LockRect(g_texGood, 0, &lr, NULL, 0))) {
                    PeindreGood((DWORD *)lr.pBits, lr.Pitch, wg, hg);
                    IDirect3DTexture9_UnlockRect(g_texGood, 0);
                    g_goodPeint = TRUE; good_w = wg;
                }
            }
        }
        if (g_texGood && g_goodPeint) {
            const DWORD age = GetTickCount() - tg;
            const float k = age < 120 ? 1.35f - 0.35f * age / 120 : 1.0f;
            const float a = age > DUREE_GOOD - 300 ? (float)(DUREE_GOOD - age) / 300 : 1.0f;
            const float cx = gx + 960 * e, cy = gy + 540 * e, dw = wg * k / 2, dh = hg * k / 2;
            DessinerTexture(dev, bb, &sd, g_texGood, cx - dw, cy - dh, cx + dw, cy + dh, (DWORD)(255 * a));
        }
    }
    IDirect3DSurface9_Release(bb);
}

typedef HRESULT (WINAPI *Present_t)(IDirect3DDevice9 *, const RECT *, const RECT *, HWND, const RGNDATA *);
typedef HRESULT (WINAPI *CreateDevice_t)(IDirect3D9 *, UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS *, IDirect3DDevice9 **);
typedef struct { void **vt; Present_t present; } CrochetDevice;
static CrochetDevice g_devices[4]; static volatile LONG g_nbDevices;
static CreateDevice_t o_createDevice;
static HRESULT WINAPI hk_present(IDirect3DDevice9 *d, const RECT *a, const RECT *b, HWND w, const RGNDATA *r)
{
    void **vt = *(void ***)d; Present_t orig = NULL;
    for (LONG i = 0; i < g_nbDevices; i++) if (g_devices[i].vt == vt) orig = g_devices[i].present;
    Echantillonner();
    Dessiner(d);
    DessinerListe(d);
    DessinerEcranDefi(d);
    return orig ? orig(d, a, b, w, r) : D3D_OK;
}
static void AccrocherDevice(IDirect3DDevice9 *dev)
{
    void **vt = *(void ***)dev; DWORD ancien;
    for (LONG i = 0; i < g_nbDevices; i++) if (g_devices[i].vt == vt) return;
    if (g_nbDevices >= 4 || vt[17] == (void *)hk_present) return;
    if (VirtualProtect(&vt[17], 4, PAGE_EXECUTE_READWRITE, &ancien)) {

        void *avant;
        do { avant = vt[17]; g_devices[g_nbDevices].vt = vt; g_devices[g_nbDevices].present = (Present_t)avant; }
        while (InterlockedCompareExchangePointer(&vt[17], (void *)hk_present, avant) != avant);
        InterlockedIncrement(&g_nbDevices);
        VirtualProtect(&vt[17], 4, ancien, &ancien);
        Journal("affichage_accroche", g_nbDevices, 0, 0);
    }
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
                Journal("affichage_filet", 1, 0, 0);
                return 0;
            }
        }
        Sleep(100);
    }
    return 0;
}
static HRESULT WINAPI hk_createDevice(IDirect3D9 *d3d, UINT a, D3DDEVTYPE t, HWND w, DWORD f, D3DPRESENT_PARAMETERS *pp, IDirect3DDevice9 **out)
{
    HRESULT hr = o_createDevice(d3d, a, t, w, f, pp, out);
    if (SUCCEEDED(hr) && out && *out && t == D3DDEVTYPE_HAL) AccrocherDeviceSur(*out);
    return hr;
}
static void AccrocherD3D(void)
{
    IDirect3D9 *d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d) { Journal("d3d9_indisponible", 0, 0, 0); return; }
    void **vt = *(void ***)d3d; DWORD ancien;
    if (VirtualProtect(&vt[16], 4, PAGE_EXECUTE_READWRITE, &ancien)) {
        void *avant;
        do { avant = vt[16]; o_createDevice = (CreateDevice_t)avant; }
        while (InterlockedCompareExchangePointer(&vt[16], (void *)hk_createDevice, avant) != avant);
        VirtualProtect(&vt[16], 4, ancien, &ancien);
    }
    IDirect3D9_Release(d3d);
    HANDLE h = CreateThread(NULL, 0, FiletDevice, vt, 0, NULL);
    if (h) CloseHandle(h);
}

#ifdef PUNISH_TEST

#define RVA_SON 0x557C40
typedef int (__cdecl *Son_t)(int, int);
static Son_t o_son;
static volatile LONG g_nbSons;
static int __cdecl hk_son(int son, int parametre)
{
    const DWORD appelant = (DWORD)(ULONG_PTR)__builtin_return_address(0) - (DWORD)(ULONG_PTR)base;
    if (InterlockedIncrement(&g_nbSons) <= 400)
        Journal("son", son, (DWORD)parametre < 0x10000 ? parametre : -1, (long)appelant);
    return o_son(son, parametre);
}
static BOOL AccrocherSons(void)
{
    static const BYTE attendu[] = {0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x0C};
    BYTE *f = base + RVA_SON;
    if (f[0] == 0xE9) {
        LONG avant; memcpy(&avant, f + 1, 4);
        o_son = (Son_t)(f + 5 + avant);
    } else {
        if (memcmp(f, attendu, sizeof attendu)) return FALSE;
        BYTE *t = VirtualAlloc(NULL, 16, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        if (!t) return FALSE;
        memcpy(t, attendu, sizeof attendu);
        t[6] = 0xE9;
        const LONG retour = (LONG)((f + 6) - (t + 11));
        memcpy(t + 7, &retour, 4);
        o_son = (Son_t)t;
    }
    DWORD ancien;
    if (!VirtualProtect(f, 6, PAGE_EXECUTE_READWRITE, &ancien)) return FALSE;
    BYTE saut[5] = {0xE9};
    const LONG rel = (LONG)((BYTE *)hk_son - (f + 5));
    memcpy(saut + 1, &rel, 4);
    memcpy(f, saut, 5);
    if (f[5] == 0x0C) f[5] = 0x90;
    VirtualProtect(f, 6, ancien, &ancien);
    FlushInstructionCache(GetCurrentProcess(), f, 6);
    return TRUE;
}
#endif
static DWORD WINAPI Demarrer(LPVOID p)
{
    (void)p;
    AccrocherD3D();
    int ok = 0;
    for (int n = 0; n < 600 && !ok; n++) {
        ok = !memcmp(base + RVA_KTOL_PRINTF, KTOL_PRINTF_BYTES, sizeof KTOL_PRINTF_BYTES);
        if (!ok) Sleep(100);
    }
    if (!ok) { Journal("refus_version_game_exe", 0, 0, 0); return 0; }
    Journal("pret", 0, 0, 0);
    Journal("crochet_ia_dummy", AccrocherIADummy(), 0, 0);

#ifdef PUNISH_TEST
    Journal("crochet_sons", AccrocherSons(), 0, 0);
#endif
    ChargerSauvegarde();
    Journal("defi_sauvegarde", g_nbSauves, 0, 0);
    InterlockedExchange(&g_pret, 1);
    return 0;
}
#ifdef DOA5EXTRA
void Punish_Demarrer(HINSTANCE h)
{
#else
BOOL WINAPI DllMain(HINSTANCE h, DWORD raison, LPVOID r)
{
    (void)r;
    if (raison != DLL_PROCESS_ATTACH) return TRUE;
    DisableThreadLibraryCalls(h);
#endif
    base = (BYTE *)GetModuleHandleW(NULL);
    InitializeCriticalSection(&g_csLog);
    InitializeCriticalSection(&g_csVerdict);
    InitializeCriticalSection(&g_csExo);
    g_t0 = GetTickCount();
    char chemin[MAX_PATH];
    const DWORD l = GetModuleFileNameA(h, chemin, sizeof chemin);
    char *sep = l && l < sizeof chemin ? strrchr(chemin, '\\') : NULL;
    if (sep && (size_t)(sep + 1 - chemin) + 20 < sizeof chemin) {
        strcpy(sep + 1, "PunishTraining.sav");
        strcpy(g_cheminSave, chemin);
#ifndef DOA5EXTRA
        strcpy(sep + 1, "PunishTraining.log");
        g_log = fopen(chemin, "w");
#endif
    }
    Journal("debut " VERSION_PUNISH, 0, 0, 0);
    HANDLE t = CreateThread(NULL, 0, Demarrer, NULL, 0, NULL);
    if (t) CloseHandle(t);
#ifndef DOA5EXTRA
    return TRUE;
#endif
}
