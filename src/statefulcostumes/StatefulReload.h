#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "SharedResources.h"

namespace StatefulReload {
enum class Action { Reject, InitialMount, RestoreMetadata };
struct ArchivePath { int key=-1; unsigned matches=0,keyMatches=0; };
inline Action Decide(ArchivePath lnk,ArchivePath bin,bool committed,int committedKey) noexcept {
 if(!lnk.matches&&!bin.matches)return committed?Action::Reject:Action::InitialMount;
 if(lnk.matches!=1||bin.matches!=1||lnk.keyMatches!=1||bin.keyMatches!=1||lnk.key<0||lnk.key!=bin.key
    ||!committed||lnk.key!=committedKey)return Action::Reject;
 return Action::RestoreMetadata;
}

inline const char* AlternateAlias(unsigned id,const char* original,bool active) noexcept {
 if(!active||!original)return nullptr;
 const SharedResource* resource=SharedAlias(id,990016);
 return resource&&!std::strcmp(resource->original,original)?resource->unique:nullptr;
}
struct Metadata { uint32_t size=0,flags=0; };
inline bool Same(Metadata a,Metadata b) noexcept {return a.size==b.size&&a.flags==b.flags;}
inline uint32_t Read32(const unsigned char* at) noexcept {uint32_t value;std::memcpy(&value,at,4);return value;}
enum class Error { None, InvalidBLP, InvalidRecord, DuplicateId, Unwritable, Unreadable, NativeRejected, ReadbackMismatch };
struct Result { Error error=Error::None; unsigned id=0,count=0,changed=0; Metadata expected{},actual{}; };
inline const char* ErrorName(Error e) noexcept {
 switch(e){
 case Error::None:return "none";case Error::InvalidBLP:return "invalid BLP header/range";
 case Error::InvalidRecord:return "invalid raw-resource record";case Error::DuplicateId:return "duplicate resource ID";
 case Error::Unwritable:return "metadata destination is not writable";case Error::Unreadable:return "metadata destination is unreadable";
 case Error::NativeRejected:return "native BLP parser rejected validated data";case Error::ReadbackMismatch:return "metadata readback differs";
 }return "unknown";
}

template<class Hooks> bool Restore(const unsigned char* blp,size_t length,Hooks& hooks,Result& out) noexcept {
 out={};
 if(!blp||length<16||std::memcmp(blp,"1PIF",4)){out.error=Error::InvalidBLP;return false;}
 const uint32_t count=Read32(blp+4),offset=Read32(blp+8);
 if(!count||count>2047||offset<16||offset>length||count>(length-offset)/20){out.error=Error::InvalidBLP;return false;}
 out.count=count;

 for(unsigned i=0;i<count;++i){
  const unsigned char* row=blp+offset+20*i;out.id=Read32(row);out.expected={Read32(row+4),Read32(row+8)};
  if(out.id>=0x705b||!out.expected.size||out.expected.flags||Read32(row+12)||Read32(row+16)){
   out.error=Error::InvalidRecord;return false;
  }
  for(unsigned j=0;j<i;++j)if(Read32(blp+offset+20*j)==out.id){out.error=Error::DuplicateId;return false;}
  if(!hooks.Writable(out.id)){out.error=Error::Unwritable;return false;}
  if(!hooks.Read(out.id,out.actual)){out.error=Error::Unreadable;return false;}
  if(!Same(out.actual,out.expected))++out.changed;
 }
 if(out.changed&&!hooks.Apply(blp)){out.error=Error::NativeRejected;return false;}
 for(unsigned i=0;i<count;++i){
  const unsigned char* row=blp+offset+20*i;out.id=Read32(row);out.expected={Read32(row+4),Read32(row+8)};
  if(!hooks.Read(out.id,out.actual)){out.error=Error::Unreadable;return false;}
  if(!Same(out.actual,out.expected)){out.error=Error::ReadbackMismatch;return false;}
 }
 return true;
}
}
