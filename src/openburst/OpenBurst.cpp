#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <cstdio>
#include <cstring>
#include <vector>
#include "PS4Burst.h"
#include "HitFXMerge.h"
#ifdef DOA5EXTRA
#include "../commun/journal_extra.h"
#endif
static_assert(sizeof(void*)==4,"DOA5LR requires x86");
static BYTE* base;
static char logPath[MAX_PATH];
static bool resourcesReady;
static DWORD textures[3];
static void* G(DWORD va){return base+va-0x400000;}
#ifdef DOA5EXTRA
static unsigned logLines;
static void log(const char* s){if(++logLines>300)return;JournalExtra_Ligne("OpenBurst",logLines==300?"Log limit reached":s);}
#else
static void log(const char* s){FILE*f=fopen(logPath,"a");if(f){fprintf(f,"%s\n",s);fclose(f);}}
#endif
static bool accessible(const void* p,size_t n,bool write=false){
 uintptr_t a=(uintptr_t)p,end=a+n;if(!p||end<a)return false;
 while(a<end){MEMORY_BASIC_INFORMATION m{};if(!VirtualQuery((void*)a,&m,sizeof m)||m.State!=MEM_COMMIT||(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;
  if(write&&!(m.Protect&(PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY)))return false;
  uintptr_t next=(uintptr_t)m.BaseAddress+m.RegionSize;if(next<=a)return false;a=next;
 }return true;
}
struct Patch {BYTE* at;std::vector<BYTE> before,after;};
static bool writeBytes(BYTE* p,const std::vector<BYTE>& b){DWORD old;if(!VirtualProtect(p,b.size(),PAGE_EXECUTE_READWRITE,&old))return false;memcpy(p,b.data(),b.size());DWORD ignored;VirtualProtect(p,b.size(),old,&ignored);FlushInstructionCache(GetCurrentProcess(),p,b.size());return true;}
static bool commit(const std::vector<Patch>& patches){
 std::vector<HANDLE> threads;HANDLE snap=CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD,0);if(snap==INVALID_HANDLE_VALUE)return false;
 THREADENTRY32 e{};e.dwSize=sizeof e;bool ok=true;
 if(Thread32First(snap,&e))do{if(e.th32OwnerProcessID!=GetCurrentProcessId()||e.th32ThreadID==GetCurrentThreadId())continue;
  HANDLE h=OpenThread(THREAD_SUSPEND_RESUME|THREAD_GET_CONTEXT|THREAD_QUERY_INFORMATION,FALSE,e.th32ThreadID);
  if(h)threads.push_back(h);else if(GetLastError()!=ERROR_INVALID_PARAMETER){ok=false;break;}
 }while(Thread32Next(snap,&e));
 CloseHandle(snap);
 std::vector<bool> suspended(threads.size(),false);size_t done=0;
 if(ok)for(size_t i=0;i<threads.size();i++){
  if(SuspendThread(threads[i])==DWORD(-1)){ok=false;break;}suspended[i]=true;
  CONTEXT c{};c.ContextFlags=CONTEXT_CONTROL;if(!GetThreadContext(threads[i],&c)){ok=false;break;}
  for(auto&p:patches)if(c.Eip>=(DWORD)p.at&&c.Eip<(DWORD)p.at+p.before.size())ok=false;if(!ok)break;
 }
 if(ok)for(auto&p:patches)if(!accessible(p.at,p.before.size())||memcmp(p.at,p.before.data(),p.before.size()))ok=false;
 if(ok)for(auto&p:patches){if(!writeBytes(p.at,p.after)){ok=false;break;}done++;}
 if(!ok)while(done){auto&p=patches[--done];writeBytes(p.at,p.before);}
 for(size_t i=0;i<threads.size();i++){if(suspended[i])ResumeThread(threads[i]);CloseHandle(threads[i]);}return ok;
}
struct Slot {void* actor=nullptr;cb::PS4Burst effect;float basis[16]{};DWORD texture[3]{};unsigned frames=0,attempted=0,accepted=0,poolSkipped=0;};
static Slot slots[16];static unsigned serial=0;
static Slot* find(void* actor){for(auto&s:slots)if(s.actor==actor)return &s;return nullptr;}
static unsigned reports;
static void report(Slot&s,const char* reason){
 if(++reports>32)return;
 char msg[240];std::snprintf(msg,sizeof msg,"CB result: %s age=%.3f renders=%u quads=%u accepted=%u pool_skipped=%u",reason,s.effect.age,s.frames,s.attempted,s.accepted,s.poolSkipped);log(msg);
}
static void __cdecl common(BYTE* tmc,BYTE* tmcl){
 resourcesReady=false;for(auto&s:slots)s.actor=nullptr;
 auto a=cb::texture(0),b=cb::texture(1),c=cb::texture(2);
 std::array<hitfx::Bytes,3> payload{{{a.data(),a.size()},{b.data(),b.size()},{c.data(),c.size()}}};
 hitfx::MergeResult result{};hitfx::MergeDiagnostics diagnostics{};const char* error=nullptr;
 bool pools=accessible(tmc,0x800000,true)&&accessible(tmcl,0x800000,true);
 bool merged=pools&&hitfx::AppendImpactTextures(tmc,0x800000,tmcl,0x800000,payload,&result,&error,0,&diagnostics);
 if(diagnostics.available){
  char msg[320];std::snprintf(msg,sizeof msg,"COMMON raw: base=%u payload=%u total=%u header=%08x,%08x,%08x mirror=%08x,%08x,%08x",diagnostics.rawBase,diagnostics.rawPayload,diagnostics.rawHeader[1],diagnostics.rawHeader[0],diagnostics.rawHeader[1],diagnostics.rawHeader[2],diagnostics.mirror[0],diagnostics.mirror[1],diagnostics.mirror[2]);log(msg);
 }
 ((void(__cdecl*)(BYTE*,BYTE*))G(0x845070))(tmc,tmcl);
 if(merged){for(unsigned i=0;i<3;i++)textures[i]=result.addedIndices[i];resourcesReady=true;char msg[180];std::snprintf(msg,sizeof msg,"READY: three procedural textures; count=%u->%u; no external runtime",result.previousCount,result.textureCount);log(msg);}
 else log(error?error:(pools?"Common merge refused; native effects retained":"Common pools inaccessible; native effects retained"));
}
static void __cdecl impact(DWORD* ctx,void* bank,DWORD first,DWORD last){
 ((void(__cdecl*)(void*,void*,DWORD,DWORD))G(0x8a4740))(ctx,bank,first,last);
 if(!resourcesReady||first||last!=1||!accessible(ctx,0x60)||ctx[4]!=1||ctx[5]!=6||ctx[7]<24)return;
 void* actor=(void*)(ctx[7]-24);
 if(!accessible(actor,44)||*(DWORD*)actor!=(DWORD)G(0xfdae34))return;
 Slot*s=find(actor);if(!s)for(auto&v:slots)if(!v.actor){s=&v;break;}if(!s)return;
 DWORD handles[3]{};
 for(unsigned i=0;i<3;i++){if(textures[i]<ctx[5])return;handles[i]=((DWORD(__thiscall*)(void*,DWORD))G(0x4227f0))(ctx,textures[i]-ctx[5]);if(handles[i]<13)return;}
 auto position=(float*)ctx;for(unsigned i=0;i<3;i++)if(!std::isfinite(position[i]))return;
 for(unsigned i=8;i<24;i++)if(!std::isfinite(position[i]))return;

 s->actor=actor;s->frames=s->attempted=s->accepted=s->poolSkipped=0;
 memcpy(s->basis,ctx+8,64);s->effect.start({position[0],position[1],position[2]},++serial,s->basis);memcpy(s->texture,handles,12);
 if(serial<=16){char msg[240];std::snprintf(msg,sizeof msg,"CB created: pos=%.3f,%.3f,%.3f basis=%.3f,%.3f,%.3f / %.3f,%.3f,%.3f",position[0],position[1],position[2],s->basis[0],s->basis[1],s->basis[2],s->basis[4],s->basis[5],s->basis[6]);log(msg);}
}
static void* __fastcall destroy(void* actor,void*,unsigned flags){if(auto*s=find(actor)){report(*s,"actor destroyed");s->actor=nullptr;}return ((void*(__thiscall*)(void*,unsigned))G(0x53db00))(actor,flags);}
static void __fastcall update(void* actor,void*,DWORD tick){
 ((void(__thiscall*)(void*,DWORD))G(0x57edd0))(actor,tick);
 if(auto*s=find(actor)){

  if(accessible((void*)tick,12)){int step=*((int*)tick+2);if(step>=0&&step<=600)s->effect.advance(float(step)/60);}
  if(s->effect.alive())*((DWORD*)actor+1)=1;else{report(*s,"completed");s->actor=nullptr;}}
}
static void __fastcall render(void* actor,void*,DWORD pass){
 ((void(__thiscall*)(void*,DWORD))G(0x8a5d70))(actor,pass);
 Slot*s=find(actor);if(!s||!*(DWORD*)G(0x12c8f98))return;s->frames++;
 const float* camera=((const float*(__cdecl*)())G(0x8557e0))();
 const float* cameraUp=((const float*(__cdecl*)())G(0x8557f0))();
 if(!accessible(camera,12)||!accessible(cameraUp,12))return;
 cb::Vec view{camera[0],camera[1],camera[2]},up{cameraUp[0],cameraUp[1],cameraUp[2]};
 if(s->frames==1&&serial<=16){char msg[180];BYTE*context=*(BYTE**)G(0x12c8f98);std::snprintf(msg,sizeof msg,"CB first render: age=%.3f context_alpha=%.3f",s->effect.age,accessible(context,0x6c)?*(float*)(context+0x68):-1.f);log(msg);}
 s->effect.draw([&](cb::Quad q){

  DWORD pool=*(DWORD*)G(0x24809fc),end=*(DWORD*)G(0x2480a00);
  if(!pool||end<pool||end-pool<512){s->poolSkipped++;return;}
  float m[16]{};

  if(!cb::billboard(q.center,view,up,q.rotation,q.width,q.height,m))return;

  float center[4]={m[12],m[13],m[14],1};struct Color{DWORD rgb;float alpha;}color{0xffffff,q.alpha};
  DWORD material[2]={s->texture[q.texture],0x10000};alignas(16) BYTE output[0x44]{};
  s->attempted++;
  ((void*(__thiscall*)(void*,void*,void*,void*,void*))G(0x41ea00))(material,output,center,m,&color);
  if(*(DWORD*)(output+0x38)==0)return;s->accepted++;
  DWORD sort=0x80000000;
  ((void(__cdecl*)(DWORD,void*,DWORD,float,void*))G(0xbc0540))(pass,output,2,0,&sort);
 });
}
static DWORD WINAPI start(void*){
#ifdef DOA5EXTRA
 if(GetModuleHandleA("DOA5LR-OpenBurst.asi")){log("REFUSED: separate DOA5LR-OpenBurst.asi is loaded");return 0;}
#endif
 if(GetModuleHandleA("DOA5LR-HitFXAdditive.asi")||GetModuleHandleA("DOA5LR-HitFXTrial.asi")){log("REFUSED: another HitFX mod is active");return 0;}
 const BYTE init[]={0xe8,0x0e,0x49,0x01,0x00},emit[]={0xe8,0xfa,0xe9,0xff,0xff};
 bool ready=false;for(unsigned n=0;n<1200;n++){if(accessible(G(0x83075d),5)&&!memcmp(G(0x83075d),init,5)){ready=true;break;}Sleep(100);}
 if(!ready){log("REFUSED: unsupported common loader");return 0;}
 std::vector<Patch> patches;
 auto call=[&](DWORD va,void* f,const BYTE* before){Patch p{(BYTE*)G(va),{before,before+5},std::vector<BYTE>(5)};p.after[0]=0xe8;DWORD rel=(DWORD)f-(DWORD)(p.at+5);memcpy(p.after.data()+1,&rel,4);patches.push_back(p);};
 call(0x83075d,(void*)common,init);call(0x8a5d41,(void*)impact,emit);
 auto slot=[&](unsigned n,DWORD original,void*f){DWORD old=(DWORD)G(original),next=(DWORD)f;Patch p{(BYTE*)G(0xfdae34+4*n),std::vector<BYTE>(4),std::vector<BYTE>(4)};memcpy(p.before.data(),&old,4);memcpy(p.after.data(),&next,4);patches.push_back(p);};
 slot(0,0x53db00,(void*)destroy);slot(1,0x57edd0,(void*)update);slot(2,0x8a5d70,(void*)render);
 log(commit(patches)?"ARMED: OpenBurst v3.1 common compatibility (214/225 textures)":"REFUSED: hook mismatch; no patch installed");return 0;
}
#if defined(DOA5EXTRA) && !defined(CB_TEST)
extern "C" void OpenBurst_Demarrer(void){
 base=(BYTE*)GetModuleHandleW(nullptr);(void)logPath;
 HANDLE t=CreateThread(nullptr,0,start,nullptr,0,nullptr);if(t)CloseHandle(t);
}
#elif !defined(CB_TEST)
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,void*){
 if(reason!=DLL_PROCESS_ATTACH)return TRUE;DisableThreadLibraryCalls(module);base=(BYTE*)GetModuleHandleW(nullptr);
 GetModuleFileNameA(module,logPath,MAX_PATH);char*slash=strrchr(logPath,'\\');if(!slash)return TRUE;
 strcpy(slash+1,"DOA5LR-OpenBurst.log");HANDLE t=CreateThread(nullptr,0,start,nullptr,0,nullptr);if(t)CloseHandle(t);return TRUE;
}
#endif
