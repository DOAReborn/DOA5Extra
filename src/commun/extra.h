#ifndef DOA5EXTRA_EXTRA_H
#define DOA5EXTRA_EXTRA_H
#include <windows.h>
#ifdef __cplusplus
extern "C" {
#endif

void Extra_Journal(const char *module, const char *ligne);

const char *Extra_Ini(void);
const wchar_t *Extra_IniW(void);

void Punish_Demarrer(HINSTANCE module);
void Hitbox_Demarrer(void);
void OpenBurst_Demarrer(void);
void StatefulCostumes_Demarrer(void);
void BustPress_Demarrer(void);
void PS4Pressure_Demarrer(void);
void ConsoleMusic_Demarrer(void);
void SoftBody_Demarrer(void);
void SoftEngineLR_Demarrer(void);
void SkinSSS_Demarrer(void);

#ifdef __cplusplus
}
#endif
#endif
