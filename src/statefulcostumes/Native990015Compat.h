#pragma once
#include <windows.h>
#include <wincrypt.h>
#include <cstdio>
#include <cstring>
#include <string>

namespace Native990015Compat {
struct ExpectedFile { const char* relative; unsigned long long size; const char* sha256; };
static const ExpectedFile Files[] = {
 {"990015.bcm",180,"dd99f85bf59df388719ab58c2e0bd06cdd948875d34a71e76ba8d75d6cb863ca"},
 {"data\\990015.bin",6202,"e745e90f5fff0d8d1aacab0ec69c82fd2654fdff7cc489aac737a0edd70bcb1f"},
 {"data\\990015.blp",2016,"c80b040d36b058a8f81d90c68ac5b77228c48c0eb7e3fc7c739644c5545b4ae8"},
 {"data\\990015.lnk",171933696ull,"5fa341aeb6bdb5746c958df85976f7043867989fd2a1a66613621cfdb43266e4"}
};

class LockedFiles {
 HANDLE handles_[4] = {INVALID_HANDLE_VALUE,INVALID_HANDLE_VALUE,INVALID_HANDLE_VALUE,INVALID_HANDLE_VALUE};
 bool verified_ = false;
 std::string folder_;
 unsigned verifiedCount_ = 0;
 static bool Fingerprint(HANDLE file,const ExpectedFile& expected) {
  LARGE_INTEGER size{};HCRYPTPROV provider=0;HCRYPTHASH hash=0;
  BYTE digest[32],buffer[65536];DWORD got=0,length=32;
  bool ok=GetFileSizeEx(file,&size)&&static_cast<unsigned long long>(size.QuadPart)==expected.size
   &&CryptAcquireContextA(&provider,nullptr,nullptr,PROV_RSA_AES,CRYPT_VERIFYCONTEXT)
   &&CryptCreateHash(provider,CALG_SHA_256,0,0,&hash);
  while(ok){
   if(!ReadFile(file,buffer,sizeof buffer,&got,nullptr)){ok=false;break;}
   if(!got)break;
   ok=CryptHashData(hash,buffer,got,0)!=0;
  }
  if(ok)ok=CryptGetHashParam(hash,HP_HASHVAL,digest,&length,0)&&length==32;
  if(hash)CryptDestroyHash(hash);
  if(provider)CryptReleaseContext(provider,0);
  if(!ok)return false;
  char hex[65];for(unsigned i=0;i<32;i++)snprintf(hex+2*i,3,"%02x",digest[i]);
  return !strcmp(hex,expected.sha256);
 }
public:
 LockedFiles() = default;
 LockedFiles(const LockedFiles&) = delete;
 LockedFiles& operator=(const LockedFiles&) = delete;
 ~LockedFiles(){Close();}
 void Close() {
  for(auto& file:handles_)if(file!=INVALID_HANDLE_VALUE){CloseHandle(file);file=INVALID_HANDLE_VALUE;}
  verified_=false;folder_.clear();
 }
 bool OpenVerified(const std::string& folder) {
  if(verified_)return folder==folder_;
  Close();verifiedCount_=0;
  for(unsigned i=0;i<4;i++){
   const std::string path=folder+"\\"+Files[i].relative;
   handles_[i]=CreateFileA(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
   if(handles_[i]==INVALID_HANDLE_VALUE){Close();return false;}
  }
  for(unsigned i=0;i<4;i++){
   if(!Fingerprint(handles_[i],Files[i])){Close();return false;}
   verifiedCount_++;
  }
  folder_=folder;verified_=true;return true;
 }
 unsigned VerifiedCount() const {return verifiedCount_;}
};

inline const char* CatalogueName(bool nativeNames,const char* current,const char* original,const char* unique) {
 if(!current||!original||!unique)return nullptr;
 if(nativeNames)return !strcmp(current,original)?original:nullptr;
 return (!strcmp(current,original)||!strcmp(current,unique))?unique:nullptr;
}
enum class MountDecision { Reject, AlreadyMounted, Mount };
inline MountDecision DecideMount(bool nativeAlreadyMounted,int lnkKey,int binKey) {
 if(lnkKey>=0||binKey>=0)
  return lnkKey>=0&&lnkKey==binKey?MountDecision::AlreadyMounted:MountDecision::Reject;

 (void)nativeAlreadyMounted;
 return MountDecision::Mount;
}
}
