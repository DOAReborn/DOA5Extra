#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "../commun/journal_extra.h"
#include <cstdio>
#include <cstring>
#include <cstdarg>
static_assert(sizeof(void*)==4,"x86 only");

static BYTE* Base;
static char IniPath[MAX_PATH],LogPath[MAX_PATH];
static BYTE* VA(DWORD va){return Base+(va-0x400000);}

static void Log(const char* fmt,...){
    va_list a; va_start(a,fmt); JournalExtra_V("BustPress",fmt,a); va_end(a);
}

static bool Same(BYTE* p,const char* hex){
    BYTE b[16]; size_t n=strlen(hex)/2; SIZE_T got=0;
    if(n>sizeof b||!ReadProcessMemory(GetCurrentProcess(),p,b,n,&got)||got!=n) return false;
    for(size_t i=0;i<n;i++){unsigned v; sscanf(hex+2*i,"%2x",&v); if(b[i]!=(BYTE)v) return false;}
    return true;
}
static const char* NOP6="660f1f440000";
static void NopJe(DWORD va,const char* orig,const char* what){
    BYTE* p=VA(va);
    if(Same(p,NOP6)){Log("%s : deja actif",what); return;}
    if(!Same(p,orig)){Log("%s : octets inattendus a %08lX, rien modifie",what,va); return;}
    static const BYTE nop[6]={0x66,0x0F,0x1F,0x44,0x00,0x00};
    DWORD old; bool ok=VirtualProtect(p,6,PAGE_EXECUTE_READWRITE,&old)!=0;
    if(ok){memcpy(p,nop,6); VirtualProtect(p,6,old,&old); FlushInstructionCache(GetCurrentProcess(),p,6);}
    Log("%s : %s",what,ok?"active":"echec VirtualProtect");
}
static DWORD WINAPI Worker(LPVOID){
    if(Extra_SepareCharge("DOA5LR-BustPress.asi")){Log("ancien DOA5LR-BustPress.asi charge : rien modifie"); return 0;}

    BYTE* probe=VA(0x4C3135);
    for(int i=0;i<600&&!Same(probe,"0f8475100000")&&!Same(probe,NOP6);i++) Sleep(100);
    if(!Same(probe,"0f8475100000")&&!Same(probe,NOP6)){Log("code du jeu non reconnu (version differente ?) : rien modifie"); return 0;}
    if(GetPrivateProfileIntA("BustPress","AllCostumes",1,IniPath)) NopJe(0x4C3135,"0f8475100000","ecrasement poitrine (sol + contacts) pour tous les costumes");
    if(GetPrivateProfileIntA("BustPress","AllModes",1,IniPath))    NopJe(0x4C3694,"0f84160b0000","ecrasement au sol dans tous les modes");
    return 0;
}
extern "C" void BustPress_Demarrer(void){
    Base=(BYTE*)GetModuleHandleA(nullptr);
    snprintf(IniPath,sizeof IniPath,"%s",Extra_Ini()); (void)LogPath;
    if(HANDLE t=CreateThread(nullptr,0,Worker,nullptr,0,nullptr)) CloseHandle(t);
}
