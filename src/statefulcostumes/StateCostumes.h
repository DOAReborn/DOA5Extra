#pragma once
#include <cstdint>

namespace StateCostumes {

using RenderOnlyPredicate = bool (*)() noexcept;
void SetRenderOnlyPredicate(RenderOnlyPredicate predicate) noexcept;
struct RenderScope {
    void* entity{};
    const void* body{};
    unsigned char* begin{};
    const void* model{};
    std::uint32_t tag0{}, tag4{};
    unsigned nodes{};
    unsigned char baseline[1024]{};
    bool active{};
};

bool BeginRenderScope(void* entity, RenderScope& scope) noexcept;

bool EndRenderScope(RenderScope& scope) noexcept;

bool IsTransitionEnabled(bool schoolgirl) noexcept;
void SetTransitionEnabled(bool schoolgirl, bool enabled) noexcept;
bool SaveTransitionOptions() noexcept;
using Logger = void (*)(const char* message);
void SetLogger(Logger logger) noexcept;

bool OnBeforeRender(void* entity) noexcept;

bool OnConfirmedHit(void* victim) noexcept;

bool OnSchoolgirlTransform(void* actor) noexcept;
bool OnTaunt(void* actor) noexcept;

bool OnNewSchoolgirlTransform(void* actor) noexcept;
bool OnNewConfirmedHit(void* actor) noexcept;

void ResetActor(void* actor) noexcept;

bool ResetForRound(bool keepBroken = false) noexcept;
void ResetAll() noexcept;
bool OnToggleSchoolgirl(void* entity) noexcept;

bool RevertForm(void* entity, bool transformed) noexcept;
bool IsSchoolgirl(void* entity) noexcept;
const char* ModelName(void* entity) noexcept;
bool IsTransformed(void* entity) noexcept;

int ClassifyCostume(const char* chara, unsigned number) noexcept;
}
