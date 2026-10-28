#include "CostumeFooter.h"
#include "StateCostumesData.h"
#include "../commun/journal_extra.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdarg>
#include <cctype>
#include <vector>
#include <string>
#include <algorithm>
#include "InstalledPacks.h"
#include "RuntimeGlue.h"
#include "StateCostumes.h"
#include "SharedResources.h"
#include "Native990015Compat.h"
#include "StatefulReload.h"

static BYTE* Base;
static BYTE* ExistingTailCall;
static char Root[MAX_PATH], LogPath[MAX_PATH];
static bool StatefulRequested=false;
static bool StatefulMountAttempted=false;

static bool StatefulMountCompleted=false;
static int StatefulArchiveKey=-1;
static bool AllowNative990015=false;
static BYTE* At(uint32_t va) { return Base + va - 0x400000; }
static uint32_t U32(const BYTE* p) { uint32_t v; memcpy(&v,p,4); return v; }
static uint64_t U64(const BYTE* p) { uint64_t v; memcpy(&v,p,8); return v; }
static void Put32(BYTE* p,uint32_t v) { memcpy(p,&v,4); }
struct StartupState {uint32_t busy;BYTE started,cancelled,entered;};
static bool ColdStartup(const StartupState& s) {

 return s.busy==0xffffffff&&s.started==0&&s.cancelled==0&&s.entered==0;
}

static bool JournalActif=false;static DWORD JournalT0=0;
void JournalCostumes(const char* ligne) {
#ifdef PREFLIGHT_TOOL
 puts(ligne);
#else
 if(JournalActif)JournalExtra_Ligne("StatefulCostumes",ligne);
#endif
}
static void Log(const char* fmt,...) {
 char msg[1024]; va_list ap; va_start(ap,fmt); vsnprintf(msg,sizeof msg,fmt,ap); va_end(ap);
 JournalCostumes(msg);
}
struct Pack { uint32_t id;std::string folder,stem,lnk,bin;const AllowedPack* allowed=nullptr;std::vector<BYTE>bcm,blp,catalog,lnktable;uint64_t lnkSize=0;bool native990015=false;Native990015Compat::LockedFiles nativeFiles; };
static bool AliasSelfTest() {
 for(const auto& a:SharedResources){
  if(a.id>=0x705b||(a.pack!=990015&&a.pack!=990016)||!a.original[0]||strlen(a.unique)>=128||!strcmp(a.original,a.unique))return false;
  for(const auto& b:SharedResources)if(&a!=&b&&(a.id==b.id||!strcmp(a.unique,b.unique)))return false;
 }
 return sizeof(SharedResources)/sizeof(SharedResources[0])==1024;
}
static bool HashBytes(const std::vector<BYTE>& bytes,const char* expected) {
 HCRYPTPROV provider=0;HCRYPTHASH hash=0;BYTE digest[32];DWORD size=32;
 bool ok=CryptAcquireContextA(&provider,nullptr,nullptr,PROV_RSA_AES,CRYPT_VERIFYCONTEXT)&&CryptCreateHash(provider,CALG_SHA_256,0,0,&hash);
 if(ok)ok=CryptHashData(hash,bytes.data(),(DWORD)bytes.size(),0)&&CryptGetHashParam(hash,HP_HASHVAL,digest,&size,0)&&size==32;
 if(hash)CryptDestroyHash(hash);if(provider)CryptReleaseContext(provider,0);if(!ok)return false;
 char hex[65];for(int i=0;i<32;i++)snprintf(hex+2*i,3,"%02x",digest[i]);return !_stricmp(hex,expected);
}
static bool ReadSmall(const std::string& name,std::vector<BYTE>& data,DWORD max) {
 HANDLE h=CreateFileA(name.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
 if(h==INVALID_HANDLE_VALUE)return false;
 LARGE_INTEGER n;DWORD got=0;bool ok=GetFileSizeEx(h,&n)&&n.QuadPart>0&&n.QuadPart<=max;
 if(ok){data.resize((size_t)n.QuadPart);ok=ReadFile(h,data.data(),(DWORD)data.size(),&got,nullptr)&&got==data.size();}
 CloseHandle(h);return ok;
}
static bool HashFile(const std::string& name,uint64_t expectedSize,const char* expectedHash) {
 HANDLE file=CreateFileA(name.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
 if(file==INVALID_HANDLE_VALUE)return false;
 LARGE_INTEGER size{};HCRYPTPROV provider=0;HCRYPTHASH hash=0;BYTE digest[32],buffer[65536];DWORD got=0,length=32;
 bool ok=GetFileSizeEx(file,&size)&&uint64_t(size.QuadPart)==expectedSize
  &&CryptAcquireContextA(&provider,nullptr,nullptr,PROV_RSA_AES,CRYPT_VERIFYCONTEXT)&&CryptCreateHash(provider,CALG_SHA_256,0,0,&hash);
 while(ok){if(!ReadFile(file,buffer,sizeof buffer,&got,nullptr)){ok=false;break;}if(!got)break;ok=CryptHashData(hash,buffer,got,0)!=0;}
 if(ok)ok=CryptGetHashParam(hash,HP_HASHVAL,digest,&length,0)&&length==32;
 if(hash)CryptDestroyHash(hash);if(provider)CryptReleaseContext(provider,0);CloseHandle(file);
 if(!ok)return false;char hex[65];for(int i=0;i<32;i++)snprintf(hex+2*i,3,"%02x",digest[i]);return !_stricmp(hex,expectedHash);
}
static bool ValidateBCM(const std::vector<BYTE>& b,uint32_t id,bool installed=false) {

 if(b.size()<8||b.size()>0x1798||b[0]<3||b[0]>10||b[2]==0||b[3]!=0)return false;
 size_t pos=8;uint32_t total=0;unsigned type=b[1];uint32_t checksum=0;
 if(type!=0){
  if(!installed)return false;
  if(type==6){
   if(b.size()!=8||(b[2]!=16&&(id!=533560||b[2]>16)))return false;
   for(unsigned i=0;i<b[2];i++)total+=i*i*6;
   checksum=((total&4095)*(id+1))%(uint32_t(b[0])*id+17);
  } else if(type==1||type==3||type==4){
   if(b.size()!=size_t(8+b[2]))return false;
   for(unsigned i=8;i<b.size();i++){if((type==1&&b[i]>=49)||(type!=1&&b[i]>1))return false;total+=b[i]*b[i]*type;}
   if(type==1)checksum=((total&4095)*(id+1))%(uint32_t(b[0])*id+0x89);
   if(type==3)checksum=(((total&4095)+id)*((total&4095)+7))%(uint32_t(b[0])*id+0xa1);
   if(type==4)checksum=(((total&4095)+id)*((total&4095)+13))%(uint32_t(b[0])*id+0xc5);
  } else return false;
  return checksum==U32(b.data()+4);
 }
 for(unsigned i=0;i<b[2];i++){
  if(pos+2>b.size())return false;
  unsigned ch=b[pos],cos=b[pos+1];pos+=2;
  if(ch>=(installed?49:47))return false;
  total+=ch*cos;
  if(installed&&cos>=100){if(cos>=160)return false;continue;}
  if(cos>=60||pos+2>b.size())return false;
  unsigned detail=b[pos],pairs=b[pos+1];pos+=2;
  if(detail<1||detail>4||pairs<1||pairs>8||pos+2*pairs>b.size())return false;

  for(unsigned j=0;j<pairs;j++){if(b[pos]>=64||(!installed&&id!=990016&&b[pos+1]==0)||b[pos+1]>=32)return false;pos+=2;}
 }
 checksum=((total&4095)*(id+1))%(uint32_t(b[0])*id+17);
 return pos==b.size()&&checksum==U32(b.data()+4);
}
static bool Preflight(Pack& p) {
 const bool installed=p.allowed!=nullptr;const std::string stem=p.stem.empty()?std::to_string(p.id):p.stem;p.stem=stem;p.lnk=p.folder+"\\data\\"+stem+".lnk";p.bin=p.folder+"\\data\\"+stem+".bin";

 if(p.native990015&&(p.id!=990015||installed||stem!="990015"||!p.nativeFiles.OpenVerified(p.folder)))return false;

 if(p.lnk.size()>=256||p.bin.size()>=256)return false;
 if(!ReadSmall(p.folder+"\\"+stem+".bcm",p.bcm,0x1798)||!ValidateBCM(p.bcm,p.id,installed))return false;
 if(installed&&(!HashBytes(p.bcm,p.allowed->bcmHash)||p.bcm[1]!=p.allowed->type))return false;
 if(p.bcm[1]!=0&&p.bcm[1]!=6)return true;
 if(!ReadSmall(p.folder+"\\data\\"+stem+".blp",p.blp,0xa000)||p.blp.size()<16||memcmp(p.blp.data(),"1PIF",4))return false;
 uint32_t n=U32(p.blp.data()+4),off=U32(p.blp.data()+8);
 if(!n||n>2047||off<16||uint64_t(off)+20ull*n>p.blp.size())return false;
 std::vector<uint32_t> ids;
 for(uint32_t i=0;i<n;i++){
  const BYTE* r=p.blp.data()+off+20*i;uint32_t aid=U32(r);
  if(aid>=0x705b||U32(r+4)==0)return false;
  uint32_t flags=U32(r+8),crc=flags&0x1fffffff;
  if(!installed){if(flags||U32(r+12)||U32(r+16))return false;}
  else if(flags){if((flags>>29)!=7||crc==0||crc>=0x3a8d||U32(r+12)==0)return false;}
  else if(U32(r+12)||U32(r+16))return false;
  if(std::find(ids.begin(),ids.end(),aid)!=ids.end())return false;ids.push_back(aid);
 }
 if(!ReadSmall(p.bin,p.catalog,4*1024*1024)||p.catalog.size()<40||memcmp(p.catalog.data(),"LFMO",4))return false;
 if(installed&&(!HashBytes(p.catalog,p.allowed->binHash)||!HashBytes(p.blp,p.allowed->blpHash)))return false;
 if(U32(p.catalog.data()+4)!=1||U32(p.catalog.data()+8)!=n)return false;
 uint32_t entries=U32(p.catalog.data()+16);
 if(entries<40||uint64_t(entries)+12ull*n>p.catalog.size())return false;
 HANDLE h=CreateFileA(p.lnk.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
 if(h==INVALID_HANDLE_VALUE)return false;
 LARGE_INTEGER size;DWORD got=0;BYTE header[32];bool ok=GetFileSizeEx(h,&size)&&size.QuadPart>=32&&ReadFile(h,header,32,&got,nullptr)&&got==32;
 if(ok)ok=!memcmp(header,"PCDM",4)&&U64(header+8)==n&&U64(header+16)==uint64_t(size.QuadPart)&&(!installed||uint64_t(size.QuadPart)==p.allowed->lnkSize);
 if(ok){p.lnkSize=(uint64_t)size.QuadPart;p.lnktable.resize(n*32);ok=ReadFile(h,p.lnktable.data(),(DWORD)p.lnktable.size(),&got,nullptr)&&got==p.lnktable.size();}
 CloseHandle(h);if(!ok)return false;
 std::vector<uint32_t> seen;
 for(uint32_t i=0;i<n;i++){
  const BYTE* r=p.catalog.data()+entries+12*i;uint32_t idx=U32(r+4),name=U32(r+8);
  if(U32(r)!=0||idx>=n||name>=p.catalog.size()||!memchr(p.catalog.data()+name,0,p.catalog.size()-name))return false;
  if(std::find(seen.begin(),seen.end(),idx)!=seen.end())return false;seen.push_back(idx);
  const BYTE* f=p.lnktable.data()+idx*32;uint64_t at=U64(f),len=U64(f+8);
  if(at<32+32ull*n||!len||len>p.lnkSize||at>p.lnkSize-len||U64(f+16)!=len||U64(f+24)!=0)return false;

  if(!installed&&(idx!=i||len!=U32(p.blp.data()+off+20*i+4)))return false;
 }
 return true;
}
static bool StatefulFingerprint(const Pack& p,bool fullArchive) {
 return p.id==990016&&p.bcm.size()==542&&p.blp.size()>=16&&p.bcm[2]==73&&U32(p.blp.data()+4)==924
  &&HashBytes(p.bcm,"e2710d531cf4bb3283207589c20045f1d55dc72e1e51d6eeeacd93ed042dd62a")
  &&HashBytes(p.catalog,"7011acecd4c1d4271fd5a0ad2f221fd0b2956cd17ad386ae41c2ef944e1ecef7")
  &&HashBytes(p.blp,"0efda1f9781589a00adcde2ac60359b384098ec0faadbbb9ad2ee0a28b98da90")
  &&p.lnkSize==2355961856ull
  &&(!fullArchive||HashFile(p.lnk,2355961856ull,"1982591c2a6585a4c911dec72afed6b1cb4ffe843cbf8a12dc069bb7f018dd31"));
}

static bool NativePath(const std::string& input,std::string& output) {
 char path[256];DWORD length=GetFullPathNameA(input.c_str(),sizeof path,path,nullptr);
 if(!length||length>=sizeof path)return false;
 for(char* c=path;*c;c++)if(*c=='\\')*c='/';
 output=path;return true;
}

#ifndef PREFLIGHT_TOOL
struct MapEntry {int key;const char* object;};
static bool Readable(const void* ptr,size_t len) {
 if(!ptr||!len)return false;
 MEMORY_BASIC_INFORMATION info={};if(!VirtualQuery(ptr,&info,sizeof info)||info.State!=MEM_COMMIT||(info.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;
 return (uintptr_t)ptr+len>=(uintptr_t)ptr&&(uintptr_t)ptr+len<=(uintptr_t)info.BaseAddress+info.RegionSize;
}
static bool ReadStartup(StartupState& s) {
 if(!Readable(At(0x10bef18),4)||!Readable(At(0x137f9b8),3))return false;
 s.busy=U32(At(0x10bef18));s.started=*At(0x137f9b8);s.cancelled=*At(0x137f9b9);s.entered=*At(0x137f9ba);
 return true;
}
static bool CheckColdStartup(const char* phase) {
 StartupState s={};
 if(!ReadStartup(s)){Log("Refused at %s: startup state is unreadable.",phase);return false;}
 if(!ColdStartup(s)){
  Log("Refused at %s: native startup is not cold; busy=%08lx started=%u cancel=%u entered=%u.",phase,(unsigned long)s.busy,s.started,s.cancelled,s.entered);
  return false;
 }
 return true;
}
static bool Map(uint32_t countVA,uint32_t baseVA,MapEntry*& rows,uint32_t& count) {
 if(!Readable(At(countVA),4)||!Readable(At(baseVA),4))return false;
 count=U32(At(countVA));rows=(MapEntry*)(uintptr_t)U32(At(baseVA));
 return count<=4096&&(!count||Readable(rows,count*sizeof(MapEntry)));
}
static bool SamePath(const char* mounted,const std::string& path) {
 if(!Readable(mounted,256))return false;size_t len=strnlen(mounted,256);if(len==256)return false;
 char left[MAX_PATH],right[MAX_PATH];if(!GetFullPathNameA(mounted,MAX_PATH,left,nullptr)||!GetFullPathNameA(path.c_str(),MAX_PATH,right,nullptr))return false;
 for(char* c=left;*c;c++)if(*c=='/')*c='\\';for(char* c=right;*c;c++)if(*c=='/')*c='\\';
 return _stricmp(left,right)==0;
}
static bool Maps(MapEntry*& a,uint32_t& na,MapEntry*& b,uint32_t& nb) {
 return Map(0x1394a6c,0x1394a74,a,na)&&Map(0x1394a7c,0x1394a84,b,nb);
}
static int FindPath(MapEntry* rows,uint32_t n,const std::string& path) {
 for(uint32_t i=0;i<n;i++)if(SamePath(rows[i].object,path))return rows[i].key;return -1;
}
static StatefulReload::ArchivePath FindArchivePath(MapEntry* rows,uint32_t n,const std::string& path) {
 StatefulReload::ArchivePath found;
 for(uint32_t i=0;i<n;i++)if(SamePath(rows[i].object,path)){found.key=rows[i].key;++found.matches;}
 if(found.matches)for(uint32_t i=0;i<n;i++)if(rows[i].key==found.key)++found.keyMatches;
 return found;
}
static bool HasKey(MapEntry* rows,uint32_t n,int key) {for(uint32_t i=0;i<n;i++)if(rows[i].key==key)return true;return false;}
static bool RedirectAliases(const Pack& p) {
 if(p.native990015)return p.id==990015&&!p.allowed;
 if(p.allowed)return true;
 const unsigned n=U32(p.blp.data()+4),off=U32(p.blp.data()+8);
 struct Saved { LONG* slot;LONG original; };std::vector<Saved> changed;
 bool ok=true;
 for(unsigned i=0;i<n;i++){
  unsigned id=U32(p.blp.data()+off+20*i);const SharedResource* a=SharedAlias(id,p.id);
  LONG* slot=(LONG*)At(0x11dacf0+id*16);
  if(!a||!Readable(slot,4)){ok=false;break;}
  const char* current=(const char*)(uintptr_t)(DWORD)*slot;
  if(current==a->unique)continue;
  if(!Readable(current,256)||strnlen(current,256)==256||strcmp(current,a->original)){ok=false;break;}
  DWORD old=0;if(!VirtualProtect(slot,4,PAGE_READWRITE,&old)){ok=false;break;}
  LONG original=InterlockedExchange(slot,(LONG)(uintptr_t)a->unique);DWORD ignored;
  changed.push_back({slot,original});VirtualProtect(slot,4,old,&ignored);
 }
 if(!ok){
  for(auto it=changed.rbegin();it!=changed.rend();++it){DWORD old=0;if(VirtualProtect(it->slot,4,PAGE_READWRITE,&old)){InterlockedExchange(it->slot,it->original);DWORD ignored;VirtualProtect(it->slot,4,old,&ignored);}}
  Log("Rejected %u: original ASSET names changed during unique-name redirect.",p.id);return false;
 }
 Log("Unique-name redirects committed for %u: %u ASSET entries; module-owned strings retained for process lifetime.",p.id,n);return true;
}

static bool ValidateBindings(const Pack& p) {
 uint32_t n=U32(p.blp.data()+4),off=U32(p.blp.data()+8),table=U32(p.catalog.data()+16);
 for(uint32_t i=0;i<n;i++){
  const BYTE* desc=p.blp.data()+off+20*i;uint32_t aid=U32(desc);BYTE* slot=At(0x11dacf0)+16*aid;
  if(!Readable(slot,4))return false;const char* wanted=(const char*)(uintptr_t)U32(slot);
  if(!Readable(wanted,256)||strnlen(wanted,256)==256)return false;
  const SharedResource* shared=p.allowed?nullptr:SharedAlias(aid,p.id);
  const char* catalogWanted=wanted;
  if(!p.allowed){
   if(!shared)return false;
   catalogWanted=Native990015Compat::CatalogueName(p.native990015,wanted,shared->original,shared->unique);
   if(!catalogWanted)return false;
  }
  bool match=false;
  for(uint32_t j=0;j<n;j++){
   const BYTE* entry=p.catalog.data()+table+12*j;uint32_t idx=U32(entry+4),pos=U32(entry+8);
   const char* name=(const char*)p.catalog.data()+pos;if(*name=='/')name++;
   if(!strcmp(catalogWanted,name)){match=U64(p.lnktable.data()+idx*32+8)==U32(desc+4);break;}
  }
  if(!match)return false;
 }
 return true;
}
static unsigned LoadPack(Pack& p) {
 if(!Preflight(p)){Log("Rejected %s: bounded preflight or allowlist fingerprint failed.",p.stem.empty()?std::to_string(p.id).c_str():p.stem.c_str());return 0;}
 using ApplyBCM=void(__cdecl*)(const BYTE*,uint32_t,int,uint32_t);
 if(p.bcm[1]!=0&&p.bcm[1]!=6){

  ((ApplyBCM)At(0x4cd140))(p.bcm.data(),0xffffffff,0,p.id);
  Log("Applied allowlisted BCM %s type=%u; no archive registration or package count change.",p.stem.c_str(),p.bcm[1]);return 1;
 }
 MapEntry *a,*b;uint32_t na,nb;
 if(!Maps(a,na,b,nb)){Log("Rejected %s: archive maps invalid.",p.stem.c_str());return 0;}
 int la=FindPath(a,na,p.lnk),bi=FindPath(b,nb,p.bin);
 const auto mountDecision=Native990015Compat::DecideMount(p.native990015,la,bi);
 if(mountDecision==Native990015Compat::MountDecision::AlreadyMounted){
  const auto lnkIdentity=FindArchivePath(a,na,p.lnk),binIdentity=FindArchivePath(b,nb,p.bin);
  if(lnkIdentity.matches!=1||binIdentity.matches!=1||lnkIdentity.keyMatches!=1||binIdentity.keyMatches!=1){Log("Rejected %s: ambiguous existing archive identity.",p.stem.c_str());return 0;}

  if(!p.allowed&&!ValidateBindings(p)){Log("Rejected %s: existing custom ASSET bindings differ.",p.stem.c_str());return 0;}
  if(p.native990015)Log("Native 990015 verified: four exact SHA256 values, original ASSET names and matching archive key %d; no redirection or registration.",la);
  else Log("Retained %s: already mounted natively at %d.",p.stem.c_str(),la);

  ((ApplyBCM)At(0x4cd140))(p.bcm.data(),0xffffffff,0,p.id);
  Log("Reapplied %s costume availability/options; no archive mount or resource-counter increment.",p.stem.c_str());
  return 2;
 }
 if(mountDecision==Native990015Compat::MountDecision::Reject){Log("Rejected %s: required existing or consistent mount absent (%d, %d).",p.stem.c_str(),la,bi);return 0;}
 if(!ValidateBindings(p)){Log("Rejected %s: current ASSET names or stored-size bindings differ.",p.stem.c_str());return 0;}
 int key=100;while(key<10000&&(HasKey(a,na,key)||HasKey(b,nb,key)))key++;
 if(key==10000){Log("Rejected %s: no free archive key.",p.stem.c_str());return 0;}
 using Mount=void(__cdecl*)(int,const char*,const char*);
 using ApplyBLP=BYTE(__cdecl*)(const BYTE*);
 using Refresh=void(__cdecl*)();
 std::string nativeLnk,nativeBin;
 if(!NativePath(p.lnk,nativeLnk)||!NativePath(p.bin,nativeBin)){Log("Rejected %s: native archive path exceeds 255 bytes or cannot resolve.",p.stem.c_str());return 0;}
 if(p.id==990016){
  if(!RuntimeGlue::ActivateControlBeforeMount()){Log("Stateful refused before native mount: controller activation guards changed.");return 0;}
  StatefulMountAttempted=true;
 }
 if(!RedirectAliases(p))return 0;
 ((Mount)At(0x830490))(key,nativeLnk.c_str(),nativeBin.c_str());
 if(!Maps(a,na,b,nb)||FindPath(a,na,p.lnk)!=key||FindPath(b,nb,p.bin)!=key){Log("Rejected %s: native mount verification failed; no BLP or BCM applied.",p.stem.c_str());return 0;}
 if(p.id==990016){
  const auto la=FindArchivePath(a,na,p.lnk),bi=FindArchivePath(b,nb,p.bin);
  if(la.matches!=1||bi.matches!=1||la.keyMatches!=1||bi.keyMatches!=1){
   Log("Rejected 990016: native mount produced duplicate paths or keys; no BLP or BCM applied.");return 0;
  }
 }
 if(!((ApplyBLP)At(0x836ed0))(p.blp.data())){Log("Rejected %s: native BLP parser rejected pack; no BCM applied.",p.stem.c_str());return 0;}
 ((Refresh)At(0x4cc280))();
 ((ApplyBCM)At(0x4cd140))(p.bcm.data(),0xffffffff,p.bcm[1]==0,p.id);

 if(p.allowed&&p.id==533560)((void(__cdecl*)(int))At(0x837360))(1);
 if(p.id==990016){StatefulArchiveKey=key;StatefulMountCompleted=true;}
 if(p.native990015)Log("Native 990015 not mounted by Steam/CreamAPI: mounted by the module with its original ASSET names (four exact SHA256 values); no Steam change.");
 Log("Loaded %s at archive key %d: %u resources, BCM type=%u count=%u.",p.stem.c_str(),key,U32(p.blp.data()+4),p.bcm[1],p.bcm[2]);return 1;
}
static void RuntimeLog(const char* line) {Log("%s",line);}
struct PackReadLocks {
 HANDLE files[4]={INVALID_HANDLE_VALUE,INVALID_HANDLE_VALUE,INVALID_HANDLE_VALUE,INVALID_HANDLE_VALUE};
 bool Open(const Pack& p){
  const std::string paths[]={p.folder+"\\990016.bcm",p.folder+"\\data\\990016.bin",p.folder+"\\data\\990016.blp",p.folder+"\\data\\990016.lnk"};
  for(unsigned i=0;i<4;i++){files[i]=CreateFileA(paths[i].c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);if(files[i]==INVALID_HANDLE_VALUE)return false;}
  return true;
 }
 ~PackReadLocks(){for(HANDLE file:files)if(file!=INVALID_HANDLE_VALUE)CloseHandle(file);}
};
static bool ValidateUniqueStatefulAliases(const Pack& p) {
 const unsigned count=U32(p.blp.data()+4),offset=U32(p.blp.data()+8);
 for(unsigned i=0;i<count;i++){
  const unsigned id=U32(p.blp.data()+offset+20*i);const SharedResource* alias=SharedAlias(id,990016);
  const BYTE* slot=At(0x11dacf0)+id*16;
  if(!alias||!Readable(slot,4)){Log("Stateful rescan refused: unique alias slot unavailable for asset %u.",id);return false;}
  const char* actual=reinterpret_cast<const char*>(uintptr_t(U32(slot)));
  const size_t length=std::strlen(alias->unique)+1;
  if(!Readable(actual,length)||std::memcmp(actual,alias->unique,length)){
   Log("Stateful rescan refused: asset %u no longer has its exact 990016 unique name.",id);return false;
  }
 }
 return true;
}
struct NativeMetadataHooks {
 bool Writable(unsigned id) const {
  const BYTE* at=At(0x1268708)+id*8;MEMORY_BASIC_INFORMATION region{};
  if(!Readable(at,8)||VirtualQuery(at,&region,sizeof region)!=sizeof region)return false;
  const DWORD mode=region.Protect&0xff;
  return mode==PAGE_READWRITE||mode==PAGE_WRITECOPY||mode==PAGE_EXECUTE_READWRITE||mode==PAGE_EXECUTE_WRITECOPY;
 }
 bool Read(unsigned id,StatefulReload::Metadata& out) const {
  const BYTE* at=At(0x1268708)+id*8;if(!Readable(at,8))return false;
  out={U32(at),U32(at+4)};return true;
 }
 bool Apply(const unsigned char* blp) const {return reinterpret_cast<BYTE(__cdecl*)(const BYTE*)>(At(0x836ed0))(blp)!=0;}
};
static bool RestoreStatefulMetadata(const Pack& p) {

 constexpr size_t kParserSize=131;constexpr uint64_t kParserHash=0x0616e8245b30aaf4ULL;
 BYTE setter[]={0x55,0x8b,0xec,0x8b,0x45,0x0c,0x8b,0x10,0x8b,0x4d,0x08,
  0x89,0x14,0xcd,0,0,0,0,0x8b,0x40,0x04,0x89,0x04,0xcd,0,0,0,0,0x5d,0xc3};
 Put32(setter+14,uint32_t(uintptr_t(At(0x1268708))));Put32(setter+24,uint32_t(uintptr_t(At(0x126870c))));
 if(!Readable(At(0x836ed0),kParserSize)||StateCostumes::Data::Fnv64(At(0x836ed0),kParserSize)!=kParserHash
   ||!Readable(At(0x823570),sizeof setter)||std::memcmp(At(0x823570),setter,sizeof setter)){
  Log("Stateful rescan refused: native BLP parser or metadata setter signature differs.");return false;
 }
 NativeMetadataHooks hooks;StatefulReload::Result result;
 if(!StatefulReload::Restore(p.blp.data(),p.blp.size(),hooks,result)){
  Log("Stateful rescan failed: %s; asset=%u expected=%u/%08x actual=%u/%08x; changed=%u/%u.",
   StatefulReload::ErrorName(result.error),result.id,result.expected.size,result.expected.flags,
   result.actual.size,result.actual.flags,result.changed,result.count);return false;
 }
 Log("Stateful rescan metadata verified: %u/%u records restored at existing archive key %d; no mount, BCM append or refresh.",
  result.changed,result.count,StatefulArchiveKey);return true;
}
static unsigned LoadStatefulPack() {
 Pack p;p.id=990016;p.folder=std::string(Root)+"DLC\\990016";
 PackReadLocks hold;

 if(!hold.Open(p)){Log("Stateful refused: four archive read locks unavailable.");return 0;}
 if(!Preflight(p)){Log("Stateful refused: archive/BCM/BLP structural preflight failed.");return 0;}
 if(!StatefulFingerprint(p,true)){Log("Stateful refused: exact four-file 990016 fingerprint differs.");return 0;}
 if(!ValidateBindings(p)){Log("Stateful refused: ASSET names or catalogue stored-size bindings differ.");return 0;}
 if(!RuntimeGlue::RendererArmed()){Log("Stateful refused: renderer/reset hooks no longer match.");return 0;}
 if(!RuntimeGlue::ValidateAlternateTables()){Log("Stateful refused: ALT pointer, pairs or asset-name guard failed.");return 0;}
 MapEntry *a,*b;uint32_t na,nb;
 if(!Maps(a,na,b,nb)){Log("Stateful refused: native archive maps invalid.");return 0;}
 const auto lnk=FindArchivePath(a,na,p.lnk),bin=FindArchivePath(b,nb,p.bin);
 const auto action=StatefulReload::Decide(lnk,bin,StatefulMountCompleted,StatefulArchiveKey);
 if(action==StatefulReload::Action::Reject){
  Log("Stateful refused: archive identity differs; LNK=%d paths=%u keys=%u BIN=%d paths=%u keys=%u committed=%u expected=%d.",
   lnk.key,lnk.matches,lnk.keyMatches,bin.key,bin.matches,bin.keyMatches,unsigned(StatefulMountCompleted),StatefulArchiveKey);return 0;
 }
 if(action==StatefulReload::Action::RestoreMetadata){
  if(!ValidateUniqueStatefulAliases(p)||!RestoreStatefulMetadata(p))return 0;
  using ApplyBCM=void(__cdecl*)(const BYTE*,uint32_t,int,uint32_t);
  ((ApplyBCM)At(0x4cd140))(p.bcm.data(),0xffffffff,0,p.id);
  Log("Reapplied 990016 costume availability/options LAST; no archive mount or resource-counter increment.");
  return 2;
 }
 if(!RuntimeGlue::InstallAlternateTables()||!RuntimeGlue::RendererArmed()){
  RuntimeGlue::RollbackAlternateTables();Log("Stateful refused before mount: ALT commit or renderer readiness failed.");return 0;
 }
 StatefulMountAttempted=false;unsigned result=LoadPack(p);
 if(!result){
  if(!StatefulMountAttempted)RuntimeGlue::RollbackAlternateTables();
  else Log("Stateful post-mount failure: retain renderer and ALT storage because native archive operations may have applied partially; restart/rollback required.");
 }else Log("Stateful 990016 mounted LAST: 73 costumes,924 resources (candidate 84: +16 Zack Island LR49 + Phase4 Gust 035, 226 state models; candidate 82: +30 Zack Island two-form swimsuits x15, 209 state models; candidate 81: +16 VF swimsuits, 194; candidate 56: +92 alt resources; candidate 44: +99 destructible PS4 models, 172 state models); 16 Schoolgirl details=4 after ALT commit. Prototype PB/taunt and bounded PL victim reaction.");
 return result;
}
static void __cdecl LoadLocalPacks() {

 unsigned result[3]={};
 Log("Begin local pass: 990015 then optional stateful990016 last.");
 Pack custom;custom.id=990015;custom.native990015=AllowNative990015;custom.folder=std::string(Root)+"DLC\\990015";result[LoadPack(custom)]++;
 if(StatefulRequested){
  if(result[0]){Log("Stateful refused before mount: an earlier local package failed.");result[0]++;}
  else result[LoadStatefulPack()]++;
 }
 Log("Local pass complete: loaded_or_applied=%u already_mounted=%u rejected=%u configured=%u. Existing tail handler follows.",result[1],result[2],result[0],1u+(StatefulRequested?1u:0u));
}
static bool Signature(uint32_t va,const BYTE* bytes,size_t n) {return Readable(At(va),n)&&!memcmp(At(va),bytes,n);}
static bool KnownAutoLink(HMODULE owner) {
 static HMODULE checked=nullptr;static bool verified=false;if(owner==checked)return verified;
 char path[MAX_PATH];if(!GetModuleFileNameA(owner,path,MAX_PATH))return false;
 HANDLE file=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);if(file==INVALID_HANDLE_VALUE)return false;
 HCRYPTPROV provider=0;HCRYPTHASH hash=0;BYTE digest[32],buffer[65536];DWORD got=0,size=32;
 bool ok=CryptAcquireContextA(&provider,nullptr,nullptr,PROV_RSA_AES,CRYPT_VERIFYCONTEXT)&&CryptCreateHash(provider,CALG_SHA_256,0,0,&hash);
 while(ok){if(!ReadFile(file,buffer,sizeof buffer,&got,nullptr)){ok=false;break;}if(!got)break;ok=CryptHashData(hash,buffer,got,0)!=0;}
 if(ok)ok=CryptGetHashParam(hash,HP_HASHVAL,digest,&size,0)&&size==32;
 if(hash)CryptDestroyHash(hash);if(provider)CryptReleaseContext(provider,0);CloseHandle(file);
 const BYTE expected[]={0x0e,0xa5,0xce,0x97,0x0a,0x98,0x06,0xda,0x20,0x74,0xc1,0xbf,0x32,0xd1,0xda,0x24,0xcf,0x94,0x23,0x49,0x12,0x82,0x8d,0x29,0x7c,0xf9,0x90,0x45,0xca,0x3f,0xcc,0xef};
 checked=owner;verified=ok&&!memcmp(digest,expected,32);return verified;
}
static bool TailExpected() {
 ExistingTailCall=nullptr;
 const BYTE rest[]={0x33,0xcd,0x33,0xc0,0x5b,0xe8};if(!Signature(0x4cd88b,rest,sizeof rest))return false;
 BYTE store[]={0x89,0x1d,0,0,0,0};Put32(store+2,(uint32_t)(uintptr_t)At(0x10bef18));
 if(Signature(0x4cd885,store,sizeof store)){

  std::string chained=std::string(Root)+"dinput8Hooked.dll";
  return GetModuleHandleA("dinput8Hooked.dll")==nullptr&&GetFileAttributesA(chained.c_str())==INVALID_FILE_ATTRIBUTES;
 }
 BYTE* site=At(0x4cd885);if(!Readable(site,6)||site[0]!=0xe8||site[5]!=0x90)return false;
 int32_t rel;memcpy(&rel,site+1,4);BYTE* target=site+5+rel;
 HMODULE owner=GetModuleHandleA("dinput8Hooked.dll");if(!owner||target!=(BYTE*)owner+0x4bb80)return false;
 const BYTE bridge[]={0x60,0x53,0xe8,0x09,0x00,0x00,0x00,0x61,0xc3};
 if(!Readable(target,sizeof bridge)||memcmp(target,bridge,sizeof bridge)||!KnownAutoLink(owner))return false;
 ExistingTailCall=target;return true;
}
static bool Expected() {
 const BYTE mount[]={0x55,0x8b,0xec,0x8b,0x45,0x0c,0x56,0x8b,0x75,0x08,0x6a,0x00,0x6a,0x01,0x50,0x56};
 const BYTE blp[]={0x55,0x8b,0xec,0x8b,0x45,0x08,0x83,0xec,0x10,0x85,0xc0};
 const BYTE bcm[]={0x55,0x8b,0xec,0x83,0xec,0x30,0xa1};
 const BYTE refresh[]={0x55,0x8b,0xec,0x6a,0xff,0x68};
 const BYTE special[]={0x55,0x8b,0xec,0x80,0x7d,0x08,0x00,0x0f,0x95,0xc0};
 return Signature(0x830490,mount,sizeof mount)&&Signature(0x836ed0,blp,sizeof blp)&&Signature(0x4cd140,bcm,sizeof bcm)&&Signature(0x4cc280,refresh,sizeof refresh)&&Signature(0x837360,special,sizeof special)&&TailExpected()&&U32(At(0x4cd147))==(uint32_t)(uintptr_t)At(0x130e700);
}
static bool Hook() {
 if(!Expected())return false;
 if(!CheckColdStartup("validated AutoLink tail"))return true;
 Pack p;p.id=990015;p.native990015=AllowNative990015;p.folder=std::string(Root)+"DLC\\990015";if(!Preflight(p)){Log("Refused: pack preflight failed before hook installation.");return true;}
 if(StatefulRequested){
  Pack state;state.id=990016;state.folder=std::string(Root)+"DLC\\990016";
  if(!Preflight(state)||!StatefulFingerprint(state,false)||!RuntimeGlue::Prepare(Base,RuntimeLog)
   ||!CheckColdStartup("before renderer arm")||!RuntimeGlue::ArmRenderer()){
   Log("Refused: requested stateful prototype failed its startup guards; no local completion hook installed.");return true;
  }
 }
 BYTE originalTail[6];memcpy(originalTail,At(0x4cd885),6);
 BYTE* gateway=(BYTE*)VirtualAlloc(nullptr,64,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);if(!gateway){Log("Refused: trampoline allocation.");return true;}

 gateway[0]=0x9c;gateway[1]=0x60;gateway[2]=0xe8;Put32(gateway+3,(uint32_t)((BYTE*)&LoadLocalPacks-(gateway+7)));gateway[7]=0x61;gateway[8]=0x9d;
 memcpy(gateway+9,At(0x4cd885),6);

 if(ExistingTailCall)Put32(gateway+10,(uint32_t)(ExistingTailCall-(gateway+14)));
 gateway[15]=0xe9;Put32(gateway+16,(uint32_t)(At(0x4cd88b)-(gateway+20)));
 DWORD old=0;if(!VirtualProtect(gateway,64,PAGE_EXECUTE_READ,&old)){VirtualFree(gateway,0,MEM_RELEASE);return true;}
 FlushInstructionCache(GetCurrentProcess(),gateway,20);
 BYTE patch[6]={0xe9,0,0,0,0,0x90};Put32(patch+1,(uint32_t)(gateway-(At(0x4cd885)+5)));
 if(!CheckColdStartup("final prepatch check")||memcmp(originalTail,At(0x4cd885),6)){
  VirtualFree(gateway,0,MEM_RELEASE);Log("Refused: native startup or another tail patch changed during preflight.");return true;
 }
 if(!VirtualProtect(At(0x4cd885),6,PAGE_EXECUTE_READWRITE,&old)){VirtualFree(gateway,0,MEM_RELEASE);Log("Refused: hook protection.");return true;}
 memcpy(At(0x4cd885),patch,6);DWORD ignored;VirtualProtect(At(0x4cd885),6,old,&ignored);FlushInstructionCache(GetCurrentProcess(),At(0x4cd885),6);
 Log("Armed: Check Pass Thread completion hook RVA 0xCD885; tail mode=%s; no Steam hooks.",ExistingTailCall?"verified AutoLink chain":"native store");return true;
}
#include "CommonEffectArchive.inc"
static DWORD WINAPI Worker(void*) {
 std::string ini=Extra_Ini();
 RuntimeGlue::EnableNativeTransitionBurst(GetPrivateProfileIntA("StatefulCostumes","EnableNativeTransitionBurst",0,ini.c_str())==1);
 RuntimeGlue::EnableNativeTransitionSound(GetPrivateProfileIntA("StatefulCostumes","EnableNativeTransitionSound",0,ini.c_str())==1);
 RuntimeGlue::EnableOriginalSchoolgirlParticles(GetPrivateProfileIntA("StatefulCostumes","EnableOriginalSchoolgirlParticles",0,ini.c_str())==1);
 const std::string rollbackIni=std::string(Root)+"scripts\\DOA5Rollback.ini";

 RuntimeGlue::EnableOnlineCostumeTrial(
  GetPrivateProfileIntA("StatefulCostumes","EnableOnlineCostumeTrial",1,ini.c_str())==1,
  GetPrivateProfileIntA("Rollback","Enabled",1,rollbackIni.c_str())==0);
 RuntimeGlue::EnableRollbackCostumes(GetPrivateProfileIntA("StatefulCostumes","EnableRollbackCostumes",1,ini.c_str())==1,
  GetPrivateProfileIntA("StatefulCostumesDebug","RollbackLabo",0,ini.c_str())==1);
 AllowNative990015=GetPrivateProfileIntA("StatefulCostumes","AllowNative990015",0,ini.c_str())==1;
 StatefulRequested=GetPrivateProfileIntA("StatefulCostumes","EnableStatefulCostumes",0,ini.c_str())==1;
 Log("Native 990015 compatibility opt-in=%u (default0); exact existing 17-costume pack only; mounted by Steam/CreamAPI if present, otherwise by this module (no cream_api.ini line needed); no Steam changes.",AllowNative990015?1:0);
 Log("Combined PROTOTYPE loaded; EnableStatefulCostumes=%u (default0); expected cold busyFFFFFFFF/flags0. Native round/training reset observer; bounded PL victim reaction; CB unavailable; runtime untested.",StatefulRequested?1:0);
 if(!CheckColdStartup("ASI worker entry"))return 0;

 if(GetPrivateProfileIntA("StatefulCostumes","EnableOriginalSchoolgirlParticles",0,ini.c_str())==1
    &&!ArmCommonEffectArchive()){
  RuntimeGlue::EnableOriginalSchoolgirlParticles(false);
  Log("Original particles OFF (common.lnk is not the pack version); costume module continues without them.");
 }
 Log("Observed initial cold state; waiting for verified AutoLink chain.");
 ULONGLONG start=GetTickCount64();while(GetTickCount64()-start<30000){
  if(!CheckColdStartup("waiting for supported tail"))return 0;

  if(GetModuleHandleA("DOA5LR-StatefulCostumes.prototype.asi")){Log("Refused: separate DOA5LR-StatefulCostumes.prototype.asi is loaded.");return 0;}
  if(Hook()){

   if(RuntimeGlue::RendererArmed()){
    Log("Costume footer hook: %s",CostumeFooter::Install(Base)?"armed":"refused signature");
    ULONGLONG lastFlush=0;
    for(;;){
     CostumeFooter::Poll();
     const ULONGLONG now=GetTickCount64();
     if(now-lastFlush>=250){RuntimeGlue::FlushLogs();lastFlush=now;}
     Sleep(15);
    }
   }
   return 0;
  }Sleep(1);
 }
 Log("Refused: supported native function signatures not found.");return 0;
}
extern "C" void StatefulCostumes_Demarrer(void) {
 Base=(BYTE*)GetModuleHandleA(nullptr);
 if(!GetModuleFileNameA(nullptr,Root,MAX_PATH))return;char* end=strrchr(Root,'\\');if(!end)return;end[1]=0;
 JournalActif=true;JournalT0=GetTickCount();(void)LogPath;(void)JournalT0;

 HANDLE t=CreateThread(nullptr,0,Worker,nullptr,0,nullptr);if(t)CloseHandle(t);
}
#else
int main(int argc,char** argv) {
 if(argc==3&&!strcmp(argv[1],"--native-990015")){
  Pack p;p.id=990015;p.native990015=true;p.folder=argv[2];const bool ok=Preflight(p);
  printf("{\"ok\":%s,\"dlc_id\":990015,\"costumes\":%u,\"resources\":%u,\"all_four_files_sha256\":%s,\"native_mount_required\":false,\"game_process_accessed\":false}\n",ok?"true":"false",ok?p.bcm[2]:0,ok?U32(p.blp.data()+4):0,ok?"true":"false");return ok?0:1;
 }
 if(argc==2&&!strcmp(argv[1],"--alias-selftest")){bool ok=AliasSelfTest();printf("{\"ok\":%s,\"unique_resource_names\":1024,\"game_process_accessed\":false}\n",ok?"true":"false");return ok?0:1;}
 if(argc==2&&!strcmp(argv[1],"--native-path-selftest")){
  std::string result;
  bool ok=NativePath("D:\\SteamLibrary\\steamapps\\common\\Dead or Alive 5 Last Round\\DLC\\990016\\data\\990016.lnk",result)
   &&result=="D:/SteamLibrary/steamapps/common/Dead or Alive 5 Last Round/DLC/990016/data/990016.lnk";
  std::string again;ok=NativePath(result,again)&&again==result&&ok;
  ok=!NativePath("D:/"+std::string(300,'x'),again)&&ok;
  printf("{\"ok\":%s,\"checks\":3,\"runtime_tested\":false}\n",ok?"true":"false");return ok?0:1;
 }
 if(argc==2&&!strcmp(argv[1],"--event-selftest"))return RuntimeGlue::EventSelfTest()?0:1;
 if(argc==2&&!strcmp(argv[1],"--reload-alias-selftest"))return RuntimeGlue::ReloadAliasSelfTest()?0:1;
 if(argc==3&&!strcmp(argv[1],"--integrated-event-selftest"))return RuntimeGlue::IntegratedEventSelfTest(argv[2])?0:1;
 if(argc==3&&!strcmp(argv[1],"--online-trial-selftest"))return RuntimeGlue::OnlineTrialSelfTest(argv[2])?0:1;
 if(argc==3&&!strcmp(argv[1],"--stateful")){
  Pack p;p.id=990016;p.folder=argv[2];bool ok=Preflight(p)&&StatefulFingerprint(p,true);
  printf("{\"ok\":%s,\"dlc_id\":990016,\"costumes\":%u,\"resources\":%u,\"all_four_files_sha256\":%s,\"runtime_tested\":false}\n",ok?"true":"false",ok?p.bcm[2]:0,ok?U32(p.blp.data()+4):0,ok?"true":"false");return ok?0:1;
 }
 if(argc==2&&!strcmp(argv[1],"--startup-state-selftest")){
  const uint32_t states[]={0xffffffff,0,1,5};unsigned checked=0,accepted=0;
  for(uint32_t busy:states)for(unsigned flags=0;flags<8;flags++){
   StartupState s={busy,BYTE(flags&1),BYTE((flags>>1)&1),BYTE((flags>>2)&1)};
   bool expect=busy==0xffffffff&&flags==0;if(ColdStartup(s)!=expect){puts("FAIL startup-state discriminator");return 1;}
   accepted+=ColdStartup(s);checked++;
  }
  printf("PASS %u startup-state combinations; %u accepted (FFFFFFFF,0,0,0).\n",checked,accepted);return 0;
 }
 if(argc!=2){puts("Usage: NativeLocalCostumes-preflight.exe <990015 package directory>");return 2;}
 Pack p;p.id=990015;p.folder=argv[1];if(!Preflight(p)){puts("FAIL package preflight");return 1;}
 printf("PASS package: %u resources, %u costumes, %llu LNK bytes\n",U32(p.blp.data()+4),p.bcm[2],(unsigned long long)p.lnkSize);
 auto bad=p.bcm;bad[4]^=1;if(ValidateBCM(bad,p.id)){puts("FAIL corrupt checksum accepted");return 1;}
 bad=p.bcm;bad[11]=9;if(ValidateBCM(bad,p.id)){puts("FAIL excessive hair count accepted");return 1;}
 bad=p.bcm;bad.pop_back();if(ValidateBCM(bad,p.id)){puts("FAIL truncated BCM accepted");return 1;}
 puts("PASS rejected invalid checksum, excessive hair count, and truncated BCM.");return 0;
}
#endif
