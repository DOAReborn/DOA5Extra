#include "../commun/extra.h"
#include "StateCostumes.h"
#include "HotkeyEdge.h"
#include "StateCostumesData.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "FastRegion.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>

static_assert(sizeof(void*) == 4, "DOA5LR module requires i686 / 32-bit pointers");
namespace StateCostumes {
namespace {
struct CostumeOptions {
    volatile LONG enabled[2]{1,1};
    char ini[MAX_PATH]{};
    CostumeOptions() noexcept {
        const DWORD length=GetModuleFileNameA(nullptr,ini,MAX_PATH);
        if(!length||length>=MAX_PATH){ini[0]=0;return;}
        char* name=std::strrchr(ini,'\\');
        if(!name){ini[0]=0;return;}
        std::snprintf(ini,sizeof ini,"%s",Extra_Ini());
        enabled[0]=GetPrivateProfileIntA("StatefulCostumes","EnableCostumeDestruction",1,ini)!=0;
        enabled[1]=GetPrivateProfileIntA("StatefulCostumes","EnableCostumeTransformation",1,ini)!=0;
    }
};
CostumeOptions& Options() noexcept {static CostumeOptions options;return options;}
}
bool IsTransitionEnabled(bool schoolgirl) noexcept {
    return InterlockedCompareExchange(&Options().enabled[schoolgirl?1:0],0,0)!=0;
}
void SetTransitionEnabled(bool schoolgirl,bool enabled) noexcept {
    InterlockedExchange(&Options().enabled[schoolgirl?1:0],enabled?1:0);
}
bool SaveTransitionOptions() noexcept {
    auto& options=Options();
    if(!options.ini[0])return false;
    bool a=WritePrivateProfileStringA("StatefulCostumes","EnableCostumeDestruction",IsTransitionEnabled(false)?"1":"0",options.ini)!=0;
    bool b=WritePrivateProfileStringA("StatefulCostumes","EnableCostumeTransformation",IsTransitionEnabled(true)?"1":"0",options.ini)!=0;
    return a&&b;
}

namespace {
constexpr unsigned kActorCapacity = 16;
struct Resolved {
    void* entity{};
    const unsigned char* body{};
    unsigned char* begin{};
    unsigned char* end{};
    const Data::Model* model{};
    uint32_t actorTag0{}, actorTag4{};
};
struct Actor {
    Resolved identity{};
    unsigned char native[Data::kMaxNodes]{};
    bool transformed{};
    bool initialized{};
    bool offlineVisibilityDirty{};
};
Actor actors[kActorCapacity]{};
SRWLOCK lock = SRWLOCK_INIT;
Logger logger = nullptr;
RenderOnlyPredicate renderOnlyPredicate = nullptr;
thread_local RenderScope* currentRenderScope = nullptr;

thread_local RenderScope pendingRenderRestore{};
static_assert(Data::kMaxNodes <= sizeof(RenderScope{}.baseline), "RenderScope buffer too small");
bool RenderOnlyRequested() noexcept {
    return currentRenderScope || pendingRenderRestore.active
        || (renderOnlyPredicate && renderOnlyPredicate());
}
bool ScopeMatches(const Resolved& resolved, const RenderScope& scope) noexcept {
    return scope.active && scope.entity == resolved.entity && scope.body == resolved.body
        && scope.begin == resolved.begin && scope.model == resolved.model
        && scope.nodes == resolved.model->nodes && scope.tag0 == resolved.actorTag0
        && scope.tag4 == resolved.actorTag4;
}
bool DeferVisibility(const Resolved& resolved) noexcept {
    return RenderOnlyRequested() && (!currentRenderScope || !ScopeMatches(resolved, *currentRenderScope));
}
bool admissionFrozen = false;
bool overflowLogged = false;
struct TryLock {
    bool held = TryAcquireSRWLockExclusive(&lock) != FALSE;
    ~TryLock() { if (held) ReleaseSRWLockExclusive(&lock); }
};

bool MemoryAccessible(const void* pointer, size_t size, bool writing) noexcept {
    const uintptr_t first = reinterpret_cast<uintptr_t>(pointer);
    if (!first || !size || size > std::numeric_limits<uintptr_t>::max() - first)
        return false;
    const uintptr_t end = first + size;
    uintptr_t cursor = first;
    while (cursor < end) {
        MEMORY_BASIC_INFORMATION region{};

        if (FastRegion::Query(reinterpret_cast<const void*>(cursor), &region, sizeof(region)) != sizeof(region))
            return false;
        if (region.State != MEM_COMMIT || (region.Protect & (PAGE_GUARD | PAGE_NOACCESS)))
            return false;
        const DWORD mode = region.Protect & 0xff;
        const bool writable = mode == PAGE_READWRITE || mode == PAGE_WRITECOPY
            || mode == PAGE_EXECUTE_READWRITE || mode == PAGE_EXECUTE_WRITECOPY;
        const bool readable = writable || mode == PAGE_READONLY || mode == PAGE_EXECUTE_READ;
        if (!readable || (writing && !writable)) return false;
        const uintptr_t regionStart = reinterpret_cast<uintptr_t>(region.BaseAddress);
        if (region.RegionSize > std::numeric_limits<uintptr_t>::max() - regionStart)
            return false;
        const uintptr_t regionEnd = regionStart + region.RegionSize;
        if (regionEnd <= cursor) return false;
        cursor = regionEnd < end ? regionEnd : end;
    }
    return true;
}
bool MemoryReadable(const void* pointer, size_t size) noexcept {
    return MemoryAccessible(pointer, size, false);
}
template<class T> bool Read(const void* pointer, T& value) noexcept {
    if (!MemoryReadable(pointer, sizeof(T))) return false;
    std::memcpy(&value, pointer, sizeof(T));
    return true;
}
bool Offset(const void* base, uintptr_t amount, const void*& out) noexcept {
    const uintptr_t start = reinterpret_cast<uintptr_t>(base);
    if (!start || amount > std::numeric_limits<uintptr_t>::max() - start) return false;
    out = reinterpret_cast<const void*>(start + amount);
    return true;
}
const Data::Model* Recognize(const unsigned char* body) noexcept {
    if (!MemoryReadable(body, 0x60) || std::memcmp(body, "TMC\0", 4)) return nullptr;
    uint32_t length{};
    std::memcpy(&length, body + 0x10, sizeof(length));
    const Data::Model* model = nullptr;
    for (const auto& candidate : Data::kModels) {
        if (length == candidate.size && Data::Fnv64(body + 0x50, 16) == candidate.identityHash) {
            model = &candidate;
            break;
        }
    }
    if (!model || !MemoryReadable(body, model->size)) return nullptr;

    uint64_t hash = Data::kFnvBasis;
    for (unsigned i = 0; i < model->guardCount; ++i) {
        const auto& guard = Data::kGuards[model->guardStart + i];
        if (guard.offset > model->size || guard.size > model->size - guard.offset) return nullptr;
        hash = Data::Fnv64(body + guard.offset, guard.size, hash);
    }
    return hash == model->guardHash ? model : nullptr;
}
bool Resolve(void* entity, Resolved& out) noexcept {
    const void* field{};
    uint32_t actorTag0{}, actorTag4{};
    if (!Read(entity, actorTag0) || !Offset(entity, 4, field) || !Read(field, actorTag4)) return false;
    const unsigned char** holder = nullptr;
    const unsigned char* body = nullptr;
    unsigned char* begin = nullptr;
    unsigned char* end = nullptr;

    if (!Offset(entity, 0x08, field) || !Read(field, holder) || !Read(holder, body)) return false;
    const Data::Model* model = Recognize(body);
    if (!model || !Offset(entity, 0x24, field) || !Read(field, begin)
        || !Offset(entity, 0x28, field) || !Read(field, end)) return false;
    const uintptr_t lo = reinterpret_cast<uintptr_t>(begin);
    const uintptr_t hi = reinterpret_cast<uintptr_t>(end);
    if (!lo || hi <= lo || hi - lo != model->nodes
        || !MemoryAccessible(begin, model->nodes, true)) return false;
    for (unsigned i = 0; i < model->opCount; ++i)
        if (Data::kOps[model->opStart + i].node >= model->nodes) return false;
    out = {entity, body, begin, end, model, actorTag0, actorTag4};
    return true;
}
bool Same(const Resolved& left, const Resolved& right) noexcept {
    return left.entity == right.entity && left.body == right.body
        && left.begin == right.begin && left.end == right.end && left.model == right.model
        && left.actorTag0 == right.actorTag0 && left.actorTag4 == right.actorTag4;
}
void Log(const Actor& actor, const char* reason) noexcept {
    if (!logger) return;
    char message[192]{};
    const char* state = actor.identity.model->schoolgirl
        ? (actor.transformed ? "battle" : "uniform")
        : (actor.transformed ? "broken" : "intact");
    std::snprintf(message, sizeof(message), "StateCostumes actor=%p %s state=%s reason=%s",
                  actor.identity.entity, actor.identity.model->name, state, reason);
    logger(message);
}
Actor* Existing(void* entity) noexcept {
    for (auto& actor : actors) if (actor.identity.entity == entity && entity) return &actor;
    return nullptr;
}
bool ProvenReplacement(const Actor& actor) noexcept {
    const void* field{};
    uint32_t tag0{},tag4{};
    if (!Read(actor.identity.entity,tag0)
        || !Offset(actor.identity.entity,4,field) || !Read(field,tag4)) return false;
    if (tag0!=actor.identity.actorTag0 || tag4!=actor.identity.actorTag4) return true;
    const unsigned char** holder=nullptr;
    const unsigned char* body=nullptr;
    if (!Offset(actor.identity.entity,8,field) || !Read(field,holder) || !Read(holder,body)) return false;

    return body!=actor.identity.body && MemoryReadable(body,0x60)
        && std::memcmp(body,"TMC\0",4)==0;
}
void RestoreRenderSnapshot(const Resolved& resolved, const RenderScope& scope) noexcept {
    for (unsigned i=0;i<resolved.model->opCount;++i) {
        const auto& op=Data::kOps[resolved.model->opStart+i];
        resolved.begin[op.node]=scope.baseline[op.node];
    }
    if (std::strcmp(resolved.model->name,"NYOTENGU_DLCU_028")==0 && scope.nodes>190)
        resolved.begin[190]=scope.baseline[190];
}
bool PendingRenderIdentityReplaced(const RenderScope& scope) noexcept {
    MEMORY_BASIC_INFORMATION region{};
    if (VirtualQuery(scope.entity,&region,sizeof region)==sizeof region
        && region.State==MEM_FREE) return true;
    Actor previous{};
    previous.identity={scope.entity,static_cast<const unsigned char*>(scope.body),scope.begin,
        scope.begin+scope.nodes,static_cast<const Data::Model*>(scope.model),scope.tag0,scope.tag4};
    if (ProvenReplacement(previous)) return true;

    const void* field{};unsigned char* begin=nullptr;unsigned char* end=nullptr;
    if (!Offset(scope.entity,0x24,field)||!Read(field,begin)
        ||!Offset(scope.entity,0x28,field)||!Read(field,end)) return false;
    const auto lo=reinterpret_cast<uintptr_t>(begin),hi=reinterpret_cast<uintptr_t>(end);
    return lo && hi>lo && hi-lo<=Data::kMaxNodes
        && (begin!=scope.begin || hi-lo!=scope.nodes)
        && MemoryAccessible(begin,hi-lo,true);
}
bool CompletePendingRenderRestore() noexcept {
    if (!pendingRenderRestore.active) return true;
    Resolved resolved{};
    if (Resolve(pendingRenderRestore.entity,resolved)) {
        if (ScopeMatches(resolved,pendingRenderRestore))
            RestoreRenderSnapshot(resolved,pendingRenderRestore);

        pendingRenderRestore=RenderScope{};
        return true;
    }
    if (PendingRenderIdentityReplaced(pendingRenderRestore)) {
        pendingRenderRestore=RenderScope{};
        return true;
    }
    return false;
}
void CollectConfirmedStale() noexcept {
    for (auto& actor : actors) {
        if (!actor.identity.entity) continue;
        MEMORY_BASIC_INFORMATION region{};
        if (VirtualQuery(actor.identity.entity, &region, sizeof(region)) == sizeof(region)
            && region.State == MEM_FREE) { actor = Actor{}; continue; }
        if (ProvenReplacement(actor)) { actor = Actor{}; continue; }
        Resolved current{};
        if (Resolve(actor.identity.entity, current) && !Same(actor.identity, current))
            actor = Actor{};
    }
}
void ApplySafeInitial(const Resolved& resolved) noexcept {

    if (DeferVisibility(resolved)) return;
    for (unsigned i = 0; i < resolved.model->opCount; ++i) {
        const auto& op = Data::kOps[resolved.model->opStart + i];
        if (op.initial <= 1) resolved.begin[op.node] = op.initial;
    }
    if (!overflowLogged) {
        overflowLogged = true;
        if (logger) logger("StateCostumes cache-full: safe initial form; new events disabled until ResetAll plus native flag reinitialization");
    }
}
Actor* Capture(const Resolved& resolved) noexcept {
    Actor* actor = Existing(resolved.entity);
    if (actor && Same(actor->identity, resolved)) return actor;
    if (admissionFrozen) return nullptr;

    if (!actor) {
        CollectConfirmedStale();
        for (auto& entry : actors) if (!entry.identity.entity) { actor = &entry; break; }
    }
    if (!actor) { admissionFrozen = true; return nullptr; }
    *actor = Actor{};
    actor->identity = resolved;

    std::memcpy(actor->native, resolved.begin, resolved.model->nodes);
    return actor;
}
void Apply(Actor& actor) noexcept {
    if (!IsTransitionEnabled(actor.identity.model->schoolgirl)) actor.transformed = false;
    actor.initialized = true;
    if (DeferVisibility(actor.identity)) return;
    if (!RenderOnlyRequested()) actor.offlineVisibilityDirty = true;
    const auto& model = *actor.identity.model;
    for (unsigned i = 0; i < model.opCount; ++i) {
        const auto& op = Data::kOps[model.opStart + i];
        const uint8_t mode = actor.transformed ? op.transformed : op.initial;
        actor.identity.begin[op.node] = mode == 2 ? actor.native[op.node] : mode;
    }

    if (std::strcmp(model.name, "NYOTENGU_DLCU_028") == 0 && model.nodes > 190)
        actor.identity.begin[190] = actor.transformed ? 1 : 0;
}
bool Activate(void* entity, bool schoolgirl, bool newOnly = false) noexcept {
    if (!IsTransitionEnabled(schoolgirl)) return false;
    TryLock guard;
    if (!guard.held) return false;
    if (!CompletePendingRenderRestore()) return false;
    Resolved resolved{};

    if (!Resolve(entity, resolved)) return false;

    if (resolved.model->schoolgirl != schoolgirl) return false;
    Actor* actor = Capture(resolved);
    if (!actor) { ApplySafeInitial(resolved); return false; }
    const bool change = !actor->initialized || !actor->transformed;
    actor->transformed = true;
    Apply(*actor);
    if (change) Log(*actor, schoolgirl ? "actor-transform" : "confirmed-victim-hit");
    return newOnly ? change : true;
}
void RestoreOfflineVisibility(Actor& actor, const Resolved& current) noexcept {

    for (unsigned i = 0; i < current.model->opCount; ++i) {
        const auto& op = Data::kOps[current.model->opStart + i];
        current.begin[op.node] = actor.native[op.node];
    }
    if (std::strcmp(current.model->name, "NYOTENGU_DLCU_028") == 0 && current.model->nodes > 190)
        current.begin[190] = actor.native[190];
    actor.offlineVisibilityDirty = false;
}
void ResetEntry(Actor& actor, bool retainKnownBaseline = false) noexcept {
    Resolved current{};
    const bool resolved = actor.identity.entity && Resolve(actor.identity.entity, current);
    if (!resolved && actor.identity.entity && retainKnownBaseline) {

        actor.transformed = false;
        actor.initialized = false;
        return;
    }
    if (resolved && Same(actor.identity, current)) {
        if (!RenderOnlyRequested() || actor.offlineVisibilityDirty)
            RestoreOfflineVisibility(actor, current);
        Log(actor, "reset-native-baseline");
        if (retainKnownBaseline) {

            actor.transformed = false;
            actor.initialized = false;
            return;
        }
    }
    actor = Actor{};
}
}
void SetRenderOnlyPredicate(RenderOnlyPredicate predicate) noexcept {

    renderOnlyPredicate = predicate;
}
bool BeginRenderScope(void* entity, RenderScope& scope) noexcept {
    if (currentRenderScope || scope.active) return false;
    TryLock guard;
    if (!guard.held) return false;
    if (!CompletePendingRenderRestore() || !RenderOnlyRequested()) return false;
    Resolved resolved{};
    if (!Resolve(entity, resolved) || resolved.model->nodes > sizeof scope.baseline) return false;

    if (Actor* actor = Existing(entity); actor && Same(actor->identity, resolved)
        && actor->offlineVisibilityDirty) RestoreOfflineVisibility(*actor, resolved);
    scope.entity=resolved.entity; scope.body=resolved.body; scope.begin=resolved.begin;
    scope.model=resolved.model; scope.nodes=resolved.model->nodes;
    scope.tag0=resolved.actorTag0; scope.tag4=resolved.actorTag4;
    std::memcpy(scope.baseline, resolved.begin, scope.nodes);
    scope.active=true; currentRenderScope=&scope;
    return true;
}
bool EndRenderScope(RenderScope& scope) noexcept {
    if (!scope.active || currentRenderScope != &scope) return false;

    Resolved resolved{};
    const bool valid=Resolve(scope.entity,resolved) && ScopeMatches(resolved,scope);
    if (valid) RestoreRenderSnapshot(resolved,scope);
    else pendingRenderRestore=scope;
    currentRenderScope=nullptr; scope.active=false;
    return valid;
}
void SetLogger(Logger target) noexcept {
    TryLock guard;
    if (guard.held) logger = target;
}
bool OnBeforeRender(void* entity) noexcept {
    TryLock guard;
    if (!guard.held) return false;
    if (!CompletePendingRenderRestore()) return false;
    Resolved resolved{};
    if (!Resolve(entity, resolved)) return false;
    Actor* actor = Capture(resolved);
    if (!actor) { ApplySafeInitial(resolved); return true; }
    const bool first = !actor->initialized;

    static DWORD skipRead = 0; static bool skipApply = false;

    static int forceNode[2] = {-1, -1}, forceVal[2] = {0, 0}; static int lastLogged[4] = {-2, -2, -2, -2};
    const DWORD now = GetTickCount();
    if (!skipRead || now - skipRead > 2000) {
        skipRead = now ? now : 1; char ini[MAX_PATH]{}; GetModuleFileNameA(nullptr, ini, MAX_PATH);
        if (std::strrchr(ini, 92)) {
            std::snprintf(ini, MAX_PATH, "%s", Extra_Ini()); skipApply = GetPrivateProfileIntA("StatefulCostumesDebug", "SkipApply", 0, ini) != 0;
            forceNode[0] = GetPrivateProfileIntA("StatefulCostumesDebug", "ForceA", -1, ini); forceVal[0] = GetPrivateProfileIntA("StatefulCostumesDebug", "ForceAVal", 0, ini);
            forceNode[1] = GetPrivateProfileIntA("StatefulCostumesDebug", "ForceB", -1, ini); forceVal[1] = GetPrivateProfileIntA("StatefulCostumesDebug", "ForceBVal", 0, ini);
        }
    }
    if (skipApply) {
        if (first && logger) { char m[160]{}; std::snprintf(m, sizeof m, "StateCostumes SkipApply: aucune ecriture pour %s", resolved.model->name); logger(m); }
        actor->initialized = true;
    } else Apply(*actor);

    if (!RenderOnlyRequested()) for (int k = 0; k < 2; ++k)
        if (forceNode[k] >= 0 && unsigned(forceNode[k]) < resolved.model->nodes) resolved.begin[forceNode[k]] = uint8_t(forceVal[k]);
    if (logger && (lastLogged[0] != forceNode[0] || lastLogged[1] != forceVal[0] || lastLogged[2] != forceNode[1] || lastLogged[3] != forceVal[1])) {
        lastLogged[0] = forceNode[0]; lastLogged[1] = forceVal[0]; lastLogged[2] = forceNode[1]; lastLogged[3] = forceVal[1];
        char m[160]{}; std::snprintf(m, sizeof m, "StateCostumes Force %s: A=%d:%d B=%d:%d skip=%u", resolved.model->name, forceNode[0], forceVal[0], forceNode[1], forceVal[1], unsigned(skipApply)); logger(m);
    }
    if (skipApply) return true;
    if (first) {
        Log(*actor, "initialize");

        if (logger) {
            char message[192]{}; int n = std::snprintf(message, sizeof(message), "StateCostumes native %s:", resolved.model->name);
            const unsigned lo = resolved.model->opStart, hi = lo + resolved.model->opCount;
            for (unsigned i = lo; i < hi && n > 0 && n < int(sizeof(message)) - 24; ++i)
                n += std::snprintf(message + n, sizeof(message) - n, " n%u=%02X/%u", Data::kOps[i].node, actor->native[Data::kOps[i].node], Data::kOps[i].initial);
            logger(message);
            unsigned counts[256]{};
            for (unsigned i = 0; i < resolved.model->nodes; ++i) ++counts[actor->native[i]];
            n = std::snprintf(message, sizeof(message), "StateCostumes native histogram %s (%u noeuds):", resolved.model->name, unsigned(resolved.model->nodes));
            for (unsigned v = 0; v < 256 && n > 0 && n < int(sizeof(message)) - 16; ++v)
                if (counts[v]) n += std::snprintf(message + n, sizeof(message) - n, " %02X:%u", v, counts[v]);
            logger(message);
        }
    }
    return true;
}
bool OnConfirmedHit(void* victim) noexcept { return Activate(victim, false); }
bool OnSchoolgirlTransform(void* actor) noexcept { return Activate(actor, true); }
bool OnTaunt(void* actor) noexcept { return OnSchoolgirlTransform(actor); }
bool OnNewSchoolgirlTransform(void* actor) noexcept { return Activate(actor, true, true); }
bool OnNewConfirmedHit(void* actor) noexcept { return Activate(actor, false, true); }

bool OnToggleSchoolgirl(void* entity) noexcept {
    if (!IsTransitionEnabled(true)) return false;
    TryLock guard;
    if (!guard.held) return false;
    if (!CompletePendingRenderRestore()) return false;
    Resolved resolved{};
    if (!Resolve(entity, resolved) || !resolved.model->schoolgirl) return false;
    Actor* actor = Capture(resolved);
    if (!actor) { ApplySafeInitial(resolved); return false; }
    actor->transformed = !actor->transformed;
    Apply(*actor);
    Log(*actor, actor->transformed ? "ps4-d1-transform" : "ps4-d1-revert");
    return true;
}
bool RevertForm(void* entity, bool transformed) noexcept {
    TryLock guard;
    if (!guard.held) return false;
    if (!CompletePendingRenderRestore()) return false;
    Resolved resolved{};
    if (!Resolve(entity, resolved)) return false;
    Actor* actor = Capture(resolved);
    if (!actor) return false;
    if (actor->initialized && actor->transformed == transformed) return true;
    actor->transformed = transformed;
    Apply(*actor);
    Log(*actor, "rollback-revert");
    return true;
}
bool IsSchoolgirl(void* entity) noexcept {
    TryLock guard;
    if (!guard.held) return false;
    Resolved resolved{};
    return Resolve(entity, resolved) && resolved.model->schoolgirl;
}

const char* ModelName(void* entity) noexcept {
    TryLock guard;
    if (!guard.held) return nullptr;
    Resolved resolved{};
    return Resolve(entity, resolved) ? resolved.model->name : nullptr;
}
bool IsTransformed(void* entity) noexcept {
    TryLock guard;
    if (!guard.held) return false;
    Resolved resolved{};
    if (!Resolve(entity, resolved)) return false;
    Actor* actor = Existing(resolved.entity);
    return actor && actor->transformed;
}
void ResetActor(void* actor) noexcept {
    TryLock guard;
    if (guard.held && CompletePendingRenderRestore()) if (auto* entry = Existing(actor)) ResetEntry(*entry);
}
bool ResetForRound(bool keepBroken) noexcept {
    TryLock guard;
    if (!guard.held) return false;
    if (!CompletePendingRenderRestore()) return false;
    for (auto& actor : actors) {

        if (keepBroken && actor.identity.entity && actor.transformed) {
            actor.initialized = false;
            Log(actor, "keep-broken-next-round");
            continue;
        }
        ResetEntry(actor, true);
    }
    return true;
}
void ResetAll() noexcept {
    TryLock guard;
    if (guard.held && CompletePendingRenderRestore()) {
        for (auto& actor : actors) ResetEntry(actor);
        admissionFrozen = false;
        overflowLogged = false;
    }
}
}

namespace StateCostumes {
int ClassifyCostume(const char* chara, unsigned number) noexcept {
    if (!chara || number > 999) return 0;
    char name[64];
    std::snprintf(name, sizeof name, "%s_DLCU_%03u", chara, number);
    for (const auto& model : Data::kModels)
        if (std::strcmp(model.name, name) == 0) return model.schoolgirl ? 2 : 1;
    return 0;
}
}
