namespace rb {
enum Kind : uint32_t {KD1=1,KCliff=2,KPB=3,KBreak=4};
struct Event {
 uint32_t kind{},side{},number{},index{},frame{},gen{};void* entity{};
 bool live{},applied{},pending{},pinned{},flip{};
};
constexpr unsigned kEvents=64;
constexpr uint32_t kMaxRewind=20;
Event events[kEvents]{};unsigned nextSlot=0;uint32_t nextGen=1;
SRWLOCK lock=SRWLOCK_INIT;
volatile LONG logicLast=0;bool logicInit=false;
volatile LONG rewinds=0,dedups=0,cancels=0;
volatile DWORD renderThread=0;
LONG wasActive=0;
struct Handle {int slot=-1;uint32_t gen=0;};

bool laboTest=false;volatile LONG laboOn=0;
bool Active() noexcept {return InterlockedCompareExchange(&rollbackAdmitted,0,0)!=0||InterlockedCompareExchange(&laboOn,0,0)!=0;}
bool LogicThread() noexcept {const DWORD r=renderThread;return r&&GetCurrentThreadId()!=r;}
uint32_t Frame() noexcept {uint32_t f=0;return ReadOnlineFrame(f)?f:0;}
void ClearLocked() noexcept {for(auto& e:events)e={};nextSlot=0;}
void Clear() noexcept {AcquireSRWLockExclusive(&lock);ClearLocked();ReleaseSRWLockExclusive(&lock);}

void LogicObserve() noexcept {
 if(!Active()){logicInit=false;return;}
 const uint32_t f=Frame();if(!f)return;
 const uint32_t last=uint32_t(InterlockedCompareExchange(&logicLast,0,0));
 if(!logicInit){logicInit=true;InterlockedExchange(&logicLast,LONG(f));return;}

 if(f<last&&last-f>kMaxRewind){
  AcquireSRWLockExclusive(&lock);ClearLocked();ReleaseSRWLockExclusive(&lock);
  char l[128];std::snprintf(l,sizeof l,"ROLLBACK compteur %u -> %u : nouvelle chronologie (round/match), suivi remis a zero.",last,f);Queue(l);
 }
 else if(f<last){
  const LONG n=InterlockedIncrement(&rewinds);unsigned marked=0;
  AcquireSRWLockExclusive(&lock);
  for(auto& e:events)if(e.live&&e.frame>f&&!e.pinned){e.pending=true;++marked;}
  ReleaseSRWLockExclusive(&lock);

  for(unsigned s=0;s<2;++s){ps4LastC[s]=0;ps4LastA[s]=0;}
  if(n<=20||marked){char l[160];std::snprintf(l,sizeof l,"ROLLBACK retour %u -> %u (n%ld) : %u evenement(s) de costume a reconfirmer.",last,f,long(n),marked);Queue(l);}
 }
 InterlockedExchange(&logicLast,LONG(f));
}

void ScriptStarted(unsigned side,uint32_t number,uint32_t f) noexcept {
 if(!Active()||!f)return;
 AcquireSRWLockExclusive(&lock);
 for(auto& e:events)if(e.live&&e.pending&&e.kind!=KBreak&&e.side==side&&e.number==number&&e.frame==f)e.pending=false;
 ReleaseSRWLockExclusive(&lock);
}

bool Claim(Kind kind,unsigned side,uint32_t number,uint32_t index,uint32_t startFrame,void* entity,Handle& h) noexcept {
 h={};
 if(!Active()||!startFrame)return true;
 AcquireSRWLockExclusive(&lock);
 for(unsigned i=0;i<kEvents;++i){Event& e=events[i];
  if(e.live&&e.kind==kind&&e.side==side&&e.number==number&&e.index==index&&e.frame==startFrame&&e.entity==entity){
   e.pending=false;ReleaseSRWLockExclusive(&lock);
   const LONG n=InterlockedIncrement(&dedups);
   if(n<=40){char l[160];std::snprintf(l,sizeof l,"ROLLBACK doublon ignore : genre %u cote %u script %u commande %u image de debut %u (recalcul).",unsigned(kind),side,number,index,startFrame);Queue(l);}
   return false;
  }}
 const unsigned i=nextSlot;nextSlot=(nextSlot+1)%kEvents;
 events[i]={kind,side,number,index,startFrame,nextGen++,entity,true,false,false,false,kind!=KBreak};
 h={int(i),events[i].gen};
 ReleaseSRWLockExclusive(&lock);
 return true;
}
bool Live(const Handle& h) noexcept {
 if(h.slot<0)return true;
 AcquireSRWLockExclusive(&lock);const Event& e=events[h.slot];const bool ok=e.gen==h.gen&&e.live;ReleaseSRWLockExclusive(&lock);
 return ok;
}
void MarkApplied(const Handle& h) noexcept {
 if(h.slot<0)return;
 AcquireSRWLockExclusive(&lock);Event& e=events[h.slot];if(e.gen==h.gen&&e.live)e.applied=true;ReleaseSRWLockExclusive(&lock);
}

bool BreakSeen(unsigned side,uint32_t f) noexcept {
 if(!Active()||!f)return false;
 bool seen=false;
 AcquireSRWLockExclusive(&lock);
 for(auto& e:events)if(e.live&&e.kind==KBreak&&e.side==side&&e.frame+1>=f&&e.frame<=f+1){e.pending=false;seen=true;}
 ReleaseSRWLockExclusive(&lock);
 return seen;
}
void RecordBreak(unsigned side,uint32_t f,void* entity) noexcept {
 if(!Active()||!f)return;
 AcquireSRWLockExclusive(&lock);
 const unsigned i=nextSlot;nextSlot=(nextSlot+1)%kEvents;
 events[i]={KBreak,side,0,0,f,nextGen++,entity,true,true,false,false,false};
 ReleaseSRWLockExclusive(&lock);
}

void PinBreaks(void* entity) noexcept {
 if(!Active())return;
 AcquireSRWLockExclusive(&lock);
 for(auto& e:events)if(e.live&&e.kind==KBreak&&e.entity==entity){e.pinned=true;e.pending=false;}
 ReleaseSRWLockExclusive(&lock);
}

void RenderTick() noexcept {
 renderThread=GetCurrentThreadId();
 if(laboTest)InterlockedExchange(&laboOn,(!IsNetworkContext()&&GetModuleHandleW(L"DOA5Rollback.asi"))?1:0);
 const LONG active=Active()?1:0;
 if(active!=wasActive){wasActive=active;Clear();if(active)Queue("ROLLBACK costumes : salon avec rollback actif, suivi des recalculs arme.");return;}
 if(!active)return;
 const uint32_t head=uint32_t(InterlockedCompareExchange(&logicLast,0,0));
 struct Undo {void* entity;bool flip;uint32_t kind,side,number,frame;unsigned slot;uint32_t gen;};
 Undo undo[kEvents];unsigned n=0;
 AcquireSRWLockExclusive(&lock);
 for(unsigned s=0;s<kEvents;++s){Event& e=events[s];if(!(e.live&&e.pending&&head>e.frame+1))continue;
  if(e.applied)undo[n++]={e.entity,e.flip,e.kind,e.side,e.number,e.frame,s,e.gen};
  e.live=false;e.pending=false;InterlockedIncrement(&cancels);
  if(!e.applied){char l[160];std::snprintf(l,sizeof l,"ROLLBACK annule avant application : genre %u cote %u script %u image %u (image predite).",unsigned(e.kind),unsigned(e.side),unsigned(e.number),unsigned(e.frame));Queue(l);}
 }
 ReleaseSRWLockExclusive(&lock);
 for(unsigned i=0;i<n;++i){
  const bool ok=undo[i].flip?StateCostumes::OnToggleSchoolgirl(undo[i].entity):StateCostumes::RevertForm(undo[i].entity,false);
  if(!ok){
   AcquireSRWLockExclusive(&lock);Event& e=events[undo[i].slot];if(e.gen==undo[i].gen){e.live=true;e.pending=true;}ReleaseSRWLockExclusive(&lock);continue;}
  char l[192];std::snprintf(l,sizeof l,"ROLLBACK %s annulee : genre %u cote %u script %u image %u n'a pas eu lieu (image predite) ; forme d'avant remise=%u.",
   undo[i].flip?"bascule":"casse",unsigned(undo[i].kind),unsigned(undo[i].side),unsigned(undo[i].number),unsigned(undo[i].frame),unsigned(ok));Queue(l);
 }
}
}
