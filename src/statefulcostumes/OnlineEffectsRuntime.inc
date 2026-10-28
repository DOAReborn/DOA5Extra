NativeEffectRngTls::Router costumeEffectRng;
NativeEffectRngTls::InstalledHooks costumeEffectRngHooks;
volatile LONG costumeEffectRngReady=0;
#ifdef PREFLIGHT_TOOL

bool costumeEffectRngTestFixture=false;
#endif
thread_local DiagnosticRateLimit costumeEffectAdmissionLog;
const BYTE costumeMtPrefix[]={
 0x55,0x8b,0xec,0x51,0x56,0x8b,0xf1,0xeb,0x02,0xdd,0xd8,
 0xff,0x8e,0xc4,0x09,0,0,0x75,0x07,0x8b,0xce,0xe8,0x16,0xc8,0xff,0xff,
 0x8b,0x8e,0xc0,0x09,0,0,0x8b,0x01,0x83,0xc1,0x04,0x89,0x8e,0xc0,0x09,0,0
};
const BYTE costumeMtSecondPrefix[]={
 0x55,0x8b,0xec,0x51,0x56,0x8b,0xf1,0xff,0x8e,0xc4,0x09,0,0,
 0x75,0x05,0xe8,0x0c,0xe8,0xff,0xff,
 0x8b,0x8e,0xc0,0x09,0,0,0x8b,0x01,0x83,0xc1,0x04,0x89,0x8e,0xc0,0x09,0,0
};

bool InstallCostumeEffectRngHooks() noexcept {
 if(InterlockedCompareExchange(&costumeEffectRngReady,0,0))return true;
 if(!image||!Signature(0x421080,costumeMtPrefix,sizeof costumeMtPrefix)
  ||!Signature(0x41f090,costumeMtSecondPrefix,sizeof costumeMtSecondPrefix))return false;
 if(!Accessible(At(0x1487ab0),NativeEffectRngTls::StateBytes,true)
  ||!Accessible(At(0x24906e0),NativeEffectRngTls::StateBytes,true)
  ||!costumeEffectRng.Configure(At(0x1487ab0),At(0x24906e0)))return false;
 const NativeEffectRngTls::HookTarget targets[]={
  {At(0x421080),costumeMtPrefix,sizeof costumeMtPrefix},
  {At(0x41f090),costumeMtSecondPrefix,sizeof costumeMtSecondPrefix}
 };
 const auto report=NativeEffectRngTls::InstallHooks(costumeEffectRng,targets,costumeEffectRngHooks);
 if(!report.Succeeded()){
  char line[256];std::snprintf(line,sizeof line,
   "Costume FX RNG hook transaction refused: status=%u error=%lu threads=%u suspended=%u resumeFailures=%u protectionFailures=%u published=%u rollbackFlushFailed=%u",
   unsigned(report.status),static_cast<unsigned long>(report.error),report.threads,report.suspended,
   report.resumeFailures,report.protectionFailures,unsigned(report.bytesPublished),unsigned(report.rollbackFlushFailed));Queue(line);
  return false;
 }
 InterlockedExchange(&costumeEffectRngReady,1);
 Queue("Costume FX RNG routing ready: private streams per thread; native RNG globals untouched.");
 return true;
}
bool ObserveCostumeEffectEngineThread() noexcept {

 if(!InterlockedCompareExchange(&costumeEffectRngReady,0,0)||!costumeEffectRng.Ready())return false;
#ifdef PREFLIGHT_TOOL
 if(costumeEffectRngTestFixture)return true;
#endif
 return NativeEffectRngTls::HooksIntact(costumeEffectRngHooks);
}
struct NativeCostumeEffectScope {
 std::optional<NativeEffectRngTls::Scope> rng;
 bool admitted=false;
 NativeCostumeEffectScope() noexcept {
  BYTE networkFlags=0;

  if(!image||!Read(At(0x1381820)+0xa64,networkFlags))return;
  if(!(networkFlags&8)){admitted=true;return;}
  const auto context=TrialContext();

  const bool rollbackFx=onlinecostume::RollbackOnlineAllowed(context);
  if(!(onlinecostume::NativeOnlineAllowed(context)||rollbackFx)||!ObserveCostumeEffectEngineThread())return;
  rng.emplace(costumeEffectRng,NativeEffectRngTls::Admission{
   true,context.rollbackQueryKnown,context.rollbackActive&&!rollbackFx,true});
  admitted=bool(*rng);
  if(!admitted&&costumeEffectAdmissionLog.Permit(GetTickCount64())){
   char line[128];std::snprintf(line,sizeof line,"Costume FX RNG admission refused: status=%u",unsigned(rng->Result()));Queue(line);
  }
 }
 ~NativeCostumeEffectScope() noexcept {
  if(rng&&admitted&&!rng->Finish()){
   char line[128];std::snprintf(line,sizeof line,"Costume FX RNG scope exit refused: status=%u",unsigned(rng->Result()));Queue(line);
  }
 }
 explicit operator bool() const noexcept {return admitted;}
 NativeCostumeEffectScope(const NativeCostumeEffectScope&)=delete;
 NativeCostumeEffectScope& operator=(const NativeCostumeEffectScope&)=delete;
};
