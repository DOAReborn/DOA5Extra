#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace AudioBridgeValidation {
enum class Kind { Unsupported, RollbackFilter, ToolsMenuFilter, RollbackChainedFilter };
inline uint32_t U32(const uint8_t* bytes) noexcept {
 uint32_t result=0;std::memcpy(&result,bytes,sizeof result);return result;
}
inline uint32_t Relative(uint32_t instructionEnd,uint32_t displacement) noexcept {
 return instructionEnd+displacement;
}
inline bool Entry(const uint8_t* entry,size_t length,const uint8_t* native,size_t nativeLength) noexcept {
 return entry&&native&&nativeLength>=6&&length>=nativeLength
  &&entry[0]==0xe9&&entry[5]==0x90
  &&std::memcmp(entry+6,native+6,nativeLength-6)==0;
}
inline Kind Classify(const uint8_t* bytes,size_t length) noexcept {
 if(!bytes)return Kind::Unsupported;

 static const uint8_t chained[]={0x8b,0x44,0x24,0x04,0x8b,0x0c,0x24,0x60,0x51,0x50,
  0xe8,0,0,0,0,0x83,0xc4,0x08,0x89,0x44,0x24,0x1c,0x61,0x85,0xc0,0x75,0x1b,
  0x83,0x3d,0,0,0,0,0,0x75,0x0c,0x55,0x8b,0xec,0x83,0xec,0x0c,0xff,0x25,
  0,0,0,0,0xff,0x25,0,0,0,0,0x83,0xc8,0xff,0xc3};
 if(length>=sizeof chained){
  bool match=true;
  for(size_t i=0;i<sizeof chained;i++){
   if((i>=11&&i<15)||(i>=29&&i<33)||(i>=44&&i<48)||(i>=50&&i<54))continue;
   if(bytes[i]!=chained[i]){match=false;break;}
  }
  if(match&&U32(bytes+29)==U32(bytes+50))return Kind::RollbackChainedFilter;
 }

 static const uint8_t rollback[]={0x8b,0x44,0x24,0x04,0x8b,0x0c,0x24,0x60,0x51,0x50,
  0xe8,0x11,0xff,0xff,0xff,0x83,0xc4,0x08,0x89,0x44,0x24,0x1c,0x61,
  0x85,0xc0,0x75,0x0c,0x55,0x8b,0xec,0x83,0xec,0x0c,0xff,0x25};
 static const uint8_t rejected[]={0x83,0xc8,0xff,0xc3};
 if(length>=43&&!std::memcmp(bytes,rollback,sizeof rollback)
   &&!std::memcmp(bytes+39,rejected,sizeof rejected))return Kind::RollbackFilter;

 static const uint8_t tools[]={0x83,0x3d,0,0,0,0,0x00,0x74,0x14,
  0x81,0x7c,0x24,0x04,0x01,0x00,0x01,0x00,0x74,0x10,
  0x81,0x7c,0x24,0x04,0x1d,0x00,0x01,0x00,0x74,0x06,0xff,0x25,
  0,0,0,0,0x31,0xc0,0xc3};
 if(length>=sizeof tools&&!std::memcmp(bytes,tools,2)
   &&!std::memcmp(bytes+6,tools+6,25)&&!std::memcmp(bytes+35,tools+35,3))
  return Kind::ToolsMenuFilter;
 return Kind::Unsupported;
}
inline bool ToolsTrampoline(const uint8_t* bytes,size_t length,uint32_t address,
 uint32_t nativeEntry,const uint8_t* native,size_t nativeLength) noexcept {
 return bytes&&native&&length>=11&&nativeLength>=6
  &&!std::memcmp(bytes,native,6)&&bytes[6]==0xe9
  &&Relative(address+11,U32(bytes+7))==nativeEntry+6;
}
}
