#pragma once
#include "NativeEffectRngTls.h"
#include "FastRegion.h"
#include <winternl.h>

namespace NativeEffectRngTls {
enum class InstallStatus {Installed,AlreadyInstalled,InvalidArgument,SignatureMismatch,
 ShimFailed,QueryUnavailable,QueryFailed,TooManyThreads,OpenThreadFailed,
 ProtectionFailed,SuspendFailed,ContextFailed,InstructionPointerBusy,
 ThreadSetChanged,ActiveScope,FlushFailed,PriorAttemptRetained};
struct InstallReport {
 InstallStatus status=InstallStatus::InvalidArgument;
 DWORD error{};unsigned threads{},suspended{},resumeFailures{},protectionFailures{};
 bool bytesPublished{},rollbackFlushFailed{};
 bool Succeeded() const noexcept {
  return (status==InstallStatus::Installed||status==InstallStatus::AlreadyInstalled)
   &&!resumeFailures&&!protectionFailures;
 }
};
struct HookTarget {BYTE* entry{};const BYTE* expected{};std::size_t expectedBytes{};};
struct InstalledHooks {Shim shims[2]{};BYTE* targets[2]{};bool installed{};};
namespace hookdetail {
constexpr unsigned MaxThreads=512;
constexpr ULONG QueryBytes=4u*1024u*1024u;
using QueryFn=NTSTATUS(NTAPI*)(SYSTEM_INFORMATION_CLASS,PVOID,ULONG,PULONG);
struct ThreadList {DWORD ids[MaxThreads]{};unsigned count{};};
struct Thread {DWORD id{};HANDLE handle{};bool suspended{};};
struct Page {void* address{};DWORD old{};bool changed{};};
static_assert(sizeof(SYSTEM_PROCESS_INFORMATION)==0xb8,"Unsupported x86 process-query layout");
static_assert(sizeof(SYSTEM_THREAD_INFORMATION)==0x40,"Unsupported x86 thread-query layout");
inline bool SameBytes(const BYTE* a,const BYTE* b,std::size_t n) noexcept {
 const volatile BYTE* p=a;
 for(std::size_t i=0;i<n;++i)if(p[i]!=b[i])return false;
 return true;
}
inline bool Readable(const void* start,std::size_t bytes) noexcept {
 auto address=reinterpret_cast<std::uintptr_t>(start);
 if(!address||!bytes||address+bytes<address)return false;
 const auto end=address+bytes;
 while(address<end){
  MEMORY_BASIC_INFORMATION region{};

  if(FastRegion::Query(reinterpret_cast<void*>(address),&region,sizeof region)!=sizeof region
   ||region.State!=MEM_COMMIT||(region.Protect&(PAGE_NOACCESS|PAGE_GUARD)))return false;
  const auto next=reinterpret_cast<std::uintptr_t>(region.BaseAddress)+region.RegionSize;
  if(next<=address)return false;
  address=next;
 }
 return true;
}

inline bool Threads(QueryFn query,BYTE* buffer,DWORD process,ThreadList& out) noexcept {
 ULONG returned=0;out.count=0;
 if(query(SystemProcessInformation,buffer,QueryBytes,&returned)<0
  ||returned<sizeof(SYSTEM_PROCESS_INFORMATION)||returned>QueryBytes)return false;
 ULONG offset=0;
 for(;;){
  if(offset>returned-sizeof(SYSTEM_PROCESS_INFORMATION))return false;
  const auto* info=reinterpret_cast<const SYSTEM_PROCESS_INFORMATION*>(buffer+offset);
  const ULONG remaining=returned-offset;
  const ULONG extent=info->NextEntryOffset?info->NextEntryOffset:remaining;
  if(extent<sizeof(*info)||extent>remaining||(info->NextEntryOffset&3))return false;
  if(reinterpret_cast<std::uintptr_t>(info->UniqueProcessId)==process){
   if(!info->NumberOfThreads||info->NumberOfThreads>MaxThreads
    ||info->NumberOfThreads>(extent-sizeof(*info))/sizeof(SYSTEM_THREAD_INFORMATION))return false;
   const auto* thread=reinterpret_cast<const SYSTEM_THREAD_INFORMATION*>(info+1);
   for(ULONG i=0;i<info->NumberOfThreads;++i){
    const auto id=reinterpret_cast<std::uintptr_t>(thread[i].ClientId.UniqueThread);
    if(!id||reinterpret_cast<std::uintptr_t>(thread[i].ClientId.UniqueProcess)!=process)return false;
    for(unsigned j=0;j<out.count;++j)if(out.ids[j]==id)return false;
    out.ids[out.count++]=DWORD(id);
   }
   return true;
  }
  if(!info->NextEntryOffset)return false;
  offset+=info->NextEntryOffset;
 }
}
inline bool SameThreads(const ThreadList& a,const ThreadList& b) noexcept {
 if(a.count!=b.count)return false;
 for(unsigned i=0;i<a.count;++i){bool found=false;
  for(unsigned j=0;j<b.count;++j)if(a.ids[i]==b.ids[j]){found=true;break;}
  if(!found)return false;
 }
 return true;
}
}

inline bool HooksIntact(const InstalledHooks& hooks) noexcept {
 if(!hooks.installed)return false;
 for(unsigned i=0;i<2;++i){
  if(!hooks.targets[i]||!hooks.shims[i].entry||!hookdetail::Readable(hooks.targets[i],sizeof Prologue))return false;
  BYTE expected[sizeof Prologue]={0xe9,0,0,0,0,0x90,0x90};
  const auto delta=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(hooks.shims[i].entry)
   -reinterpret_cast<std::uintptr_t>(hooks.targets[i]+5));
  std::memcpy(expected+1,&delta,4);
  if(!hookdetail::SameBytes(hooks.targets[i],expected,sizeof expected))return false;
 }
 return true;
}

inline InstallReport InstallHooks(Router& router,const HookTarget (&targets)[2],
 InstalledHooks& installed) noexcept {
 using namespace hookdetail;
 InstallReport result;
 if(installed.installed){
  if(installed.targets[0]==targets[0].entry&&installed.targets[1]==targets[1].entry)
   result.status=HooksIntact(installed)?InstallStatus::AlreadyInstalled:InstallStatus::SignatureMismatch;
  return result;
 }

 if(installed.shims[0].entry||installed.shims[1].entry){
  result.status=InstallStatus::PriorAttemptRetained;return result;
 }
 if(!router.Ready()||targets[0].entry==targets[1].entry)return result;
 for(const auto& target:targets){
  if(!target.entry||!target.expected||target.expectedBytes<sizeof Prologue
   ||!Readable(target.entry,target.expectedBytes)||!Readable(target.expected,target.expectedBytes))return result;
  if(!SameBytes(target.entry,target.expected,target.expectedBytes)
   ||!SameBytes(target.entry,Prologue,sizeof Prologue)){
   result.status=InstallStatus::SignatureMismatch;return result;
  }
 }
 auto query=reinterpret_cast<QueryFn>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"NtQuerySystemInformation"));
 if(!query){result.status=InstallStatus::QueryUnavailable;return result;}

 const HANDLE process=GetCurrentProcess();const DWORD processId=GetCurrentProcessId(),self=GetCurrentThreadId();
 SYSTEM_INFO system{};GetSystemInfo(&system);
 if(!system.dwPageSize)return result;
 BYTE* queryBuffer=static_cast<BYTE*>(VirtualAlloc(nullptr,QueryBytes,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
 if(!queryBuffer){result.status=InstallStatus::QueryFailed;result.error=GetLastError();return result;}
 ThreadList before{},after{};Thread threads[MaxThreads]{};
 unsigned threadCount=0;Page pages[2]{};unsigned pageCount=0;
 BYTE patch[2][sizeof Prologue]{};BYTE original[2][sizeof Prologue]{};
 bool prepared=true,patchAttempted=false;
 if(!Threads(query,queryBuffer,processId,before)){
  result.status=InstallStatus::QueryFailed;prepared=false;
 }
 if(prepared){
  bool hasSelf=false;
  for(unsigned i=0;i<before.count;++i){
   if(before.ids[i]==self){hasSelf=true;continue;}
   Thread& thread=threads[threadCount++];thread.id=before.ids[i];
   thread.handle=OpenThread(THREAD_SUSPEND_RESUME|THREAD_GET_CONTEXT|THREAD_QUERY_INFORMATION,FALSE,thread.id);
   if(!thread.handle){result.status=InstallStatus::OpenThreadFailed;result.error=GetLastError();prepared=false;break;}

   if(GetProcessIdOfThread(thread.handle)!=processId){
    result.status=InstallStatus::ThreadSetChanged;prepared=false;break;
   }
  }
  if(!hasSelf){result.status=InstallStatus::QueryFailed;prepared=false;}
 }
 result.threads=before.count;
 if(prepared)for(unsigned i=0;i<2;++i){
  if(!BuildShim(installed.shims[i],targets[i].entry,router)){
   result.status=InstallStatus::ShimFailed;result.error=GetLastError();prepared=false;break;
  }
  installed.targets[i]=targets[i].entry;
  for(unsigned j=0;j<sizeof Prologue;++j)original[i][j]=targets[i].entry[j];
  patch[i][0]=0xe9;
  const auto delta=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(installed.shims[i].entry)
   -reinterpret_cast<std::uintptr_t>(targets[i].entry+5));
  std::memcpy(patch[i]+1,&delta,4);patch[i][5]=patch[i][6]=0x90;
  const auto start=reinterpret_cast<std::uintptr_t>(targets[i].entry);
  const auto page=start-(start%system.dwPageSize);

  if(start+sizeof Prologue>page+system.dwPageSize){prepared=false;result.status=InstallStatus::InvalidArgument;break;}
  bool found=false;
  for(unsigned j=0;j<pageCount;++j)if(pages[j].address==reinterpret_cast<void*>(page))found=true;
  if(!found)pages[pageCount++].address=reinterpret_cast<void*>(page);
 }
 if(prepared)for(unsigned i=0;i<pageCount;++i){
  if(!VirtualProtect(pages[i].address,system.dwPageSize,PAGE_EXECUTE_READWRITE,&pages[i].old)){
   result.status=InstallStatus::ProtectionFailed;result.error=GetLastError();prepared=false;break;
  }
  pages[i].changed=true;
 }

 if(prepared){
  for(unsigned i=0;i<threadCount;++i){
   if(SuspendThread(threads[i].handle)==DWORD(-1)){
    result.status=InstallStatus::SuspendFailed;result.error=GetLastError();prepared=false;break;
   }
   threads[i].suspended=true;++result.suspended;
  }
  if(prepared)for(unsigned i=0;i<threadCount;++i){
   CONTEXT context{};context.ContextFlags=CONTEXT_CONTROL;
   if(!GetThreadContext(threads[i].handle,&context)){
    result.status=InstallStatus::ContextFailed;result.error=GetLastError();prepared=false;break;
   }
   for(unsigned j=0;j<2;++j){
    const auto start=reinterpret_cast<std::uintptr_t>(targets[j].entry);
    if(context.Eip>=start&&context.Eip<start+sizeof Prologue){
     result.status=InstallStatus::InstructionPointerBusy;prepared=false;break;
    }
   }
   if(!prepared)break;
  }
  if(prepared&&(!Threads(query,queryBuffer,processId,after)||!SameThreads(before,after))){
   result.status=InstallStatus::ThreadSetChanged;prepared=false;
  }
  if(prepared&&InterlockedCompareExchange(const_cast<volatile LONG*>(router.ActiveCounter()),0,0)){
   result.status=InstallStatus::ActiveScope;prepared=false;
  }
  if(prepared)for(const auto& target:targets)if(!SameBytes(target.entry,target.expected,target.expectedBytes)){
   result.status=InstallStatus::SignatureMismatch;prepared=false;break;
  }
  if(prepared){
   patchAttempted=true;
   for(unsigned i=0;i<2;++i){volatile BYTE* target=targets[i].entry;
    for(unsigned j=0;j<sizeof Prologue;++j)target[j]=patch[i][j];
   }
   bool flushed=true;
   for(unsigned i=0;i<2;++i)if(!FlushInstructionCache(process,targets[i].entry,sizeof Prologue))flushed=false;
   if(flushed){
    installed.installed=true;result.bytesPublished=true;result.status=InstallStatus::Installed;
   }else{
    result.status=InstallStatus::FlushFailed;result.error=GetLastError();
    for(unsigned i=0;i<2;++i){volatile BYTE* target=targets[i].entry;
     for(unsigned j=0;j<sizeof Prologue;++j)target[j]=original[i][j];
    }
    for(unsigned i=0;i<2;++i)if(!FlushInstructionCache(process,targets[i].entry,sizeof Prologue))result.rollbackFlushFailed=true;
   }
  }
  for(unsigned i=threadCount;i>0;--i)if(threads[i-1].suspended){
   if(ResumeThread(threads[i-1].handle)==DWORD(-1))++result.resumeFailures;
   threads[i-1].suspended=false;
  }
 }

 for(unsigned i=0;i<pageCount;++i)if(pages[i].changed){DWORD unused=0;
  if(!VirtualProtect(pages[i].address,system.dwPageSize,pages[i].old,&unused))++result.protectionFailures;
 }
 for(unsigned i=0;i<threadCount;++i)if(threads[i].handle)CloseHandle(threads[i].handle);
 VirtualFree(queryBuffer,0,MEM_RELEASE);

 if(!patchAttempted)for(auto& shim:installed.shims)if(shim.entry){VirtualFree(shim.entry,0,MEM_RELEASE);shim={};}
 return result;
}
}
