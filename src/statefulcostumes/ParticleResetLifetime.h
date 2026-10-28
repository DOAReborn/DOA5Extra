#pragma once
#include <cstdint>
#include <cstring>

namespace ParticleResetLifetime {
constexpr unsigned NativeSize=0xd0;
constexpr unsigned TuningDoneOffset=NativeSize+sizeof(uint32_t);
constexpr unsigned ObjectSize=TuningDoneOffset+sizeof(uint32_t);
constexpr unsigned AllocationSize=12+ObjectSize;
inline void Stamp(void* object,uint32_t epoch) noexcept {
 std::memcpy(static_cast<unsigned char*>(object)+NativeSize,&epoch,sizeof epoch);
}
inline bool Keep(void* object,uint32_t epoch) noexcept {
 uint32_t created=0;
 std::memcpy(&created,static_cast<unsigned char*>(object)+NativeSize,sizeof created);
 if(created==epoch)return true;
 const uint32_t inactive=0;
 std::memcpy(static_cast<unsigned char*>(object)+4,&inactive,sizeof inactive);
 return false;
}
}
