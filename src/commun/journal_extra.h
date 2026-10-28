#pragma once
#include <windows.h>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include "extra.h"

inline void JournalExtra_Masquer(char* s)
{
    char* w = s;
    for (const char* r = s; *r;) {
        const char* d = r; bool prefixe = false;
        if (r[0] == '0' && (r[1] == 'x' || r[1] == 'X')) { d = r + 2; prefixe = true; }
        const char* e = d; bool lettre = false;
        while (isxdigit((unsigned char)*e)) { if (!isdigit((unsigned char)*e)) lettre = true; e++; }
        const size_t n = size_t(e - d);
        if (n && (prefixe || n >= 8 || (n >= 6 && lettre))) { *w++ = '?'; r = e; continue; }
        if (n) { while (r < e) *w++ = *r++; continue; }
        *w++ = *r++;
    }
    *w = 0;
}

inline bool Extra_SepareCharge(const char* nom)
{
    Sleep(1000);
    return GetModuleHandleA(nom) != nullptr;
}
inline void JournalExtra_Ligne(const char* module, const char* ligne)
{
    char m[1024];
    snprintf(m, sizeof m, "%s", ligne);
    JournalExtra_Masquer(m);
    Extra_Journal(module, m);
}
inline void JournalExtra_V(const char* module, const char* fmt, va_list a)
{
    char m[1024];
    vsnprintf(m, sizeof m, fmt, a);
    JournalExtra_Masquer(m);
    Extra_Journal(module, m);
}
