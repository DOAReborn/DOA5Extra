#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <new>

namespace NativeEffectRngTls {
static_assert(sizeof(void*)==4,"DOA5 RNG routing requires x86");
constexpr unsigned WordCount=624,MaximumDepth=32;
constexpr std::size_t StateBytes=0x9c8;
struct MtState {
 std::uint32_t words[WordCount];
 std::uint32_t next,count;
};
static_assert(sizeof(MtState)==StateBytes&&offsetof(MtState,next)==0x9c0,"Native MT layout");
inline void Seed(MtState& s,std::uint32_t seed) noexcept {
 s.words[0]=seed;
 for(unsigned i=1;i<WordCount;++i)s.words[i]=1812433253u*(s.words[i-1]^(s.words[i-1]>>30))+i;

 for(unsigned i=0;i<WordCount;++i){
  const std::uint32_t y=(s.words[i]&0x80000000u)|(s.words[(i+1)%WordCount]&0x7fffffffu);
  s.words[i]=s.words[(i+397)%WordCount]^(y>>1)^((y&1)?0x9908b0dfu:0u);
 }
 s.next=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(s.words));s.count=WordCount;
}
enum class Status {Entered,Restored,NotReady,NotNativeOnline,RollbackUnknown,
 RollbackActive,VersionUnverified,NoMemory,NestingLimit,OutOfOrder,WrongThread,
 Inactive,Poisoned};
struct Admission {bool nativeOnline{},rollbackKnown{},rollbackActive{},versionVerified{};};
class Router;
struct ThreadState {
 Router* owner{};DWORD thread{};unsigned depth{};std::uint32_t generation=1;
 bool poisoned{};
 MtState states[2]{};
};
class Scope;
class Router {
 friend class Scope;
 DWORD slot_=FLS_OUT_OF_INDEXES;
 void* native_[2]{};
 std::uint32_t seed_{};
 volatile LONG ready_=0,sequence_=0,active_=0;
 static void NTAPI Destroy(void* value) noexcept {
  auto* state=static_cast<ThreadState*>(value);
  if(!state)return;
  if(state->depth)InterlockedDecrement(&state->owner->active_);
  state->~ThreadState();
  HeapFree(GetProcessHeap(),0,state);
 }
 ThreadState* Current(bool create) noexcept {
  auto* state=static_cast<ThreadState*>(FlsGetValue(slot_));
  if(state||!create)return state;
  void* memory=HeapAlloc(GetProcessHeap(),0,sizeof(ThreadState));
  if(!memory)return nullptr;
  state=new(memory) ThreadState{};
  state->owner=this;state->thread=GetCurrentThreadId();state->generation=1;
  const auto serial=static_cast<std::uint32_t>(InterlockedIncrement(&sequence_));
  Seed(state->states[0],seed_^(serial*0x9e3779b9u));
  Seed(state->states[1],seed_^0xa5c31f27u^(serial*0x85ebca6bu));
  if(!FlsSetValue(slot_,state)){state->~ThreadState();HeapFree(GetProcessHeap(),0,state);return nullptr;}
  return state;
 }
 Status Enter(const Admission& a,ThreadState*& state,unsigned& level,std::uint32_t& generation) noexcept {
  if(!a.nativeOnline)return Status::NotNativeOnline;
  if(!a.rollbackKnown)return Status::RollbackUnknown;
  if(a.rollbackActive)return Status::RollbackActive;
  if(!a.versionVerified)return Status::VersionUnverified;
  if(!Ready())return Status::NotReady;
  state=Current(true);if(!state)return Status::NoMemory;
  if(state->poisoned)return Status::Poisoned;
  if(state->depth>=MaximumDepth)return Status::NestingLimit;
  if(!state->depth)InterlockedIncrement(&active_);
  generation=state->generation;level=++state->depth;return Status::Entered;
 }
 Status Leave(ThreadState* state,unsigned level,std::uint32_t generation) noexcept {
  if(GetCurrentThreadId()!=state->thread)return Status::WrongThread;
  if(!state->depth||generation!=state->generation)return Status::Inactive;
  if(level!=state->depth){
   state->poisoned=true;state->depth=0;++state->generation;InterlockedDecrement(&active_);
   return Status::OutOfOrder;
  }
  if(--state->depth==0){++state->generation;InterlockedDecrement(&active_);}
  return Status::Restored;
 }
public:
 Router() noexcept=default;
 Router(const Router&)=delete;Router& operator=(const Router&)=delete;

 bool Configure(void* combat,void* visual,std::uint32_t seed=0x53465831u) noexcept {
  if(Ready())return native_[0]==combat&&native_[1]==visual&&seed_==seed;
  if(!combat||!visual||combat==visual||(reinterpret_cast<std::uintptr_t>(combat)&3)
    ||(reinterpret_cast<std::uintptr_t>(visual)&3))return false;
  const DWORD slot=FlsAlloc(&Destroy);if(slot==FLS_OUT_OF_INDEXES)return false;
  native_[0]=combat;native_[1]=visual;seed_=seed;slot_=slot;
  InterlockedExchange(&ready_,1);return true;
 }
 bool Ready() const noexcept {return InterlockedCompareExchange(const_cast<volatile LONG*>(&ready_),0,0)!=0;}
 void* Native(unsigned index) const noexcept {return native_[index];}
 const volatile LONG* ActiveCounter() const noexcept {return &active_;}
 void* Route(void* original) noexcept {
  if(!Ready()||(original!=native_[0]&&original!=native_[1]))return original;

  const DWORD error=GetLastError();auto* state=Current(false);SetLastError(error);
  if(!state||!state->depth||state->poisoned)return original;
  return &state->states[original==native_[1]?1:0];
 }
};
class Scope {
 Router* router_{};ThreadState* state_{};unsigned level_{};std::uint32_t generation_{};
 Status status_=Status::Inactive;bool active_{};
public:
 Scope(Router& router,const Admission& admission) noexcept:router_(&router){
  status_=router.Enter(admission,state_,level_,generation_);active_=status_==Status::Entered;
 }
 Scope(const Scope&)=delete;Scope& operator=(const Scope&)=delete;
 ~Scope(){if(active_)Finish();}
 explicit operator bool() const noexcept {
  return active_&&GetCurrentThreadId()==state_->thread&&state_->depth>=level_
   &&state_->generation==generation_&&!state_->poisoned;
 }
 Status Result() const noexcept {return status_;}
 bool Finish() noexcept {
  if(!active_)return status_==Status::Restored;
  status_=router_->Leave(state_,level_,generation_);
  if(status_!=Status::WrongThread)active_=false;
  return status_==Status::Restored;
 }
};
inline void* __cdecl RouteThunk(Router* router,void* original) noexcept {return router->Route(original);}
struct Shim {BYTE* entry{};std::size_t size{};};

inline constexpr BYTE Prologue[]={0x55,0x8b,0xec,0x51,0x56,0x8b,0xf1};
inline bool BuildShim(Shim& out,void* originalEntry,Router& router) noexcept {
 if(out.entry||!router.Ready()||!originalEntry
  ||std::memcmp(originalEntry,Prologue,sizeof Prologue)
  ||!IsProcessorFeaturePresent(PF_XMMI64_INSTRUCTIONS_AVAILABLE))return false;
 BYTE* code=static_cast<BYTE*>(VirtualAlloc(nullptr,256,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
 if(!code)return false;
 unsigned used=0;
 auto byte=[&](BYTE b){code[used++]=b;};
 auto word=[&](std::uint32_t x){std::memcpy(code+used,&x,4);used+=4;};
 auto bytes=[&](std::initializer_list<BYTE> b){for(BYTE x:b)byte(x);};
 auto branch=[&](BYTE second){bytes({0x0f,second});const unsigned at=used;word(0);return at;};
 auto fix=[&](unsigned at,unsigned label){const std::int32_t d=std::int32_t(label)-std::int32_t(at+4);std::memcpy(code+at,&d,4);};
 byte(0x9c);
 bytes({0x81,0xf9});word(std::uint32_t(reinterpret_cast<std::uintptr_t>(router.Native(0))));
 const unsigned interested=branch(0x84);
 bytes({0x81,0xf9});word(std::uint32_t(reinterpret_cast<std::uintptr_t>(router.Native(1))));
 const unsigned otherObject=branch(0x85);
 fix(interested,used);
 bytes({0x83,0x3d});word(std::uint32_t(reinterpret_cast<std::uintptr_t>(router.ActiveCounter())));byte(0);
 const unsigned inactive=branch(0x84);
 byte(0x60);bytes({0x89,0xe5});
 bytes({0x81,0xec});word(0x220);bytes({0x83,0xe4,0xf0});
 bytes({0x0f,0xae,0x04,0x24});
 bytes({0xdb,0xe3});
 bytes({0xc7,0x84,0x24});word(0x200);word(0x1f80);
 bytes({0x0f,0xae,0x94,0x24});word(0x200);
 byte(0xfc);
 bytes({0x8b,0x45,0x18,0x83,0xec,0x08,0x50,0x68});
 word(std::uint32_t(reinterpret_cast<std::uintptr_t>(&router)));
 byte(0xe8);word(std::uint32_t(reinterpret_cast<std::uintptr_t>(&RouteThunk)-reinterpret_cast<std::uintptr_t>(code+used+4)));
 bytes({0x83,0xc4,0x10,0x89,0x45,0x18});
 bytes({0x0f,0xae,0x0c,0x24,0x89,0xec,0x61});
 fix(otherObject,used);fix(inactive,used);byte(0x9d);
 for(BYTE b:Prologue)byte(b);
 byte(0xe9);word(std::uint32_t(reinterpret_cast<std::uintptr_t>(originalEntry)+sizeof Prologue-reinterpret_cast<std::uintptr_t>(code+used+4)));
 DWORD old=0;
 if(!VirtualProtect(code,256,PAGE_EXECUTE_READ,&old)||!FlushInstructionCache(GetCurrentProcess(),code,used)){
  VirtualFree(code,0,MEM_RELEASE);return false;
 }
 out={code,used};return true;
}
}
