static unsigned onlineParticleNativeCalls=0;
static void __fastcall OnlineParticleUpdateSpy(void*,void*,void*) noexcept {++onlineParticleNativeCalls;IntegratedMutateRng();}
static void __fastcall OnlineParticleDrawSpy(void*,void*,void*) noexcept {++onlineParticleNativeCalls;IntegratedMutateRng();}
static void __fastcall OnlineParticleStateSpy(void*) noexcept {++onlineParticleNativeCalls;IntegratedMutateRng();}
static void __cdecl OnlineParticleBoneSpy(float* out,void*,int) noexcept {
 ++onlineParticleNativeCalls;IntegratedMutateRng();for(unsigned i=0;i<4;++i)out[i]=0;
}
struct OnlineWorkerCallback {
 void(__fastcall* callback)(BYTE*,void*,void*) noexcept;
 BYTE* object;
 void* delta;
};
static DWORD WINAPI OnlineWorkerCallbackEntry(void* argument) {
 const auto& work=*static_cast<OnlineWorkerCallback*>(argument);
 work.callback(work.object,nullptr,work.delta);
 return 0;
}
bool OnlineTrialSelfTest(const char* root) noexcept {
 if(image||prepared||rendererArmed||controlActive||!root)return false;
 if(!IntegratedArena())return false;
 IntegratedActor a,b;
 if(!a.Load(root,"KOKORO_DLCU_032",0,1)||!b.Load(root,"KOKORO_DLCU_025",1,1)){
  puts("FAIL online trial: converted TMC fixtures missing");return false;
 }
 IntegratedBind(a,b);controlActive=1;
 StateCostumes::SetTransitionEnabled(false,true);StateCostumes::SetTransitionEnabled(true,true);
 StateCostumes::OnBeforeRender(a.entity);StateCostumes::OnBeforeRender(b.entity);
 nativeBurstEnabled=nativeSoundEnabled=true;
 trialMockRollback=true;trialMockKnown=true;trialMockActive=false;
 alignas(4) BYTE manager[64]{};Put32(At(0x13ce790),uint32_t(uintptr_t(manager)));
 Put32(manager+8,2);
 IntegratedInputFrame(manager,100);Put32(At(0x13818a0),3);
 Put32(At(0x137a240),777);
 EntityInfo ea{},eb{};if(!Entity(a.entity,ea,true)||!Entity(b.entity,eb,true))return false;
 unsigned checks=0,failed=0;
 auto check=[&](bool b,const char* why){++checks;if(!b){++failed;printf("FAIL online trial: %s\n",why);}};

 {
  const auto rngBefore=IntegratedSnapshotRng();BYTE* savedImage=image;bool admitted=false;
  image=nullptr;
  {NativeCostumeEffectScope unavailable;admitted=bool(unavailable);}
  image=savedImage;
  check(!admitted,"unreadable network context refuses an effect scope");
  check(IntegratedRngEquals(rngBefore),"unreadable network context leaves both RNG blocks untouched");
 }

 for(unsigned network=0;network<2;++network)
 for(unsigned opt=0;opt<2;++opt)
 for(unsigned disabled=0;disabled<2;++disabled)
 for(unsigned active=0;active<2;++active)
 for(unsigned changed=0;changed<2;++changed){
  Put32(At(0x1382884),network?0:1);*At(0x1381820+0xa64)=network?8:0;
  onlineTrialEnabled=opt;rollbackConfiguredDisabled=disabled;trialMockActive=active;
  ResetOnlineClock();offlineEventSinks={};offlineEventSinks.enabled=true;
  offlineEventSinks.beforeSubmit=&IntegratedMutateRng;
  const auto rngBefore=IntegratedSnapshotRng();const unsigned mutations=integratedRngMutationCalls;
  const unsigned calls=integratedNative.activeCalls;
  TransitionBurst(ea,changed,true);TransitionSound(ea,changed);TransitionBurst(eb,changed,false);
  unsigned expected=changed&&(!network||(opt&&!active));
  check(offlineEventSinks.schoolgirl[0]==expected,"Schoolgirl effect admission");
  check(offlineEventSinks.sounds[0]==expected,"transformation sound admission");
  check(offlineEventSinks.destruction[1]==expected,"destruction effect admission");
  check(integratedRngMutationCalls==mutations+3*expected,"only admitted submissions run synthetic native FX work");
  if(network||!expected)check(IntegratedRngEquals(rngBefore),"online/refused submissions preserve both native RNG blocks");
  else check(!IntegratedRngEquals(rngBefore),"offline submissions retain native RNG progress");
  if(network&&!expected)check(integratedNative.activeCalls==calls,"refused online effects return before native actor calls");
 }

 {
  struct Entry {uint32_t va;const void* target;BYTE saved[16];DWORD protection;};
  Entry entries[]={
   {0x57edd0,reinterpret_cast<const void*>(&OnlineParticleUpdateSpy),{},0},
   {0x4264e0,reinterpret_cast<const void*>(&OnlineParticleDrawSpy),{},0},
   {0xbb97f0,reinterpret_cast<const void*>(&OnlineParticleStateSpy),{},0},
   {0x8a3e60,reinterpret_cast<const void*>(&OnlineParticleBoneSpy),{},0}};
  for(auto& entry:entries){
   std::memcpy(entry.saved,At(entry.va),sizeof entry.saved);
   if(!VirtualProtect(At(entry.va),sizeof entry.saved,PAGE_EXECUTE_READWRITE,&entry.protection))return false;
   IntegratedJump(entry.va,entry.target);
  }
  FlushInstructionCache(GetCurrentProcess(),image,0x2100000);
  using Callback=void(__fastcall*)(BYTE*,void*,void*) noexcept;
  Callback callbacks[]={&SchoolgirlUpdate,&SchoolgirlDraw,&DestructionUpdate,&DestructionDraw};
  alignas(16) BYTE renderState[124]{};float delta[3]={1,1,1};
  uint32_t previousState=U32(At(0x12c8f98));
  Put32(At(0x12c8f98),uint32_t(uintptr_t(renderState)));
  for(unsigned admitted=0;admitted<2;++admitted){
   *At(0x1381820+0xa64)=8;Put32(At(0x1382884),0);
   onlineTrialEnabled=admitted!=0;trialMockActive=false;ResetOnlineClock();
   check(OnlineStateAllowed()==(admitted!=0),"surviving particle test covers admitted and refused online");
   for(unsigned workerThread=0;workerThread<2;++workerThread)
   for(auto callback:callbacks){
    alignas(16) BYTE object[ParticleResetLifetime::ObjectSize]{};
    Put32(object+4,1);Put32(object+0xc,uint32_t(uintptr_t(a.entity)));
    const float remaining=2;std::memcpy(object+0xc0,&remaining,sizeof remaining);
    ParticleResetLifetime::Stamp(object,uint32_t(InterlockedCompareExchange(&resetEpoch,0,0)));
    BYTE expected[sizeof object];std::memcpy(expected,object,sizeof object);Put32(expected+4,0);
    const unsigned activeCalls=integratedNative.activeCalls;
    const auto rngBefore=IntegratedSnapshotRng();const unsigned mutations=integratedRngMutationCalls;
    BYTE stateBefore[sizeof renderState];std::memcpy(stateBefore,renderState,sizeof renderState);
    onlineParticleNativeCalls=0;
    if(workerThread){

     OnlineWorkerCallback work{callback,object,delta};
     HANDLE worker=CreateThread(nullptr,0,OnlineWorkerCallbackEntry,&work,0,nullptr);
     check(worker!=nullptr,"particle worker starts");
     if(!worker)return false;
     if(WaitForSingleObject(worker,5000)!=WAIT_OBJECT_0){
      puts("FAIL online trial: worker callback did not complete");
      std::abort();
     }
     CloseHandle(worker);
    }else callback(object,nullptr,delta);
    if(admitted){
     check(U32(object+4)==1,"admitted online callback keeps the current-epoch particle root alive");
     check(onlineParticleNativeCalls==2&&integratedRngMutationCalls==mutations+2,"admitted callback executes its native spies");
    }else{
     check(U32(object+4)==0,"refused online callback expires a current-epoch particle root");
     check(!std::memcmp(object,expected,sizeof object),"refused callback changes only root active flag");
     check(!onlineParticleNativeCalls&&integratedNative.activeCalls==activeCalls,"refused callback invokes no native actor/particle work");
    }
    check(IntegratedRngEquals(rngBefore),"callback preserves combat and visual RNG byte for byte");
    check(U32(At(0x12c8f98))==uint32_t(uintptr_t(renderState)),"online callback leaves native render state pointer intact");
    check(!std::memcmp(renderState,stateBefore,sizeof renderState),"online callback leaves inherited native render state bytes intact");
   }
  }
  Put32(At(0x12c8f98),previousState);
  for(auto& entry:entries){DWORD ignored=0;std::memcpy(At(entry.va),entry.saved,sizeof entry.saved);
   if(!VirtualProtect(At(entry.va),sizeof entry.saved,entry.protection,&ignored))return false;
  }
  FlushInstructionCache(GetCurrentProcess(),image,0x2100000);
 }
 onlineTrialEnabled=true;rollbackConfiguredDisabled=false;trialMockActive=false;
 Put32(At(0x1382884),0);*At(0x1381820+0xa64)=8;ResetOnlineTracking();
 check(OnlineStateAllowed(),"native online admitted with configured module known inactive");
 {
  const auto stallStart=CostumeTime();Sleep(350);OnlineStateAllowed();Sleep(50);
  check(OnlineStateAllowed()&&CostumeTime()>=stallStart+200,"stalled native clock bridged by local time");
  ResetOnlineTracking();check(OnlineStateAllowed(),"re-admitted after stall test");
 }
 const auto start=CostumeTime();
 for(unsigned i=0;i<100;++i)check(OnlineStateAllowed()&&CostumeTime()==start,"duplicate rendering does not advance timer");

 StateCostumes::ResetForRound(false);StateCostumes::OnBeforeRender(a.entity);
 check(QueueD1Flip(ea,42,1,start,false),"queue verified delayed transition");
 IntegratedInputFrame(manager,109);D1Tick();
 check(!StateCostumes::IsTransformed(a.entity),"9 frames preserves initial form");
 IntegratedInputFrame(manager,110);D1Tick();
 check(StateCostumes::IsTransformed(a.entity),"10 frames applies delayed form");
 D1Tick();check(StateCostumes::IsTransformed(a.entity),"duplicate frame cannot toggle back");
 check(U32(At(0x112ebc4))==1&&*At(0x14885be)==0,"reseed counter stays frozen while real input ticks execute the transition");
 check(U32(At(0x137a240))==777,"unrelated netcode counter is not used as a deadline clock");

 for(unsigned offset=0;offset<6;++offset){
  StateCostumes::ResetForRound(false);ResetOnlineTracking();
  IntegratedInputFrame(manager,200);check(OnlineStateAllowed(),"phase test clock origin");
  IntegratedInputFrame(manager,200+offset);check(OnlineStateAllowed(),"phase test timer start");
  check(QueueD1Flip(ea,42,1,CostumeTime(),false),"phase test queued flip");
  IntegratedInputFrame(manager,209+offset);D1Tick();
  check(!StateCostumes::IsTransformed(a.entity),"all phases wait nine frames");
  IntegratedInputFrame(manager,210+offset);D1Tick();
  check(StateCostumes::IsTransformed(a.entity),"all phases toggle on tenth frame");
 }

 StateCostumes::ResetForRound(false);offlineEventSinks={};offlineEventSinks.enabled=true;
 PS4Break(1,"selftest",1,0);PS4Break(1,"selftest repeat",1,0);
 check(StateCostumes::IsTransformed(b.entity)&&offlineEventSinks.destruction[1]==1,"one logical break with exactly one admitted engine effect");
 StateCostumes::ResetForRound(false);trialMockActive=true;PS4Break(1,"active rollback",1,0);
 check(!StateCostumes::IsTransformed(b.entity),"active rollback refuses state mutation");
 trialMockActive=false;trialMockKnown=false;check(!OnlineStateAllowed(),"unknown rollback API refused");
 trialMockKnown=true;Put32(At(0x13818a0),1);check(!OnlineStateAllowed(),"non-lobby mode refused");
 Put32(At(0x13818a0),3);IntegratedInputFrame(manager,109);check(!OnlineStateAllowed(),"backwards input frame refused");
 IntegratedInputFrame(manager,300);check(!OnlineStateAllowed(),"clock discontinuity stays refused");
 ResetOnlineTracking();check(OnlineStateAllowed(),"explicit native reset permits new clock");
 check(!pendingFlips[0]&&!pendingFlips[1]&&!pbWas[0]&&!pbWas[1],"reset clears pending transitions");

 *At(0x13d0604)=1;Put32(At(0x24722ac),1);
 unsigned surfaces=integratedNative.surfaceCalls;
 PS4CheckSides();check(integratedNative.surfaceCalls==surfaces,"admitted online skips extra stage query");
 trialMockActive=true;
 const unsigned actors=integratedNative.activeCalls;
 PS4CheckSides();ScriptBreak(a.entity);
 check(integratedNative.surfaceCalls==surfaces&&integratedNative.activeCalls==actors,"refused danger/script entries call no native predicates");
 trialMockActive=false;*At(0x13d0604)=0;

 originalRender=&IntegratedRender;originalReset=&IntegratedReset;
 StateCostumes::ResetForRound(false);ResetOnlineTracking();
 networkModeObserved=true;previousNetworkMode=true;onlineResetPending=0;
 observedResetEpoch=resetEpoch;
 IntegratedInputFrame(manager,500);check(OnlineStateAllowed(),"render test clock origin");
 scriptRun[0].number=675;scriptRun[0].maxOff=30;scriptRun[0].base=1;scriptRun[0].serial=1;
 const auto flagsBefore=a.flags;const auto otherFlagsBefore=b.flags;
 std::vector<BYTE> playersBefore(At(0x13d0540),At(0x13d0540)+2*0x6c8);
 std::vector<BYTE> positionsBefore(At(0x13cbaf0),At(0x13cbaf0)+2*0x180);
 const auto rngBefore=IntegratedSnapshotRng();
 auto expectedDraw=a.baseline;
 for(unsigned i=0;i<a.model->opCount;++i){const auto& op=StateCostumes::Data::kOps[a.model->opStart+i];
  expectedDraw[op.node]=op.transformed==2?a.baseline[op.node]:op.transformed;
 }
 offlineEventSinks={};offlineEventSinks.enabled=true;
 offlineEventSinks.beforeSubmit=&IntegratedMutateRng;
 const unsigned mutationsBeforeRender=integratedRngMutationCalls;
 const unsigned renders=integratedNative.renders;
 RenderBridge(a.entity,nullptr,0);
 check(!StateCostumes::IsTransformed(a.entity)&&pendingFlips[0]==1,"observed D1 starts effects and queues the delayed visual flip");
 check(offlineEventSinks.sounds[0]==1&&offlineEventSinks.schoolgirl[0]==1,"observed D1 emits one sound and one Schoolgirl effect immediately");
 check(a.flags==flagsBefore&&b.flags==otherFlagsBefore&&IntegratedRngEquals(rngBefore),"D1 emission restores visibility and both native RNG blocks");
 IntegratedInputFrame(manager,510);RenderBridge(a.entity,nullptr,0);
 check(StateCostumes::IsTransformed(a.entity),"render executes queued online logical transition");
 check(integratedNative.renders==renders+2&&integratedNative.drawFlags[0]==expectedDraw,"second native draw sees the delayed transformed costume");
 check(a.flags==flagsBefore&&b.flags==otherFlagsBefore,"render restores exact native visibility of both actors");
 check(!std::memcmp(playersBefore.data(),At(0x13d0540),playersBefore.size())
   &&!std::memcmp(positionsBefore.data(),At(0x13cbaf0),positionsBefore.size())
   &&IntegratedRngEquals(rngBefore),"render preserves player/position/combat and visual RNG bytes");
 check(offlineEventSinks.sounds[0]==1&&!offlineEventSinks.sounds[1]
   &&offlineEventSinks.schoolgirl[0]==1&&!offlineEventSinks.schoolgirl[1]
   &&!offlineEventSinks.destruction[0]&&!offlineEventSinks.destruction[1]
   &&integratedRngMutationCalls==mutationsBeforeRender+2,"online render creates one isolated sound and Schoolgirl effect");

 StateCostumes::ResetForRound(false);ResetOnlineTracking();
 Put32(At(0x11d94d4),2);
 IntegratedInputFrame(manager,600);check(OnlineStateAllowed(),"post-KO input clock origin");
 const uint16_t healthy=300,dead=0;
 std::memcpy(At(0x13d056c),&healthy,2);std::memcpy(At(0x13d056c+0x6c8),&dead,2);
 hpWas[0]=healthy;hpWas[1]=0;
 scriptRun[0].number=675;scriptRun[0].maxOff=30;scriptRun[0].base=1;scriptRun[0].serial=1;
 offlineEventSinks={};offlineEventSinks.enabled=true;offlineEventSinks.beforeSubmit=&IntegratedMutateRng;
 const auto postKoRng=IntegratedSnapshotRng();
 RenderBridge(a.entity,nullptr,0);
 check(pendingFlips[0]==1&&!StateCostumes::IsTransformed(a.entity),"post-KO D1 starts a fresh delayed transition");
 for(unsigned frame=1;frame<=9;++frame){
  IntegratedInputFrame(manager,600+frame);RenderBridge(a.entity,nullptr,0);
  check(!StateCostumes::IsTransformed(a.entity),"post-KO form waits all nine input ticks");
 }
 IntegratedInputFrame(manager,610);RenderBridge(a.entity,nullptr,0);
 check(StateCostumes::IsTransformed(a.entity)&&!pendingFlips[0],"post-KO form changes at the tenth advancing input tick");
 for(unsigned repeat=0;repeat<3;++repeat)RenderBridge(a.entity,nullptr,0);
 check(StateCostumes::IsTransformed(a.entity)&&offlineEventSinks.sounds[0]==1&&offlineEventSinks.schoolgirl[0]==1,
  "duplicate post-KO renders neither toggle again nor repeat effects");
 check(IntegratedRngEquals(postKoRng),"post-KO local effects preserve native RNG streams");
 check(U32(At(0x112ebc4))==1&&*At(0x14885be)==0,"post-KO test never advances the old reseed clock");

 StateCostumes::ResetForRound(false);ResetOnlineTracking();
 Put32(manager+8,2);IntegratedInputFrame(manager,650);
 check(OnlineStateAllowed(),"recorder-stop fixture has a readable combat origin");
 const auto stoppedTime=CostumeTime();
 check(QueueD1Flip(ea,42,1,stoppedTime,false),"stopped-clock fixture queues a delayed flip");
 Put32(manager+8,0);
 for(unsigned repeat=0;repeat<20;++repeat)D1Tick();
 check(pendingFlips[0]==1&&!StateCostumes::IsTransformed(a.entity),"stopped input clock leaves delayed flip pending");
 check(CostumeTime()==stoppedTime&&U32(At(0x112ebc4))==1,
  "repeated observations cannot substitute wall time or the reseed counter");
 Put32(manager+8,2);

 StateCostumes::ResetForRound(false);ResetOnlineTracking();
 std::memcpy(At(0x13d056c+0x6c8),&healthy,2);hpWas[0]=hpWas[1]=healthy;
 IntegratedInputFrame(manager,700);check(OnlineStateAllowed(),"rematch fixture old clock origin");
 check(QueueD1Flip(ea,42,1,CostumeTime(),false),"old-match pending transition queued");
 IntegratedInputFrame(manager,705);D1Tick();
 check(pendingFlips[0]==1&&!StateCostumes::IsTransformed(a.entity),"old transition remains pending before rematch");
 IntegratedReset();IntegratedInputFrame(manager,0);
 const LONG rematchEpoch=InterlockedIncrement(&resetEpoch);
 InterlockedExchange(&resetFull,1);InterlockedExchange(&onlineResetPending,rematchEpoch);
 check(!OnlineStateAllowed(),"backwards rematch frame is refused before its reset is acknowledged");
 RenderBridge(a.entity,nullptr,0);
 check(observedResetEpoch==rematchEpoch&&!onlineResetPending&&!pendingFlips[0],"render acknowledges rematch and clears old clock deadlines");
 check(OnlineStateAllowed()&&!StateCostumes::IsTransformed(a.entity),"acknowledged rematch admits new initial form at input frame zero");
 check(QueueD1Flip(ea,42,2,CostumeTime(),false),"new-match transition starts from new clock origin");
 IntegratedInputFrame(manager,9);D1Tick();check(!StateCostumes::IsTransformed(a.entity),"new-match form still waits nine frames");
 IntegratedInputFrame(manager,10);D1Tick();check(StateCostumes::IsTransformed(a.entity),"new-match form switches on its own tenth frame");

 alignas(4) BYTE replacementManager[64]{};Put32(replacementManager+0x1c,10);
 Put32(At(0x13ce790),uint32_t(uintptr_t(replacementManager)));
 check(!OnlineStateAllowed(),"same numeric frame in a replacement manager is refused");
 Put32(At(0x13ce790),uint32_t(uintptr_t(manager)));
 check(!OnlineStateAllowed(),"replacing the old pointer does not silently heal clock discontinuity");
 ResetOnlineTracking();check(OnlineStateAllowed(),"explicit reset permits a validated manager again");
 const auto readableTime=CostumeTime();Put32(At(0x13ce790),0);
 check(!OnlineStateAllowed()&&CostumeTime()==readableTime,"missing manager refuses admission without advancing deadlines");
 Put32(At(0x13ce790),uint32_t(uintptr_t(manager)));
 check(OnlineStateAllowed(),"temporary unreadability can resume the same validated manager");

 networkModeObserved=false;previousNetworkMode=false;onlineResetPending=1;observedResetEpoch=1;
 *At(0x1381820+0xa64)=0;Put32(At(0x1382884),1);
 pendingFlips[0]=1;pendingAt[0]=9999999;ObserveTimerDomain();
 check(onlineResetPending==1&&pendingFlips[0]==1,"offline observation retains online reset");
 *At(0x1381820+0xa64)=8;Put32(At(0x1382884),0);ObserveTimerDomain();
 check(!onlineResetPending&&!pendingFlips[0],"online entry consumes pending reset and clears wall timer");
 pendingFlips[0]=1;pendingAt[0]=1000;
 *At(0x1381820+0xa64)=0;Put32(At(0x1382884),1);ObserveTimerDomain();
 check(!pendingFlips[0],"offline return clears native-frame deadline");
 *At(0x1381820+0xa64)=8;ObserveTimerDomain();
 onlineResetPending=2;observedResetEpoch=1;pendingFlips[0]=1;
 ObserveTimerDomain();check(onlineResetPending==2&&pendingFlips[0]==1,"unacknowledged reset is not consumed");
 observedResetEpoch=2;ObserveTimerDomain();
 check(!onlineResetPending&&!pendingFlips[0],"acknowledged reset clears tracking");
 StateCostumes::ResetAll();controlActive=0;
 printf("ONLINE TRIAL: %u checks, %u failures; synthetic x86 arena; real TMC fixtures; no live game or peer test\n",checks,failed);
 return failed==0;
}
