bool onlineTrialEnabled=false;
bool rollbackConfiguredDisabled=false;

bool rollbackCostumesEnabled=false;
volatile LONG rollbackAdmitted=0;
onlinecostume::FrameClock onlineFrameClock;
SRWLOCK onlineClockLock=SRWLOCK_INIT;
uint32_t onlineMilliseconds=1000;
BYTE* onlineClockManager=nullptr;
int lastOnlineAdmission=-1;

constexpr ULONGLONG onlineStallGraceMs=100;
ULONGLONG onlineLastAdvanceTick=0,onlineStallCountedTick=0;
uint32_t onlineStallOffset=0;
volatile LONG onlineResetPending=0;
bool networkModeObserved=false,previousNetworkMode=false;
struct RollbackNetInfo {int size=24,active=0,ping=0,delay=0,mode=0,limit=0;};
static_assert(sizeof(RollbackNetInfo)==24,"Public lab116 NetInfo ABI");
#ifdef PREFLIGHT_TOOL
bool trialMockRollback=false,trialMockKnown=true,trialMockActive=false;
#endif
bool ReadRollbackActive(bool& active) noexcept {
#ifdef PREFLIGHT_TOOL
 if(trialMockRollback){active=trialMockActive;return trialMockKnown;}
#endif
 active=false;
 HMODULE module=GetModuleHandleW(L"DOA5Rollback.asi");
 if(!module)return true;
 using Query=int(__cdecl*)(RollbackNetInfo*);
 auto query=reinterpret_cast<Query>(GetProcAddress(module,"DOA5RB_GetNetInfo"));
 if(!query)return false;
 RollbackNetInfo info;
 if(query(&info)!=1||(info.active!=0&&info.active!=1))return false;
 active=info.active==1;return true;
}
bool ReadOnlineFrame(uint32_t& frame,BYTE** managerOut=nullptr) noexcept {
 frame=0;
 if(managerOut)*managerOut=nullptr;
 if(!image)return false;
 BYTE* manager=nullptr;BYTE* after=nullptr;
 uint32_t modeBefore=0,modeAfter=0,value=0;
 if(!Read(At(0x13ce790),manager))return false;
 const auto address=reinterpret_cast<uintptr_t>(manager);
 if(address<0x10000u||address>std::numeric_limits<uintptr_t>::max()-0x20u||(address&3u))return false;

 if(!Read(manager+8,modeBefore)||!Read(manager+0x1c,value)
  ||!Read(manager+8,modeAfter)||!Read(At(0x13ce790),after)
  ||manager!=after||modeBefore!=modeAfter)return false;
 frame=value;if(managerOut)*managerOut=manager;return true;
}
onlinecostume::Context TrialContext(bool sampleClock=true) noexcept {
 onlinecostume::Context c{};c.optIn=onlineTrialEnabled;c.rollbackOptIn=rollbackCostumesEnabled;
 if(!image)return c;
 c.readable=Read(At(0x1382884),c.context)&&Read(At(0x1381820)+0xa64,c.networkFlags);
 if(!c.readable||!(c.networkFlags&8))return c;
 c.rollbackConfiguredDisabled=rollbackConfiguredDisabled;
 c.rollbackQueryKnown=ReadRollbackActive(c.rollbackActive);
 if(!Read(At(0x13818a0),c.lobbyType))return c;

 uint32_t frame=0;
 BYTE* manager=nullptr;
 AcquireSRWLockExclusive(&onlineClockLock);
 c.frameReadable=ReadOnlineFrame(frame,&manager);
 if(!c.frameReadable||!sampleClock){ReleaseSRWLockExclusive(&onlineClockLock);return c;}

 if(onlineClockManager&&onlineClockManager!=manager)onlineFrameClock.Invalidate();
 onlineClockManager=manager;

 static bool rollbackLoaded=false;if(!rollbackLoaded)rollbackLoaded=GetModuleHandleW(L"DOA5Rollback.asi")!=nullptr;
 const auto sample=onlineFrameClock.Observe(frame,c.rollbackOptIn&&(c.rollbackActive||rollbackLoaded));
 c.frameStable=sample.valid;
 if(sample.valid){
  const ULONGLONG nowTick=GetTickCount64();
  if(sample.advanced||!onlineLastAdvanceTick){onlineLastAdvanceTick=nowTick;onlineStallCountedTick=0;}
  else if(nowTick-onlineLastAdvanceTick>onlineStallGraceMs){
   const ULONGLONG from=onlineStallCountedTick?onlineStallCountedTick:onlineLastAdvanceTick+onlineStallGraceMs;
   if(nowTick>from)onlineStallOffset+=uint32_t(nowTick-from>1000?1000:nowTick-from);
   onlineStallCountedTick=nowTick;
  }
  onlineMilliseconds=sample.milliseconds+onlineStallOffset;
 }
 ReleaseSRWLockExclusive(&onlineClockLock);
 return c;
}
bool OnlineStateAllowed() noexcept {
 const auto c=TrialContext();
 const bool allowed=onlinecostume::Allowed(c);
 InterlockedExchange(&rollbackAdmitted,(allowed&&c.readable&&(c.networkFlags&8)&&c.rollbackActive)?1:0);
 if(c.readable&&(c.networkFlags&8)){
  const int admission=allowed?1:0;
  if(admission!=lastOnlineAdmission){
   lastOnlineAdmission=admission;
   char line[320];std::snprintf(line,sizeof line,
    "ONLINE TRIAL %s %s: opt=%u lobby=%u clockReadable=%u stable=%u rollbackKnown=%u active=%u disabled=%u rollbackCostumes=%u; peer state unverified",
    (c.rollbackActive&&c.rollbackOptIn)?"rollback":"native-only",
    allowed?"ADMITTED":"REFUSED",unsigned(c.optIn),c.lobbyType,unsigned(c.frameReadable),unsigned(c.frameStable),
    unsigned(c.rollbackQueryKnown),unsigned(c.rollbackActive),unsigned(c.rollbackConfiguredDisabled),unsigned(c.rollbackOptIn));Queue(line);
  }
 }else lastOnlineAdmission=-1;
 return allowed;
}
bool IsNetworkContext() noexcept {
 BYTE flags=0;return image&&Read(At(0x1381820)+0xa64,flags)&&(flags&8);
}
ULONGLONG CostumeTime() noexcept {

 if(!IsNetworkContext())return GetTickCount64();
 AcquireSRWLockShared(&onlineClockLock);
 const ULONGLONG now=onlineMilliseconds;
 ReleaseSRWLockShared(&onlineClockLock);return now;
}
void ResetOnlineClock() noexcept {
 AcquireSRWLockExclusive(&onlineClockLock);
 onlineFrameClock.Reset();onlineMilliseconds=1000;onlineClockManager=nullptr;lastOnlineAdmission=-1;
 onlineLastAdvanceTick=onlineStallCountedTick=0;onlineStallOffset=0;
 ReleaseSRWLockExclusive(&onlineClockLock);
}
void ResetOnlineTracking() noexcept;
void ObserveTimerDomain() noexcept {
 BYTE flags=0;if(!Read(At(0x1381820)+0xa64,flags))return;
 const bool network=(flags&8)!=0;
 const bool changed=networkModeObserved?network!=previousNetworkMode:network;
 networkModeObserved=true;previousNetworkMode=network;

 const LONG pendingEpoch=InterlockedCompareExchange(&onlineResetPending,0,0);
 const bool pending=network&&pendingEpoch&&pendingEpoch<=observedResetEpoch
  &&InterlockedCompareExchange(&onlineResetPending,0,pendingEpoch)==pendingEpoch;
 if(changed||pending)ResetOnlineTracking();
}
void QueueCostumeState(const char* message) noexcept {
 if(!IsNetworkContext()){Queue(message);return;}
 uint32_t frame=0;
 if(!ReadOnlineFrame(frame)){Queue(message);return;}
 char line[256];std::snprintf(line,sizeof line,"ONLINE inputFrame=%u %s",frame,message);Queue(line);
}
