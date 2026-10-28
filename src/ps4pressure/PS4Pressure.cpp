#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "../commun/journal_extra.h"
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <cmath>
#include <cstdlib>
#include <initializer_list>
static_assert(sizeof(void*)==4,"x86 only");

static BYTE* Base;
static char IniPath[MAX_PATH],LogPath[MAX_PATH],LrIni[MAX_PATH];
static BYTE* VA(DWORD va){return Base+(va-0x400000);}
static volatile LONG LrOn;
static bool AllModes;
static float K=0.01f,D=0.05f,Smooth=0.2f,ContactEps=0.01f;

static void Log(const char* fmt,...){
    va_list a; va_start(a,fmt); JournalExtra_V("PS4Pressure",fmt,a); va_end(a);
}
static bool Same(BYTE* p,const char* hex){
    BYTE b[16]; size_t n=strlen(hex)/2; SIZE_T got=0;
    if(n>sizeof b||!ReadProcessMemory(GetCurrentProcess(),p,b,n,&got)||got!=n) return false;
    for(size_t i=0;i<n;i++){unsigned v; sscanf(hex+2*i,"%2x",&v); if(b[i]!=(BYTE)v) return false;}
    return true;
}

struct Side{BYTE* rec; float accX,accY,oldX,oldY,tX,tY; bool pending; DWORD seen;};
static Side Sides[16];
static Side* Slot(BYTE* rec){
    Side* oldest=&Sides[0];
    for(auto& s:Sides){ if(s.rec==rec) return &s; if(s.seen<oldest->seen) oldest=&s; }
    memset(oldest,0,sizeof *oldest); oldest->rec=rec; return oldest;
}
static DWORD Frame;
static volatile LONG DiagLeft;
extern "C" void __cdecl PrePressure(BYTE* ebp){
    if(!LrOn) return;
    BYTE* rec=*(BYTE**)(ebp-0x64); if(!rec) return;
    Side* s=Slot(rec); s->seen=++Frame;
    s->oldX=*(float*)(rec+0x38); s->oldY=*(float*)(rec+0x3C);
    s->tX=*(float*)(ebp-0x98); s->tY=*(float*)(ebp-0x94); s->pending=true;
}

static float Clamp(float v){ if(!(v==v)) return 0; return v<0?0:v>1?1:v; }
extern "C" void __cdecl PostPressure(BYTE* ebp){
    if(!LrOn) return;
    BYTE* rec=*(BYTE**)(ebp-0x64); if(!rec) return;
    Side* s=Slot(rec); if(!s->pending) return; s->pending=false;
    float x,y;
    if((s->tX>s->tY?s->tX:s->tY)>=ContactEps){
        s->accX=s->accY=0;
        x=s->oldX+(s->tX-s->oldX)*Smooth; y=s->oldY+(s->tY-s->oldY)*Smooth;
    }else{
        s->accX+=(s->tX-s->oldX)*K; s->accY+=(s->tY-s->oldY)*K;
        x=(s->oldX+s->accX)*(1-D); y=(s->oldY+s->accY)*(1-D);
    }
    if(DiagLeft>0){ InterlockedDecrement(&DiagLeft);
        Log("diag t=%lu rec=%p cible=(%.4f %.4f) avant=(%.4f %.4f) natif=(%.4f %.4f) ps4=(%.4f %.4f) acc=(%.4f %.4f) %s",
            GetTickCount(),rec,s->tX,s->tY,s->oldX,s->oldY,*(float*)(rec+0x38),*(float*)(rec+0x3C),Clamp(x),Clamp(y),s->accX,s->accY,
            ((s->tX>s->tY?s->tX:s->tY)>=ContactEps)?"contact":"ressort"); }
    *(float*)(rec+0x38)=Clamp(x); *(float*)(rec+0x3C)=Clamp(y);
}

static BYTE* MakeStub(void* fn,const BYTE* orig,size_t n,BYTE* back){
    BYTE* t=(BYTE*)VirtualAlloc(nullptr,256,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE); if(!t) return nullptr;
    size_t k=0; auto B=[&](std::initializer_list<int> l){for(int v:l)t[k++]=(BYTE)v;};
    B({0x60,0x9C,0x81,0xEC,0x80,0,0,0});
    for(int r=0;r<8;r++){ if(r==0) B({0xF3,0x0F,0x7F,0x04,0x24}); else B({0xF3,0x0F,0x7F,0x44+r*8,0x24,r*0x10}); }
    B({0x55,0xE8}); *(DWORD*)(t+k)=(DWORD)fn-(DWORD)(t+k+4); k+=4; B({0x83,0xC4,0x04});
    for(int r=0;r<8;r++){ if(r==0) B({0xF3,0x0F,0x6F,0x04,0x24}); else B({0xF3,0x0F,0x6F,0x44+r*8,0x24,r*0x10}); }
    B({0x81,0xC4,0x80,0,0,0,0x9D,0x61});
    memcpy(t+k,orig,n); k+=n;
    B({0xE9}); *(DWORD*)(t+k)=(DWORD)back-(DWORD)(t+k+4); k+=4;
    FlushInstructionCache(GetCurrentProcess(),t,k); return t;
}
static bool Hook(DWORD va,const char* hex,size_t n,void* fn){
    BYTE* p=VA(va); if(!Same(p,hex)){Log("octets inattendus a %08lX, rien modifie",va); return false;}
    BYTE orig[16]; memcpy(orig,p,n);
    BYTE* stub=MakeStub(fn,orig,n,p+n); if(!stub) return false;
    BYTE patch[16]; patch[0]=0xE9; *(DWORD*)(patch+1)=(DWORD)stub-(DWORD)(p+5); for(size_t i=5;i<n;i++) patch[i]=0x90;
    DWORD old; if(!VirtualProtect(p,n,PAGE_EXECUTE_READWRITE,&old)) return false;
    memcpy(p,patch,n); VirtualProtect(p,n,old,&old); FlushInstructionCache(GetCurrentProcess(),p,n); return true;
}
static float IniF(const char* k,float d){char b[32],dd[32]; snprintf(dd,sizeof dd,"%g",d); GetPrivateProfileStringA("PS4Pressure",k,dd,b,sizeof b,IniPath); return (float)atof(b);}

static DWORD WINAPI Worker(LPVOID){
    if(Extra_SepareCharge("DOA5LR-PS4Pressure.asi")){Log("ancien DOA5LR-PS4Pressure.asi charge : rien modifie"); return 0;}
    BYTE* probe=VA(0x4C450C);
    for(int i=0;i<600&&!Same(probe,"8b459cf30f104038");i++) Sleep(100);
    if(!Same(VA(0x4C450C),"8b459cf30f104038")||!Same(VA(0x4C46CF),"8b45a0408945a0")){Log("code du jeu non reconnu (version differente ?) : rien modifie"); return 0;}
    AllModes=GetPrivateProfileIntA("PS4Pressure","AllModes",1,IniPath)!=0;
    DiagLeft=GetPrivateProfileIntA("PS4Pressure","DiagFrames",0,IniPath);
    K=IniF("Spring",0.01f); D=IniF("Damping",0.05f); Smooth=IniF("Smooth",0.2f); ContactEps=IniF("ContactThreshold",0.01f);
    bool a=Hook(0x4C46CF,"8b45a0408945a0",7,(void*)PostPressure);
    bool b=a&&Hook(0x4C450C,"8b459cf30f104038",8,(void*)PrePressure);
    Log("ecrasement PS4 (ressort %g, amortissement %g, lissage %g, seuil contact %g) : %s",K,D,Smooth,ContactEps,(a&&b)?"installe":"echec");
    LONG last=-1;
    for(;;){
        LONG on=AllModes||(GetPrivateProfileIntA("DOA5Extra","SoftEngineLR",1,LrIni)&&GetPrivateProfileIntA("SoftEngineLR","LR",0,LrIni));
        if(on!=last){ LrOn=on; last=on; Log("%s",on?(AllModes?"tous les modes : ecrasement PS4":"mode LR : ecrasement PS4"):"mode PC : ecrasement natif"); }
        Sleep(1000);
    }
}
extern "C" void PS4Pressure_Demarrer(void){
    Base=(BYTE*)GetModuleHandleA(nullptr);
    snprintf(IniPath,sizeof IniPath,"%s",Extra_Ini()); (void)LogPath;

    strcpy(LrIni,IniPath);
    if(HANDLE t=CreateThread(nullptr,0,Worker,nullptr,0,nullptr)) CloseHandle(t);
}
