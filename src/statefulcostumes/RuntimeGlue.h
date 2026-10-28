#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace RuntimeGlue {
using Logger = void (*)(const char*);
void EnableNativeTransitionBurst(bool enabled) noexcept;
void EnableNativeTransitionSound(bool enabled) noexcept;
void EnableOriginalSchoolgirlParticles(bool enabled) noexcept;
void EnableOnlineCostumeTrial(bool enabled, bool rollbackDisabled) noexcept;

void EnableRollbackCostumes(bool enabled, bool laboTest = false) noexcept;
bool Prepare(BYTE* imageBase, Logger logger) noexcept;
bool ArmRenderer() noexcept;
bool RendererArmed() noexcept;

bool ActivateControlBeforeMount() noexcept;
bool ValidateAlternateTables() noexcept;
bool InstallAlternateTables() noexcept;
void RollbackAlternateTables() noexcept;
void FlushLogs() noexcept;

bool EventSelfTest() noexcept;
#ifdef PREFLIGHT_TOOL
bool ReloadAliasSelfTest() noexcept;

bool IntegratedEventSelfTest(const char* convertedRoot) noexcept;
bool OnlineTrialSelfTest(const char* convertedRoot) noexcept;
#endif
}
