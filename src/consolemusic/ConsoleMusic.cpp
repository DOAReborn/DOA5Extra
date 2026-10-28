#include <windows.h>
#include "../commun/journal_extra.h"
#include <cstdarg>
#include <cstdio>
#include <cstring>

static const char* Version = "1.2b";
static BYTE* Base;
static BYTE* At(DWORD va) { return Base + va - 0x400000; }

static const DWORD GraphLookupVa = 0x95F8F0, GraphTableVa = 0x2470824;
static const DWORD NodeTableVa = 0x2470828, NodeCountVa = 0x247082C;
static const DWORD StageRowsVa = 0x2471334, StageCountVa = 0x24712E4, ArrivalRowsVa = 0x2471338, ArrivalCountVa = 0x24712E8;
static const DWORD NativeGraphs = 0x1EAB, NativeNodes = 14001, DonorNode = 6;
static const DWORD NoGraph = 0xFFFFFFFF;

enum { NFireworks, NAzuchi, NTitan, NodeTotal };
struct NodeSpec { const char* name; WORD cue; BYTE selection; };
static const NodeSpec NodeSpecs[NodeTotal] = { {"Fireworks", 91, 0}, {"Azuchi", 79, 1}, {"Attack on Titan", 77, 0} };
static BYTE Nodes[NodeTotal][0x40];

enum { GFireworks, GAzuchi1, GAzuchiArrival, GAzuchi2, GTitan, GraphTotal };
#define NODE_ID(k) (BYTE)((NativeNodes + (k)) >> 8), (BYTE)(NativeNodes + (k))
static const BYTE Code[GraphTotal][12] = {
    {0x00, NODE_ID(NFireworks), 0x00, 0x00, 0x1E},
    {0x01, NODE_ID(NAzuchi), 0x00, 0x00, 0x1E},
    {0x18, 0x00, 0x01, 0x01, NODE_ID(NAzuchi), 0x00, 0x00, 0x1E},
    {0x18, 0x00, 0x02, 0x01, NODE_ID(NAzuchi), 0x00, 0x00, 0x1E},
    {0x00, NODE_ID(NTitan), 0x00, 0x00, 0x1E},
};

struct MusicRow { const char* name; DWORD rowsVa, countVa, stage, graph; bool* enabled; };
static bool UseFireworks = true, UseAzuchi = true, UseTitan = true, LogOn = true;
static const MusicRow Rows[] = {
    {"Fireworks stage", StageRowsVa, StageCountVa, 18, NativeGraphs + GFireworks, &UseFireworks},
    {"Azuchi 1 stage", StageRowsVa, StageCountVa, 23, NativeGraphs + GAzuchi1, &UseAzuchi},
    {"Azuchi 2 stage", StageRowsVa, StageCountVa, 32, NativeGraphs + GAzuchi2, &UseAzuchi},
    {"Attack on Titan stage", StageRowsVa, StageCountVa, 28, NativeGraphs + GTitan, &UseTitan},
    {"Azuchi 1 arrival", ArrivalRowsVa, ArrivalCountVa, 23, NativeGraphs + GAzuchiArrival, &UseAzuchi},
    {"Azuchi 2 arrival", ArrivalRowsVa, ArrivalCountVa, 32, NativeGraphs + GAzuchiArrival, &UseAzuchi},
};

static char Root[MAX_PATH], LogPath[MAX_PATH], IniPath[MAX_PATH];
static void Log(const char* fmt, ...) {
    if (!LogOn) return;
    va_list a; va_start(a, fmt); JournalExtra_V("ConsoleMusic", fmt, a); va_end(a);
}

static volatile LONG NodesLive = 0;

static DWORD __cdecl GraphLookup(DWORD arg) {
    WORD id = (WORD)arg;
    if (id >= NativeGraphs) { DWORD k = id - NativeGraphs; return k < GraphTotal && NodesLive ? (DWORD)Code[k] : 0; }
    DWORD* t = *(DWORD* volatile*)At(GraphTableVa);
    return t ? t[id * 2] : 0;
}

static bool Detoured = false, DetourRefused = false;
static void TryDetour() {
    BYTE* p = At(GraphLookupVa);
    DWORD table = (DWORD)At(GraphTableVa);
    BYTE want[22] = {0x55, 0x8B, 0xEC, 0x8B, 0x0D, 0, 0, 0, 0, 0x85, 0xC9, 0x74, 0x16, 0x66, 0x8B, 0x45, 0x08, 0xBA, 0xAB, 0x1E, 0x00, 0x00};
    memcpy(want + 5, &table, 4);
    if (memcmp(p, want, sizeof(want))) {
        if (p[0] == 0xE9 && !DetourRefused) { DetourRefused = true; Log("Refused: graph lookup 0x95F8F0 already patched by another module"); }
        return;
    }
    BYTE jmp[5] = {0xE9}; DWORD rel = (DWORD)&GraphLookup - (DWORD)p - 5; memcpy(jmp + 1, &rel, 4);
    DWORD old, ignored;
    if (!VirtualProtect(p, 5, PAGE_EXECUTE_READWRITE, &old)) { Log("Refused: VirtualProtect 0x95F8F0 failed (%lu)", GetLastError()); DetourRefused = true; return; }
    memcpy(p, jmp, 5);
    VirtualProtect(p, 5, old, &ignored);
    FlushInstructionCache(GetCurrentProcess(), p, 5);
    Detoured = true;
    Log("Graph lookup detoured: ids 0x%X..0x%X = console music graphs", NativeGraphs, NativeGraphs + GraphTotal - 1);
}

static DWORD* LastNodeTable = nullptr; static bool NodeWarned = false;
static bool InstallNodes() {
    DWORD* t = *(DWORD* volatile*)At(NodeTableVa);
    volatile DWORD* count = (volatile DWORD*)At(NodeCountVa);
    if (!t) { NodesLive = 0; return false; }
    if (*count == NativeNodes + NodeTotal && t[NativeNodes * 2] == (DWORD)Nodes[0]) { NodesLive = 1; return true; }
    NodesLive = 0;
    if (*count != NativeNodes) {
        if (!NodeWarned) { NodeWarned = true; Log("Refused: node table has %lu entries (expected %lu)", *count, NativeNodes); }
        return false;
    }
    for (DWORD k = 0; k < NodeTotal; k++)
        if (t[(NativeNodes + k) * 2]) {
            if (!NodeWarned) { NodeWarned = true; Log("Refused: node slot %lu is not free", NativeNodes + k); }
            return false;
        }
    BYTE* donor = (BYTE*)t[DonorNode * 2];
    DWORD type = t[DonorNode * 2 + 1];
    if (!donor || *(DWORD*)donor || *(DWORD*)(donor + 0x14) || *(WORD*)(donor + 0x1C) != 222 || *(WORD*)(donor + 0x1E) != 84 || donor[0x20] != 0x0F) {
        if (!NodeWarned) { NodeWarned = true; Log("Refused: node %lu is not the Danger Zone music node (group 222, cue 84)", DonorNode); }
        return false;
    }
    for (DWORD k = 0; k < NodeTotal; k++) {
        BYTE* n = Nodes[k];
        memcpy(n, donor, 0x40);
        *(float*)(n + 0x08) = 1.0f;
        *(WORD*)(n + 0x1A) = (WORD)(NativeNodes + k);
        *(WORD*)(n + 0x1E) = NodeSpecs[k].cue;
        n[0x21] = NodeSpecs[k].selection;
        t[(NativeNodes + k) * 2 + 1] = type;
        t[(NativeNodes + k) * 2] = (DWORD)n;
    }
    *count = NativeNodes + NodeTotal;
    NodesLive = 1;
    if (LastNodeTable != t) {
        Log("Music nodes %lu..%lu installed (cues 91, 79, 77 of group 222)%s", NativeNodes, NativeNodes + NodeTotal - 1, LastNodeTable ? " after a sound script reload" : "");
        LastNodeTable = t;
    }
    return true;
}

static DWORD* LastRows[sizeof(Rows) / sizeof(Rows[0])];
static void FillRows() {
    for (unsigned i = 0; i < sizeof(Rows) / sizeof(Rows[0]); i++) {
        const MusicRow& r = Rows[i];
        if (!*r.enabled) continue;
        DWORD* t = *(DWORD* volatile*)At(r.rowsVa);
        DWORD n = *(volatile DWORD*)At(r.countVa);
        if (!t || r.stage >= n) continue;
        if (t[r.stage] == r.graph) continue;
        if (t[r.stage] != NoGraph) {
            if (LastRows[i] != t) { LastRows[i] = t; Log("Kept: %s row %lu already plays graph %ld", r.name, r.stage, (long)t[r.stage]); }
            continue;
        }
        t[r.stage] = r.graph;
        if (LastRows[i] != t) { LastRows[i] = t; Log("%s row %lu -> graph 0x%lX", r.name, r.stage, r.graph); }
    }
}

static const DWORD TrackTableVa = 0x2471340, TrackCountVa = 0x24712F0, NativeTracks = 140, StageTrack = 105;
static const DWORD NameTableVa = 0xD73A90, OrderTableVa = 0xD73CE8, MsgLookupVa = 0xB45BA0;
enum { TFireworks, TAzuchi, TTitan, TrackTotal };
static const DWORD TrackGraph[TrackTotal] = {NativeGraphs + GFireworks, NativeGraphs + GAzuchi1, NativeGraphs + GTitan};
static const char* const TrackKey[TrackTotal] = {"Fireworks", "Azuchi", "AttackOnTitan"};
static const wchar_t* const TrackDefault[TrackTotal] = {L"Tropical Beat", L"Look Ahead", L"Against the mega disastrous blow -Last Round mix-"};
static const DWORD FakeMsg = 0x7FFF0000;
static DWORD Tracks[NativeTracks + TrackTotal], Names[NativeTracks + TrackTotal];
static BYTE Order[NativeTracks + TrackTotal];
static char NameA[TrackTotal][160]; static wchar_t NameW[TrackTotal][160];
static bool UseMenu = true, MenuPatched = false, MenuRefused = false, TrackWarned = false;
static int Wide = -1;

typedef DWORD(__cdecl* MsgFn)(DWORD, DWORD, DWORD);
static void DumpText(const char* what, DWORD msg, const BYTE* s) {
    if (!s) { Log("Text %s 0x%lX : none", what, msg); return; }
    char hex[3 * 48 + 1] = ""; unsigned n = 0;
    for (; n < 48; n++) { if (!s[n] && (Wide != 1 || (!(n & 1) && !s[n + 1]))) break; sprintf(hex + 3 * n, "%02X ", s[n]); }
    Log("Text %s 0x%lX : %s(%u bytes)", what, msg, hex, n);
}
static DWORD __cdecl MsgHook(DWORD lang, DWORD msg, DWORD flags) {
    MsgFn orig = (MsgFn)At(MsgLookupVa);
    if (msg - FakeMsg >= TrackTotal) return orig(lang, msg, flags);
    if (Wide < 0) {
        DWORD known = ((DWORD*)At(NameTableVa))[0];
        const BYTE* s = (const BYTE*)orig(lang, known, flags);
        Wide = s && s[0] && !s[1] ? 1 : 0;
        Log("Menu names : game text is %s (language %lu)", Wide ? "UTF-16" : "8-bit", lang);
        DumpText("track 0", known, s);
        static const DWORD probe[] = {0x3900A4, 0x3900A5, 0x3900A7, 0x3900A8, 0x3900A9, 0x3900AA, 0x3900AB, 0x3900AC};
        for (DWORD m : probe) DumpText("probe", m, (const BYTE*)orig(lang, m, flags));
    }
    DWORD k = msg - FakeMsg;
    return Wide ? (DWORD)NameW[k] : (DWORD)NameA[k];
}

struct MusicPatch { DWORD va; BYTE n; BYTE want[8]; BYTE put[8]; };
static void Put32(BYTE* p, DWORD v) { memcpy(p, &v, 4); }
static void PatchMenu() {
    DWORD names = (DWORD)At(NameTableVa), order = (DWORD)At(OrderTableVa), msg = (DWORD)At(MsgLookupVa);
    const BYTE* nativeOrder = (const BYTE*)order;
    unsigned at = 0, j = 0;
    while (at < NativeTracks && nativeOrder[at] != StageTrack) at++;
    for (unsigned i = 0; i < NativeTracks; i++) {
        if (i == at) for (DWORD k = 0; k < TrackTotal; k++) Order[j++] = (BYTE)(NativeTracks + k);
        Order[j++] = nativeOrder[i];
    }
    if (at == NativeTracks) for (DWORD k = 0; k < TrackTotal; k++) Order[j++] = (BYTE)(NativeTracks + k);
    memcpy(Names, (const void*)names, NativeTracks * 4);
    for (DWORD k = 0; k < TrackTotal; k++) Names[NativeTracks + k] = FakeMsg + k;

    MusicPatch p[] = {
        {0x4D9B46, 2, {0x3C, 0x8C}, {0x3C, 0x8F}},
        {0x4D9C1F, 3, {0x80, 0xF9, 0x8C}, {0x80, 0xF9, 0x8F}},
        {0x4D9C5B, 2, {0x3C, 0x8C}, {0x3C, 0x8F}},
        {0x4DA0EA, 2, {0x3C, 0x8C}, {0x3C, 0x8F}},
        {0x4DA3DE, 7, {0x8B, 0x04, 0x85}, {0x8B, 0x04, 0x85}},
        {0x4DA77E, 7, {0x8B, 0x04, 0x85}, {0x8B, 0x04, 0x85}},
        {0x4DA3F4, 5, {0xE8}, {0xE8}},
        {0x4DA794, 5, {0xE8}, {0xE8}},
        {0x4DD1D7, 5, {0xBB}, {0xBB}},
        {0x4DD1E2, 7, {0xC7, 0x45, 0xF8, 0x8C, 0, 0, 0}, {0xC7, 0x45, 0xF8, 0x8F, 0, 0, 0}},
    };
    Put32(p[4].want + 3, names); Put32(p[4].put + 3, (DWORD)Names);
    Put32(p[5].want + 3, names); Put32(p[5].put + 3, (DWORD)Names);
    for (int i = 6; i < 8; i++) {
        BYTE* site = At(p[i].va);
        Put32(p[i].want + 1, msg - (DWORD)site - 5); Put32(p[i].put + 1, (DWORD)&MsgHook - (DWORD)site - 5);
    }
    Put32(p[8].want + 1, order); Put32(p[8].put + 1, (DWORD)Order);
    for (const MusicPatch& q : p)
        if (memcmp(At(q.va), q.want, q.n)) {
            Log("Menu refused: 0x%lX is not the original code (another module ?) - music list left as is", q.va);
            MenuRefused = true; return;
        }
    for (const MusicPatch& q : p) {
        BYTE* s = At(q.va); DWORD old, ignored;
        if (!VirtualProtect(s, q.n, PAGE_EXECUTE_READWRITE, &old)) { Log("Menu: VirtualProtect 0x%lX failed (%lu)", q.va, GetLastError()); MenuRefused = true; return; }
        memcpy(s, q.put, q.n);
        VirtualProtect(s, q.n, old, &ignored);
        FlushInstructionCache(GetCurrentProcess(), s, q.n);
    }
    MenuPatched = true;
    Log("Menu patched: %lu tracks, console tracks %lu..%lu at list position %u", NativeTracks + TrackTotal, NativeTracks, NativeTracks + TrackTotal - 1, at);
}

static DWORD* LastTracks = nullptr;
static void InstallTracks() {
    DWORD* volatile* pp = (DWORD* volatile*)At(TrackTableVa);
    volatile DWORD* count = (volatile DWORD*)At(TrackCountVa);
    DWORD* t = *pp;
    if (!t || t == Tracks) return;
    if (*count != NativeTracks) {
        if (!TrackWarned) { TrackWarned = true; Log("Menu: track table has %lu entries (expected %lu), not extended", *count, NativeTracks); }
        return;
    }
    memcpy(Tracks, t, NativeTracks * 4);
    for (DWORD k = 0; k < TrackTotal; k++) Tracks[NativeTracks + k] = TrackGraph[k];
    *pp = Tracks;
    MemoryBarrier();
    *count = NativeTracks + TrackTotal;
    Log("Track table extended: 140 -> graph 0x%lX, 141 -> 0x%lX, 142 -> 0x%lX%s", TrackGraph[0], TrackGraph[1], TrackGraph[2], LastTracks ? " after a sound script reload" : "");
    LastTracks = t;
}

static bool LogRequests = false, RequestHooked = false;
typedef DWORD(__cdecl* RequestFn)(DWORD, BYTE*);
typedef DWORD(__cdecl* PlayFn)(DWORD, BYTE*, DWORD);
static RequestFn RequestOrig; static PlayFn PlayOrig;
static volatile LONG InMusicRequest = 0; static volatile DWORD PlayedGraph;
static DWORD CurrentStage() {
    BYTE* mgr = *(BYTE* volatile*)At(0x2472580); if (!mgr) return 0xFF;
    BYTE* c = *(BYTE* volatile*)(mgr + 0xC); return c ? *(DWORD*)(c + 8) : 0xFF;
}
static unsigned CharSetting(BYTE ch) {
    const BYTE* ids = At(0xD71F1C);
    for (unsigned i = 0; i < 0x28; i++) if (ids[i] == ch) return i < 0x26 ? At(0x1374196)[i] : At(0x137879D)[i];
    return 0xFFu;
}
static DWORD __cdecl PlayHook(DWORD graph, BYTE* req, DWORD flags) {
    if (InMusicRequest) PlayedGraph = graph;
    return PlayOrig(graph, req, flags);
}
static DWORD __cdecl RequestHook(DWORD id, BYTE* req) {
    if ((id >> 28) != 6) return RequestOrig(id, req);
    PlayedGraph = 0xFFFFFFFE; InterlockedIncrement(&InMusicRequest);
    DWORD r = RequestOrig(id, req);
    InterlockedDecrement(&InMusicRequest);
    BYTE p1 = *At(0x13D0544), p2 = *At(0x13D0544 + 0x6C8);
    Log("Request 0x%08lX stage=%lu bgmMode=%u p1=%u(setting 0x%X) p2=%u(setting 0x%X) track=%d -> graph %ld", id, CurrentStage(),
        *At(0x1374195), p1, CharSetting(p1), p2, CharSetting(p2), id == 0x60000043 && req ? req[0x32] : -1, (long)PlayedGraph);
    return r;
}
static void* Trampoline(DWORD va, const BYTE* expect, unsigned n, void* hook, const char* name) {
    BYTE* p = At(va);
    if (memcmp(p, expect, n)) { Log("Diagnostics: %s 0x%lX already patched, not hooked", name, va); return nullptr; }
    BYTE* t = (BYTE*)VirtualAlloc(nullptr, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!t) return nullptr;
    memcpy(t, p, n); t[n] = 0xE9; *(DWORD*)(t + n + 1) = (DWORD)(p + n) - (DWORD)(t + n + 5);
    DWORD old, ignored;
    if (!VirtualProtect(p, n, PAGE_EXECUTE_READWRITE, &old)) return nullptr;
    *(DWORD*)(p + 1) = (DWORD)hook - (DWORD)(p + 5); p[0] = 0xE9;
    for (unsigned i = 5; i < n; i++) p[i] = 0x90;
    VirtualProtect(p, n, old, &ignored);
    FlushInstructionCache(GetCurrentProcess(), p, n);
    return t;
}
static void HookRequests() {
    static const BYTE request[] = {0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x0C}, play[] = {0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x3C};
    RequestHooked = true;
    if (!(PlayOrig = (PlayFn)Trampoline(0x957650, play, 6, (void*)&PlayHook, "play graph"))) return;
    if (!(RequestOrig = (RequestFn)Trampoline(0x957C40, request, 6, (void*)&RequestHook, "sound request"))) return;
    Log("Diagnostics: music requests are logged");
}

static DWORD WINAPI Watch(LPVOID) {
    if (Extra_SepareCharge("DOA5LR-ConsoleMusic.asi")) { Log("Refused: separate DOA5LR-ConsoleMusic.asi is loaded"); return 0; }
    for (unsigned tick = 0;; tick++) {
        if (!Detoured && !DetourRefused) TryDetour();
        if (Detoured && LogRequests && !RequestHooked) HookRequests();
        if (Detoured && UseMenu && !MenuPatched && !MenuRefused) PatchMenu();
        if (Detoured && InstallNodes()) { FillRows(); if (MenuPatched) InstallTracks(); }
        if (!Detoured && !DetourRefused && tick == 600) Log("Still waiting for the graph lookup (game.exe not 1.10C ?)");
        Sleep(Detoured && NodesLive ? 500 : 100);
    }
}

static bool IniBool(const char* sec, const char* key, bool def) { return GetPrivateProfileIntA(sec, key, def ? 1 : 0, IniPath) != 0; }

extern "C" void ConsoleMusic_Demarrer(void) {
    Base = (BYTE*)GetModuleHandleA(nullptr);
    char exe[MAX_PATH]; GetModuleFileNameA(nullptr, exe, MAX_PATH);
    snprintf(Root, sizeof(Root), "%s", exe); if (char* s = strrchr(Root, '\\')) s[1] = 0;
    snprintf(IniPath, sizeof(IniPath), "%s", Extra_Ini()); (void)LogPath;
    LogOn = true;
    LogRequests = IniBool("ConsoleMusic", "LogRequests", false);
    UseFireworks = IniBool("ConsoleMusic", "Fireworks", true);
    UseAzuchi = IniBool("ConsoleMusic", "Azuchi", true);
    UseTitan = IniBool("ConsoleMusic", "AttackOnTitan", true);
    UseMenu = IniBool("ConsoleMusic", "Menu", true);
    wchar_t iniW[MAX_PATH]; MultiByteToWideChar(CP_ACP, 0, IniPath, -1, iniW, MAX_PATH);
    for (int k = 0; k < TrackTotal; k++) {
        wchar_t key[32]; MultiByteToWideChar(CP_ACP, 0, TrackKey[k], -1, key, 32);
        GetPrivateProfileStringW(L"ConsoleMusicNames", key, TrackDefault[k], NameW[k], 160, iniW);
        if (!NameW[k][0]) wcscpy(NameW[k], TrackDefault[k]);
        WideCharToMultiByte(CP_UTF8, 0, NameW[k], -1, NameA[k], sizeof(NameA[k]), nullptr, nullptr);
    }
    Log("Startup v%s (Fireworks=%d Azuchi=%d AttackOnTitan=%d Menu=%d)", Version, UseFireworks, UseAzuchi, UseTitan, UseMenu);
    if (HANDLE h = CreateThread(nullptr, 0, Watch, nullptr, 0, nullptr)) CloseHandle(h);
}
