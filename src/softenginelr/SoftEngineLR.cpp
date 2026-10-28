#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "../statefulcostumes/FastRegion.h"
#include "../commun/journal_extra.h"
#include <tlhelp32.h>
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <vector>
#include <array>
#include <utility>
#include <cmath>
static_assert(sizeof(void*)==4,"x86 required");
static BYTE *Base;
static volatile BYTE LR;
static char Ini[MAX_PATH],Dir[MAX_PATH],Root[MAX_PATH];
static constexpr DWORD LRItem=0x7ffe;
static void Log(const char* fmt,...){va_list v;va_start(v,fmt);JournalExtra_V("SoftEngineLR",fmt,v);va_end(v);}

struct QueryCache { MEMORY_BASIC_INFORMATION regions[32]{};unsigned count=0,next=0,hits=0,misses=0; };
static thread_local QueryCache* ActiveQueries=nullptr;
static SIZE_T WINAPI PhysicsQuery(LPCVOID address,PMEMORY_BASIC_INFORMATION out,SIZE_T length){
 auto*c=ActiveQueries;
 if(!c&&length>=sizeof(*out)&&out){

  static thread_local QueryCache tickCache;static thread_local DWORD tickOf=0;DWORD now=GetTickCount();
  if(now!=tickOf){tickOf=now;tickCache.count=tickCache.next=0;}
  c=&tickCache;
 }
 if(!c||length<sizeof(*out)||!out)return VirtualQuery(address,out,length);
 uintptr_t a=(uintptr_t)address;
 for(unsigned i=0;i<c->count;i++){const auto&m=c->regions[i];uintptr_t b=(uintptr_t)m.BaseAddress,e=b+m.RegionSize;if(a>=b&&a<e){*out=m;uintptr_t page=a&~uintptr_t(4095);out->BaseAddress=(void*)page;out->RegionSize=e-page;c->hits++;return sizeof(*out);}}

 c->misses++;SIZE_T result=FastRegion::Query(address,out,length,true);
 if(result==sizeof(*out)&&out->State==MEM_COMMIT&&!(out->Protect&(PAGE_GUARD|PAGE_NOACCESS))){unsigned slot=c->count<32?c->count++:c->next++%32;c->regions[slot]=*out;}return result;
}
struct QueryScope {QueryCache cache;QueryCache*previous;QueryScope():previous(ActiveQueries){ActiveQueries=&cache;}~QueryScope(){ActiveQueries=previous;}};
static bool Readable(const void*p,size_t n){uintptr_t a=(uintptr_t)p,e=a+n;if(!a||e<a)return false;while(a<e){MEMORY_BASIC_INFORMATION m{};if(!PhysicsQuery((void*)a,&m,sizeof m)||m.State!=MEM_COMMIT||(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;DWORD q=m.Protect&255;if(q!=PAGE_READONLY&&q!=PAGE_READWRITE&&q!=PAGE_WRITECOPY&&q!=PAGE_EXECUTE_READ&&q!=PAGE_EXECUTE_READWRITE&&q!=PAGE_EXECUTE_WRITECOPY)return false;uintptr_t next=(uintptr_t)m.BaseAddress+m.RegionSize;if(next<=a)return false;a=next;}return true;}
template<class T>T& At(void*p,size_t o){return *(T*)((BYTE*)p+o);}
static BYTE* VA(DWORD a){return Base+a-0x400000;}
static void Rel(BYTE*p,BYTE opcode,const void*target){p[0]=opcode;At<DWORD>(p,1)=(DWORD)target-(DWORD)(p+5);}
static bool Write(BYTE*p,const void*s,size_t n){DWORD old;if(!VirtualProtect(p,n,PAGE_EXECUTE_READWRITE,&old))return false;memcpy(p,s,n);DWORD unused;VirtualProtect(p,n,old,&unused);FlushInstructionCache(GetCurrentProcess(),p,n);return true;}
struct Change { BYTE* at;std::vector<BYTE> old,now; };
static std::vector<Change> Changes;
static bool Prepare(BYTE*at,const BYTE*expected,size_t n,const void*target,BYTE opcode=0xe9){if(!Readable(at,n)||memcmp(at,expected,n))return false;Change c{at,{at,at+n},std::vector<BYTE>(n,0x90)};c.now[0]=opcode;At<DWORD>(c.now.data(),1)=(DWORD)target-(DWORD)(at+5);Changes.push_back(c);return true;}

static void* Hook(BYTE*at,const char*hex,void*target){std::vector<BYTE>b;while(*hex){unsigned u;sscanf(hex,"%2x",&u);b.push_back((BYTE)u);hex+=2;}if(!Readable(at,b.size())||memcmp(at,b.data(),b.size()))return nullptr;BYTE*t=(BYTE*)VirtualAlloc(nullptr,b.size()+5,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);if(!t)return nullptr;memcpy(t,b.data(),b.size());Rel(t+b.size(),0xe9,at+b.size());if(!Prepare(at,b.data(),b.size(),target))return nullptr;return t;}

using BuildFn=void(__attribute__((thiscall))*)(BYTE*,DWORD*);

using LabelFn=const wchar_t*(__stdcall*)(DWORD*,DWORD);
using MenuFn=void(__attribute__((thiscall))*)(BYTE*,DWORD);
using RefreshFn=void(__attribute__((thiscall))*)(BYTE*);
using SetFn=BYTE(__cdecl*)(DWORD);
using GetFn=DWORD(__cdecl*)();
static BuildFn BuildOriginal;static LabelFn LabelOriginal;
static MenuFn ApplyOriginal,ConfirmOriginal,CancelOriginal;
static RefreshFn RefreshOriginal;static SetFn SetOriginal;static GetFn GetOriginal;
static bool HasLR=false;
static void SaveMode(bool enabled){if(LR==(BYTE)enabled)return;LR=enabled;WritePrivateProfileStringA("SoftEngineLR","LR",enabled?"1":"0",Ini);Log("Mode %s",enabled?"LR":"native PC");}
static BYTE __cdecl SetOption(DWORD mode){SaveMode(false);return SetOriginal(mode);}
static DWORD __cdecl GetOption(){return LR?2:GetOriginal();}
static const wchar_t* __stdcall Label(DWORD*item,DWORD context){return item&&item[1]==LRItem?L"LR":LabelOriginal(item,context);}
static void FixSelection(BYTE*self){if(!LR||!HasLR)return;using SelectFn=void(__attribute__((thiscall))*)(BYTE*,DWORD,DWORD);((SelectFn)VA(0xbd6790))(self,self[0x5864],LRItem);At<DWORD>(self,0x5880)=LRItem;}
static void __attribute__((fastcall)) Refresh(BYTE*self,void*){RefreshOriginal(self);FixSelection(self);}
static void __attribute__((fastcall)) Confirm(BYTE*self,void*,DWORD channel){ConfirmOriginal(self,channel);FixSelection(self);}
static void __attribute__((fastcall)) Cancel(BYTE*self,void*,DWORD channel){CancelOriginal(self,channel);FixSelection(self);}
static void __attribute__((fastcall)) Apply(BYTE*self,void*,DWORD channel){using GroupFn=DWORD(__attribute__((thiscall))*)(BYTE*,DWORD);DWORD group=((GroupFn)VA(0xbd63f0))(self,channel);if(group==0xd7&&At<DWORD>(self,0x5880)==LRItem){SetOriginal(2);SaveMode(true);return;}ApplyOriginal(self,channel);}

static bool ExpandMenu(DWORD*header,BYTE*blob,std::vector<DWORD>&groups,std::vector<DWORD>&items){
 if(header[0]>1024||header[2]>4096||!header[1]||!header[3])return false;
 auto*g=(DWORD*)(blob+header[1]);auto*i=(DWORD*)(blob+header[3]);
 if(!Readable(g,header[0]*24)||!Readable(i,header[2]*24))return false;
 int group=-1,omg=-1;unsigned seen=0;
 for(unsigned n=0;n<header[0];n++)if(g[n*6]==0xd7){if(group>=0)return false;group=n;}
 for(unsigned n=0;n<header[2];n++){DWORD id=i[n*6+1];if(id==LRItem)return false;if(i[n*6]==0xd7){if(id<0x52f||id>0x532)return false;seen|=1u<<(id-0x52f);if(id==0x532)omg=n;}}
 if(group<0||omg<0||seen!=15)return false;
 DWORD x=g[group*6+3],y=g[group*6+4];if(!((x==4&&y==1)||(x==1&&y==4))){Log("LR menu rejected dimensions %lu x %lu",x,y);return false;}
 unsigned axis=x==4?2:3;
 if(i[omg*6+axis]!=3)return false;
 groups.assign(g,g+header[0]*6);items.assign(i,i+header[2]*6);
 std::array<DWORD,6> entry{};memcpy(entry.data(),i+omg*6,24);entry[1]=LRItem;
 items[omg*6+axis]=4;items.insert(items.begin()+omg*6,entry.begin(),entry.end());
 groups[group*6+(x==4?3:4)]=5;
 return true;
}
static void __attribute__((fastcall)) Build(BYTE*self,void*,DWORD*root){
 if(!root||!*root||!Readable(self,0x64)){BuildOriginal(self,root);return;}
 BYTE*blob=At<BYTE*>(self,0x60);DWORD*h=(DWORD*)(blob+*root);

 if(!Readable(h,0x38)){BuildOriginal(self,root);return;}
 std::vector<DWORD>groups,items;DWORD copy[14];memcpy(copy,h,sizeof copy);
 if(!ExpandMenu(copy,blob,groups,items)){BuildOriginal(self,root);return;}
 copy[1]=(DWORD)groups.data()-(DWORD)blob;copy[2]++;copy[3]=(DWORD)items.data()-(DWORD)blob;
 DWORD offset=(DWORD)copy-(DWORD)blob;Log("Building LR menu with complete navigation header");HasLR=true;BuildOriginal(self,&offset);Log("Menu LR created: native choices + LR; OMG unlock preserved");
}

#include "BodyBounce.h"
#include "OursEngine.inc"
using UpdateFn=void(__attribute__((thiscall))*)(BYTE*,float,DWORD);
static UpdateFn UpdateOriginal;
static HMODULE RollbackOwner;
static BYTE* RollbackPhysicsTarget(BYTE* site,HMODULE owner){
 if(!owner||!Readable(site,6)||site[0]!=0xe9||site[5]!=0x90)return nullptr;
 BYTE* target=site+5+At<int>(site,1);MEMORY_BASIC_INFORMATION m{};
 if(!VirtualQuery(target,&m,sizeof m)||m.AllocationBase!=owner||m.State!=MEM_COMMIT||(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return nullptr;
 DWORD p=m.Protect&255;if(p!=PAGE_EXECUTE_READ&&p!=PAGE_EXECUTE_READWRITE&&p!=PAGE_EXECUTE_WRITECOPY)return nullptr;
 return target;
}
static bool WaitForRollback(){
 HMODULE rb=GetModuleHandleA("DOA5Rollback.asi");
 if(!rb){

  char candidate[MAX_PATH];snprintf(candidate,sizeof candidate,"%sDOA5Rollback.asi",Dir);
  bool expected=GetFileAttributesA(candidate)!=INVALID_FILE_ATTRIBUTES;
  if(!expected){GetModuleFileNameA(nullptr,candidate,MAX_PATH);char*s=strrchr(candidate,'\\');if(s)strcpy(s+1,"DOA5Rollback.asi");expected=GetFileAttributesA(candidate)!=INVALID_FILE_ATTRIBUTES;}
  if(!expected)return true;
  for(unsigned i=0;i<150&&!rb;i++){Sleep(100);rb=GetModuleHandleA("DOA5Rollback.asi");}
  if(!rb){Log("Rollback file present but module not loaded; LR installation deferred");return false;}
 }
 char ini[MAX_PATH];if(!GetModuleFileNameA(rb,ini,MAX_PATH))return false;
 char*dot=strrchr(ini,'.');if(!dot)return false;strcpy(dot,".ini");
 if(!GetPrivateProfileIntA("Rollback","Enabled",1,ini))return true;
 for(unsigned i=0;i<150;i++){
  if(RollbackPhysicsTarget(VA(0x4c64d0),rb)){RollbackOwner=rb;Log("Rollback physics hook ready; installing chained LR wrapper");return true;}
  Sleep(100);
 }
 Log("Rollback physics hook unavailable after 15 seconds; LR left uninstalled to preserve diagnostics");return false;
}
static void __fastcall PhysicsUpdate(BYTE*self,void*,float dt,DWORD context){
 if(!LR&&!BodyBounce::settings.enabled){UpdateOriginal(self,dt,context);return;}
 QueryScope scope;
 {OursLR::Scope lr(self);UpdateOriginal(self,dt,context);}
 OursLR::Measure(self,"maison");
 BodyBounce::Apply(self,dt,(float*)context);
 static thread_local unsigned frames=0,hits=0,misses=0;hits+=scope.cache.hits;misses+=scope.cache.misses;
 static unsigned bilans=0;
 if(++frames==600){if(bilans<3){++bilans;Log("LR memory checks over 600 actor updates: reused=%u system=%u",hits,misses);}frames=hits=misses=0;}
}
static bool PreparePhysicsUpdate(){
 if(RollbackOwner){
  BYTE*site=VA(0x4c64d0);BYTE*target=RollbackPhysicsTarget(site,RollbackOwner);if(!target)return false;
  UpdateOriginal=(UpdateFn)target;
  return Prepare(site,site,6,(void*)PhysicsUpdate);
 }
 UpdateOriginal=(UpdateFn)Hook(VA(0x4c64d0),"558bec568bf1",(void*)PhysicsUpdate);
 return UpdateOriginal!=nullptr;
}
static bool PrepareMenu(){
 BuildOriginal=(BuildFn)Hook(VA(0xbd9060),"558bec8b4508",(void*)Build);
 LabelOriginal=(LabelFn)Hook(VA(0xacaac0),"558bec8b4508",(void*)Label);
 ApplyOriginal=(MenuFn)Hook(VA(0xaca790),"558bec8b4508",(void*)Apply);
 RefreshOriginal=(RefreshFn)Hook(VA(0xaca2c0),"56578bf1bf8d000000",(void*)Refresh);
 ConfirmOriginal=(MenuFn)Hook(VA(0xacc290),"558bec5156",(void*)Confirm);
 CancelOriginal=(MenuFn)Hook(VA(0xacb8c0),"558bec5156",(void*)Cancel);
 SetOriginal=(SetFn)Hook(VA(0x4be2a0),"558bec8b4508",(void*)SetOption);

 BYTE expected[6]={0xa1};At<DWORD>(expected,1)=(DWORD)VA(0x13744ac);expected[5]=0xc3;
 GetOriginal=[]()->DWORD{return At<DWORD>(Base,0xf744ac);};
 return BuildOriginal&&LabelOriginal&&ApplyOriginal&&RefreshOriginal&&ConfirmOriginal&&CancelOriginal&&SetOriginal&&Prepare(VA(0x4be130),expected,6,(void*)GetOption);
}
static bool Commit(){

 std::vector<HANDLE>threads;HANDLE snap=CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD,0);if(snap==INVALID_HANDLE_VALUE)return false;
 THREADENTRY32 e{};e.dwSize=sizeof e;if(Thread32First(snap,&e)){do{if(e.th32OwnerProcessID==GetCurrentProcessId()&&e.th32ThreadID!=GetCurrentThreadId()){HANDLE h=OpenThread(THREAD_SUSPEND_RESUME|THREAD_QUERY_LIMITED_INFORMATION,FALSE,e.th32ThreadID);if(!h){DWORD error=GetLastError();if(error==ERROR_INVALID_PARAMETER)continue;for(auto t:threads)CloseHandle(t);CloseHandle(snap);Log("Commit OpenThread failed thread=%lu error=%lu",e.th32ThreadID,error);return false;}threads.push_back(h);}}while(Thread32Next(snap,&e));}CloseHandle(snap);

 std::vector<BYTE>suspended(threads.size(),0);bool ok=true;DWORD error=0;const char*phase="suspend";BYTE*failed=nullptr;
 for(size_t i=0;i<threads.size();i++){if(SuspendThread(threads[i])!=DWORD(-1)){suspended[i]=1;continue;}error=GetLastError();DWORD code=STILL_ACTIVE;if(GetExitCodeThread(threads[i],&code)&&code!=STILL_ACTIVE)continue;ok=false;break;}
 if(ok){phase="signature";for(auto&c:Changes)if(memcmp(c.at,c.old.data(),c.old.size())){failed=c.at;ok=false;break;}}
 size_t done=0;if(ok){phase="write";for(auto&c:Changes){if(!Write(c.at,c.now.data(),c.now.size())){failed=c.at;error=GetLastError();ok=false;break;}done++;}}
 if(!ok)while(done){auto&c=Changes[--done];Write(c.at,c.old.data(),c.old.size());}
 for(size_t i=0;i<threads.size();i++)if(suspended[i])ResumeThread(threads[i]);for(auto h:threads)CloseHandle(h);if(!ok)Log("Commit failed phase=%s address=%p error=%lu",phase,failed,error);return ok;
}
static DWORD WINAPI Worker(void*){
 if(Extra_SepareCharge("DOA5LR-SoftEngineLR.asi")){Log("Refused: separate DOA5LR-SoftEngineLR.asi is loaded");return 0;}
 Log("LR module revision 12 (moteur maison, tables PS4 1.19, entree corps LR f8/f10 separees, cache memoire FastRegion) starting");BodyBounce::Load();
 BYTE probe[]={0x55,0x8b,0xec,0x6a,0xff};bool ready=false;
 for(int i=0;i<600;i++){if(Readable(VA(0x4c6730),5)&&!memcmp(VA(0x4c6730),probe,5)){ready=true;break;}Sleep(100);}
 if(!ready){Log("Unsupported/encrypted game code: no changes");return 0;}
 if(!WaitForRollback())return 0;
 if(!OursLR::Prepare()||!PreparePhysicsUpdate()||!PrepareMenu()){Log("Signature validation failed: no game changes");return 0;}
 LR=GetPrivateProfileIntA("SoftEngineLR","LR",1,Ini)!=0;
 bool installed=false;for(unsigned attempt=0;attempt<10&&!installed;attempt++){installed=Commit();if(!installed)Sleep(50);}
 if(!installed){LR=0;Log("Installation failed/rolled back; LR is INACTIVE");return 0;}
 Log("Installed: moteur maison (tables PS4 1.19, %u fiches, aucune DLL), %zu changements, LR=%u",PS4LR::kNombreFiches,Changes.size(),LR);return 0;
}
#ifndef LR_TEST
extern "C" void SoftEngineLR_Demarrer(void){
 Base=(BYTE*)GetModuleHandleA(nullptr);
 snprintf(Ini,sizeof Ini,"%s",Extra_Ini());strcpy(Dir,Ini);char*s=strrchr(Dir,'\\');if(s)s[1]=0;
 GetModuleFileNameA(nullptr,Root,MAX_PATH);s=strrchr(Root,'\\');if(s)s[1]=0;
 HANDLE t=CreateThread(nullptr,0,Worker,nullptr,0,nullptr);if(t)CloseHandle(t);
}
#endif
