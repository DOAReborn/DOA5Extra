#pragma once
#include <windows.h>
#include <cstdint>

namespace ProtectedPointer {
struct Result { bool exchanged=false; bool protectionRestored=false; };

inline Result Exchange(PVOID volatile* field,PVOID wanted,PVOID expected) noexcept {
 Result result{};MEMORY_BASIC_INFORMATION info{};
 const auto address=reinterpret_cast<uintptr_t>(field);
 if(!address||address%sizeof(void*)||VirtualQuery(const_cast<PVOID*>(field),&info,sizeof info)!=sizeof info
  ||info.State!=MEM_COMMIT||(info.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return result;
 const auto end=reinterpret_cast<uintptr_t>(info.BaseAddress)+info.RegionSize;
 if(end<address||end-address<sizeof(void*))return result;
 const DWORD mode=info.Protect&0xff;
 if(mode!=PAGE_READONLY&&mode!=PAGE_READWRITE&&mode!=PAGE_WRITECOPY)return result;
 DWORD previous=0,ignored=0;
 if(!VirtualProtect(const_cast<PVOID*>(field),sizeof(void*),PAGE_READWRITE,&previous))return result;
 result.exchanged=InterlockedCompareExchangePointer(field,wanted,expected)==expected;
 result.protectionRestored=VirtualProtect(const_cast<PVOID*>(field),sizeof(void*),previous,&ignored)!=0;
 return result;
}
}
