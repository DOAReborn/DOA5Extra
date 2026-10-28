using NativeTextFn=void(__cdecl*)(void*,void*);
NativeTextFn nativeText=nullptr;
volatile LONG nativeStamp=0;
void NativeLog(const char* line){ ::JournalCostumes(line); }
void __cdecl NativeText(void* descriptor,void* context){
 nativeText(descriptor,context);
 uint32_t gui=0;
 if(!InterlockedCompareExchange(&eligible,0,0)||!Read(At(0x138a7ac),gui)||gui!=4
   ||static_cast<DWORD>(GetTickCount()-static_cast<DWORD>(InterlockedCompareExchange(&stamp,0,0)))>=200)return;
 alignas(16) unsigned char copy[0x84]{};
 SIZE_T got=0;
 if(!ReadProcessMemory(GetCurrentProcess(),descriptor,copy,sizeof copy,&got)||got!=sizeof copy)return;
 float x=0,y=0;memcpy(&x,copy+0x54,4);memcpy(&y,copy+0x58,4);
 if(!(y>500&&y<760))return;
 uintptr_t pointer=0;memcpy(&pointer,copy+0x40,4);if(!pointer)return;
 wchar_t text[256]{};
 for(unsigned i=0;i<255;++i){if(!Read(pointer+i*2,text[i]))return;if(!text[i])break;}

 if(!wcsstr(text,L"@[pad:Decide]"))return;
 const bool a=StateCostumes::IsTransitionEnabled(false),b=StateCostumes::IsTransitionEnabled(true);
 const wchar_t* label=a&&b?L"@[pad:R1] Costume effects: ON":!a&&!b?L"@[pad:R1] Costume effects: OFF":L"@[pad:R1] Costume effects: MIXED";
 memcpy(copy+0x40,&label,4);

 const float footerX=x;
 x-=270.0f;memcpy(copy+0x54,&x,4);
 float unlimited=0;memcpy(copy+0x20,&unlimited,4);memcpy(copy+0x24,&unlimited,4);
 nativeText(copy,context);
 InterlockedExchange(&nativeStamp,static_cast<LONG>(GetTickCount()));

 static unsigned lastCostume[2]={0xffff,0xffff};static DWORD changed[2]{};
 for(unsigned n=0;n<5;++n){
  unsigned char id[8]{};
  if(!Read(At(0x2453f70+n*0x50),id)||id[2]==0xff||id[7]==0xff||id[3]>1)continue;
  const unsigned p=id[3],now=unsigned(id[2])<<8|id[7];

  if(!(InterlockedCompareExchange(&playerStage,0,0)&(1<<p)))continue;
  if(now!=lastCostume[p]){lastCostume[p]=now;changed[p]=GetTickCount();}
  if(GetTickCount()-changed[p]<150||!StarWanted(id[2],id[7]))continue;
  static const wchar_t* star=L"\u2605";

  const float phase=float((GetTickCount()-changed[p])%1200)/1200.0f;
  const float alpha=0.725f+0.275f*cosf(phase*6.2831853f);
  const float red[4]={0.95f,0.08f,0.12f,alpha};
  memcpy(copy+0x40,&star,4);memcpy(copy+0x5c,red,16);

  uint32_t lobby=0;const bool online=Read(At(0x13818a0),lobby)&&lobby==3;

  float sx=p?footerX-(online?88.25f:85.75f):(online?201.75f:203.75f),sy=online?321.75f:338.75f;

  const float starScale=0.88f;memcpy(copy+0x4c,&starScale,4);memcpy(copy+0x50,&starScale,4);memcpy(copy+0x54,&sx,4);memcpy(copy+0x58,&sy,4);
  nativeText(copy,context);
 }}
bool InstallNativeText(){
 auto address=reinterpret_cast<unsigned char*>(At(0xbc7e30));

 const unsigned char expected[]={0x55,0x8b,0xec,0x6a,0xff};
 unsigned char observed[5]{};SIZE_T got=0;
 if(!ReadProcessMemory(GetCurrentProcess(),address,observed,5,&got)||got!=5||memcmp(observed,expected,5))return false;
 auto gateway=static_cast<unsigned char*>(VirtualAlloc(nullptr,16,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));if(!gateway)return false;
 memcpy(gateway,expected,5);gateway[5]=0xe9;
 uint32_t jump=static_cast<uint32_t>(address+5-(gateway+10));memcpy(gateway+6,&jump,4);
 DWORD old=0;if(!VirtualProtect(gateway,16,PAGE_EXECUTE_READ,&old)){VirtualFree(gateway,0,MEM_RELEASE);return false;}
 FlushInstructionCache(GetCurrentProcess(),gateway,10);
 nativeText=reinterpret_cast<NativeTextFn>(gateway);
 if(!VirtualProtect(address,5,PAGE_EXECUTE_READWRITE,&old)){nativeText=nullptr;VirtualFree(gateway,0,MEM_RELEASE);return false;}
 unsigned char patch[5]={0xe9};jump=static_cast<uint32_t>(reinterpret_cast<unsigned char*>(&NativeText)-(address+5));memcpy(patch+1,&jump,4);
 memcpy(address,patch,5);DWORD ignored;VirtualProtect(address,5,old,&ignored);FlushInstructionCache(GetCurrentProcess(),address,5);
 NativeLog("Native text hook installed; scoped to costume selection and the confirm-button tag.");
 return true;
}
