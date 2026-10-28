#pragma once

#include <windows.h>
#include <cstdint>

namespace FastRegion {

inline ULONGLONG LongLife(const MEMORY_BASIC_INFORMATION& m) noexcept {
 const DWORD p=m.Protect&0xff;
 if(m.Type==MEM_IMAGE&&(p==PAGE_READONLY||p==PAGE_EXECUTE_READ))return 1000;
 if((m.Type==MEM_IMAGE||m.Type==MEM_PRIVATE)&&p==PAGE_READWRITE&&m.RegionSize>=(16u<<20))return 250;
 return 0;
}

inline SIZE_T Query(const void* address,MEMORY_BASIC_INFORMATION* out,SIZE_T length,bool longOnly=false) noexcept {
 if(!out||length<sizeof(*out))return VirtualQuery(address,out,length);
 struct Entry {MEMORY_BASIC_INFORMATION info;DWORD tick;ULONGLONG until;};
 static thread_local Entry cache[32]{};static thread_local unsigned next=0;
 const auto a=reinterpret_cast<std::uintptr_t>(address);
 const DWORD tick=GetTickCount();const ULONGLONG now=GetTickCount64();
 for(auto& e:cache){
  const auto base=reinterpret_cast<std::uintptr_t>(e.info.BaseAddress);
  if(!e.info.RegionSize||a-base>=e.info.RegionSize)continue;
  if(e.tick!=tick&&now>=e.until){e.info.RegionSize=0;continue;}
  if(longOnly&&now>=e.until)continue;
  *out=e.info;

  const auto page=a&~std::uintptr_t(4095);
  out->BaseAddress=reinterpret_cast<void*>(page);out->RegionSize=base+e.info.RegionSize-page;
  return sizeof(*out);
 }
 const SIZE_T result=VirtualQuery(address,out,length);
 if(result==sizeof(*out)&&out->State==MEM_COMMIT&&!(out->Protect&(PAGE_GUARD|PAGE_NOACCESS))&&out->RegionSize){
  const ULONGLONG life=LongLife(*out);
  cache[next]=Entry{*out,tick,life?now+life:0};next=(next+1)%32;
 }
 return result;
}

}
