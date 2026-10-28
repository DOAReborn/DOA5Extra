namespace {
struct IntegratedNative {
 BYTE* active[2]{};
 BYTE* opponent[2]{};
 bool pb[2]{},taunt[2]{},property=false;
 unsigned renders=0,resets=0,propertyCalls=0,activeCalls=0,surfaceCalls=0;
 uint32_t propertyAction=0,propertySelector=0;
 std::vector<BYTE> drawFlags[2];
} integratedNative;
void* __cdecl IntegratedActive(int side){++integratedNative.activeCalls;return side>=0&&side<2?integratedNative.active[side]:nullptr;}
int __cdecl IntegratedSurface(uint32_t,uint32_t){++integratedNative.surfaceCalls;return 7;}
BYTE __cdecl IntegratedPB(int side){return side>=0&&side<2&&integratedNative.pb[side];}
BYTE __cdecl IntegratedTaunt(int side){return side>=0&&side<2&&integratedNative.taunt[side];}
void* __cdecl IntegratedOpponent(const void* key){
 for(unsigned side=0;side<2;++side)
  if(integratedNative.active[side]&&key==integratedNative.active[side]+0x58)return integratedNative.opponent[side];
 return nullptr;
}
BYTE __cdecl IntegratedProperty(int32_t* action,uint32_t selector){
 ++integratedNative.propertyCalls;integratedNative.propertyAction=uint32_t(*action);
 integratedNative.propertySelector=selector;
 *action=-123;
 return integratedNative.property;
}
void __cdecl IntegratedRender(void* value,void*,uint32_t){
 ++integratedNative.renders;
 BYTE* entity=static_cast<BYTE*>(value);BYTE *begin=nullptr,*end=nullptr;BYTE side=0;
 if(Read(entity+0x36,side)&&side<2&&Read(entity+0x24,begin)&&Read(entity+0x28,end)
   &&begin&&end>begin&&size_t(end-begin)<=StateCostumes::Data::kMaxNodes&&Accessible(begin,size_t(end-begin)))
  integratedNative.drawFlags[side].assign(begin,end);
}
void __cdecl IntegratedReset(){
 ++integratedNative.resets;

 for(unsigned side=0;side<2;++side){
  BYTE* player=At(0x13d0540)+side*0x6c8;
  Put32(player+0x6c,0);player[0x70]=0;
  integratedNative.pb[side]=integratedNative.taunt[side]=false;
 }
 integratedNative.property=false;
 Put32(At(0x13ce9f8),0);Put32(At(0x13cea00),0);
}
struct IntegratedActor {
 BYTE entity[128]{};
 const BYTE* holder=nullptr;
 const StateCostumes::Data::Model* model=nullptr;
 std::vector<BYTE> body,flags,baseline;
 unsigned side=0;
 bool Load(const char* root,const char* name,unsigned chosenSide,unsigned generation){
  side=chosenSide;
  for(const auto& item:StateCostumes::Data::kModels)if(!std::strcmp(item.name,name))model=&item;
  if(!model)return false;
  const std::string path=std::string(root)+"/"+name+"/"+name+".TMC";
  std::ifstream file(path,std::ios::binary);
  body.assign(std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>());
  if(body.size()!=model->size||U32(body.data()+0x10)!=model->size
    ||StateCostumes::Data::Fnv64(body.data()+0x50,16)!=model->identityHash)return false;
  flags.resize(model->nodes);baseline.resize(model->nodes);
  for(unsigned i=0;i<model->nodes;++i)flags[i]=baseline[i]=BYTE(i%3);
  holder=body.data();Put32(entity,10);Put32(entity+4,1|(generation<<16));
  Put32(entity+8,uint32_t(reinterpret_cast<uintptr_t>(&holder)));
  Put32(entity+0x24,uint32_t(reinterpret_cast<uintptr_t>(flags.data())));
  Put32(entity+0x28,uint32_t(reinterpret_cast<uintptr_t>(flags.data()+flags.size())));
  entity[0x34]=0;entity[0x35]=0;entity[0x36]=BYTE(side);
  Put32(entity+0x40,uint32_t(reinterpret_cast<uintptr_t>(At(0x13d0540)+side*0x6c8)));
  return true;
 }
 bool State(bool transformed)const{
  auto expected=baseline;
  for(unsigned i=0;i<model->opCount;++i){
   const auto& op=StateCostumes::Data::kOps[model->opStart+i];
   const BYTE mode=transformed?op.transformed:op.initial;
   expected[op.node]=mode==2?baseline[op.node]:mode;
  }
  return flags==expected;
 }
 void NativeReuse(unsigned generation){
  Put32(entity+4,1|(generation<<16));
  for(unsigned i=0;i<model->nodes;++i)flags[i]=baseline[i]=BYTE((i+1)%3);
 }
};
void IntegratedAction(unsigned side,uint32_t action,BYTE mode,bool taunt=false,bool pb=false){
 BYTE* player=At(0x13d0540)+side*0x6c8;
 Put32(player+0x6c,action);player[0x70]=mode;player[0x100]=7;
 integratedNative.taunt[side]=taunt;integratedNative.pb[side]=pb;
}
void IntegratedBind(IntegratedActor& left,IntegratedActor& right){
 integratedNative.active[0]=left.entity;integratedNative.active[1]=right.entity;
 integratedNative.opponent[0]=right.entity;integratedNative.opponent[1]=left.entity;
}

void IntegratedInputFrame(BYTE* manager,uint32_t frame){
 Put32(manager+0x1c,frame);
 Put32(At(0x112ebc4),1);*At(0x14885be)=0;
}
void IntegratedJump(uint32_t va,const void* function){
 BYTE* stub=At(va);stub[0]=0xb8;Put32(stub+1,uint32_t(reinterpret_cast<uintptr_t>(function)));
 stub[5]=0xff;stub[6]=0xe0;
}
using IntegratedRngBytes=std::array<std::array<BYTE,0x9c8>,2>;
static unsigned integratedRngMutationCalls=0;
static constexpr uint32_t integratedRngAddresses[]={0x1487ab0,0x24906e0};
void IntegratedSeedRng(){
 for(unsigned stream=0;stream<2;++stream){
  BYTE* block=At(integratedRngAddresses[stream]);
  for(unsigned word=0;word<624;++word)Put32(block+word*4,0x31415926u+stream*0x10000u+word*17u);
  Put32(block+0x9c0,uint32_t(uintptr_t(block)));Put32(block+0x9c4,624);
 }
}
IntegratedRngBytes IntegratedSnapshotRng(){
 IntegratedRngBytes result{};
 for(unsigned stream=0;stream<2;++stream)std::memcpy(result[stream].data(),At(integratedRngAddresses[stream]),0x9c8);
 return result;
}
bool IntegratedRngEquals(const IntegratedRngBytes& before){
 for(unsigned stream=0;stream<2;++stream)
  if(std::memcmp(before[stream].data(),At(integratedRngAddresses[stream]),0x9c8))return false;
 return true;
}

void IntegratedMutateRng() noexcept {
 ++integratedRngMutationCalls;
 for(uint32_t address:integratedRngAddresses){
  BYTE* block=static_cast<BYTE*>(costumeEffectRng.Route(At(address)));
  const uint32_t count=U32(block+0x9c4);
  Put32(block+16,U32(block+16)+0x101u);
  Put32(block+0x9c0,count>1?U32(block+0x9c0)+4:uint32_t(uintptr_t(block))+4);
  Put32(block+0x9c4,count>1?count-1:624);
 }
}
bool IntegratedArena(){
 image=static_cast<BYTE*>(VirtualAlloc(nullptr,0x2100000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
 if(!image)return false;
 StateCostumes::SetRenderOnlyPredicate(IsNetworkContext);
 Put32(At(0x1487a4c),4);Put32(At(0x1382884),1);
 const BYTE factory[]={0x55,0x8b,0xec,0x6a,0xff};
 const BYTE position[]={0x55,0x8b,0xec,0x83,0xec,0x54};
 const BYTE submit[]={0x55,0x8b,0xec,0x83,0xec,0x0c,0x57,0x8b,0x7d,0x08,0x83,0xff,0xff};
 for(uint32_t va:{0x8a6f00u,0x8a6ae0u,0x8a34c0u})std::memcpy(At(va),factory,sizeof factory);
 std::memcpy(At(0x8a3e60),position,sizeof position);std::memcpy(At(0x957c40),submit,sizeof submit);
 IntegratedSeedRng();
 IntegratedJump(0x982670,reinterpret_cast<const void*>(&IntegratedActive));
 IntegratedJump(0x6b3730,reinterpret_cast<const void*>(&IntegratedPB));
 IntegratedJump(0x6b20c0,reinterpret_cast<const void*>(&IntegratedTaunt));
 IntegratedJump(0x982640,reinterpret_cast<const void*>(&IntegratedOpponent));
 IntegratedJump(0x6b0330,reinterpret_cast<const void*>(&IntegratedProperty));
 IntegratedJump(0x98e680,reinterpret_cast<const void*>(&IntegratedSurface));

 for(uint32_t va:{0x982670u,0x6b3730u,0x6b20c0u,0x982640u,0x6b0330u,0x98e680u,0x8a6f00u,0x8a6ae0u,0x8a34c0u,0x8a3e60u,0x957c40u,0x421080u}){
  DWORD old=0;if(!VirtualProtect(At(va),16,PAGE_EXECUTE_READ,&old))return false;
 }
 FlushInstructionCache(GetCurrentProcess(),image,0x2100000);

 if(!costumeEffectRng.Configure(At(0x1487ab0),At(0x24906e0)))return false;
 costumeEffectRngTestFixture=true;
 InterlockedExchange(&costumeEffectRngReady,1);
 return ObserveCostumeEffectEngineThread();
}
}
bool IntegratedEventSelfTest(const char* convertedRoot) noexcept {
 unsigned checks=0,failures=0;
 auto check=[&](bool value,const char* label){++checks;if(!value){++failures;std::printf("FAIL integrated: %s\n",label);}};

 if(image||prepared||rendererArmed||controlActive||!convertedRoot){std::puts("FAIL integrated: not a clean preflight process");return false;}
 IntegratedActor author,victim,replacement;
 bool initialized=IntegratedArena();
 if(initialized)initialized=author.Load(convertedRoot,"KOKORO_DLCU_032",0,1)
  &&victim.Load(convertedRoot,"KOKORO_DLCU_025",1,1)
  &&replacement.Load(convertedRoot,"KOKORO_DLCU_032",0,3);
 check(initialized,"private arena and actual source TMC fixtures");
 if(initialized){
  StateCostumes::ResetAll();StateCostumes::SetLogger(nullptr);
  integratedNative={};offlineEventSinks={};offlineEventSinks.enabled=true;
  offlineEventSinks.beforeSubmit=&IntegratedMutateRng;integratedRngMutationCalls=0;
  const auto rngBeforeOffline=IntegratedSnapshotRng();
  originalRender=&IntegratedRender;originalReset=&IntegratedReset;
  nativeBurstEnabled=nativeSoundEnabled=true;controlActive=1;resetEpoch=observedResetEpoch=0;
  edges[0]={};edges[1]={};IntegratedBind(author,victim);
  auto render=[](IntegratedActor& actor){RenderBridge(actor.entity,nullptr,0);};
  auto totals=[&](unsigned sounds,unsigned schoolgirl,unsigned destruction,const char* label){
   check(offlineEventSinks.sounds[0]==sounds&&offlineEventSinks.schoolgirl[0]==schoolgirl
    &&offlineEventSinks.destruction[1]==destruction&&offlineEventSinks.sounds[1]==0
    &&offlineEventSinks.schoolgirl[1]==0&&offlineEventSinks.destruction[0]==0
    &&offlineEventSinks.wrongDestructionHeader==0,label);
  };
  auto taunt=[&](IntegratedActor& actor){IntegratedAction(0,8009,5,true);render(actor);};
  auto neutral=[&](IntegratedActor& actor){IntegratedAction(actor.side,0,0);render(actor);};
  auto reset=[&](IntegratedActor& actor){ResetBridge();render(actor);render(victim);};
  render(author);render(victim);
  check(author.State(false)&&victim.State(false),"first real render captures both initial states");
  totals(0,0,0,"neutral initial render emits nothing");
  taunt(author);check(author.State(true)&&victim.State(false),"taunt transforms only author");
  totals(1,1,0,"first taunt emits one admitted sound and Schoolgirl effect");
  check(offlineEventSinks.soundActor[0]==author.entity&&offlineEventSinks.schoolgirlActor[0]==author.entity,"taunt sinks receive actual author pointer");
  render(author);totals(1,1,0,"held taunt edge emits no duplicate");
  neutral(author);taunt(author);totals(1,1,0,"second rising edge while transformed stays inert");

  Put32(At(0x1487a4c),3);render(author);Put32(At(0x1487a4c),4);render(author);
  totals(1,1,0,"pause/resume preserves edge and idempotent state");
  reset(author);check(integratedNative.resets==1&&observedResetEpoch==resetEpoch,"real ResetBridge epoch consumed on render");
  check(author.State(false)&&victim.State(false),"reset restores initial visibility without native flag initialization");
  totals(1,1,0,"reset and first neutral frame emit nothing");
  render(author);totals(1,1,0,"repeated neutral frame after reset emits nothing");
  taunt(author);totals(2,2,0,"new transition after reset emits exactly once");
  author.NativeReuse(2);neutral(author);
  check(author.State(false),"same-address generation reuse starts from fresh native baseline");
  totals(2,2,0,"generation replacement neutral frame does not emit");
  taunt(author);totals(3,3,0,"new generation can transform exactly once");
  IntegratedBind(replacement,victim);IntegratedAction(0,8009,5,true);render(author);
  totals(3,3,0,"stale rendered entity rejected by actual active-actor guard");
  neutral(replacement);check(replacement.State(false),"different active entity starts initial");
  taunt(replacement);totals(4,4,0,"replacement entity receives its own transition");
  check(offlineEventSinks.soundActor[0]==replacement.entity&&offlineEventSinks.schoolgirlActor[0]==replacement.entity,"replacement sinks point to new entity");

  IntegratedAction(0,8500,6,false,true);render(replacement);
  check(victim.State(true),"PB transitions destructible victim");
  totals(4,4,1,"PB emits only newly admitted victim destruction");
  check(offlineEventSinks.destructionActor[1]==victim.entity&&DestructionHeaderControlled(victim.entity),"post-break model still recognized and effect owner is victim");
  render(replacement);totals(4,4,1,"held PB emits no duplicate");
  neutral(replacement);IntegratedAction(0,8500,6,false,true);render(replacement);
  totals(4,4,1,"new PB edge on already broken victim emits no duplicate");
  reset(replacement);totals(4,4,1,"reset after PB emits no sound or destruction");

  IntegratedAction(0,8350,6);integratedNative.property=true;render(victim);
  totals(4,4,1,"PL author action alone (whiff) emits nothing");
  IntegratedAction(1,16001,9);render(victim);totals(4,4,1,"blocked PL victim action emits nothing");
  IntegratedAction(1,25633,9);render(victim);
  totals(5,5,2,"committed PL emits for correct author and victim");
  check(integratedNative.propertyAction==8350&&integratedNative.propertySelector==7
    &&U32(At(0x13d0540)+0x6c)==8350,"native PL query receives action COPY and zero-extended selector");
  check(replacement.State(true)&&victim.State(true),"PL changes both eligible families");
  render(victim);totals(5,5,2,"held PL reaction emits no duplicate");
  reset(replacement);
  IntegratedAction(0,8350,6);integratedNative.property=true;
  IntegratedAction(1,25633,9);integratedNative.opponent[1]=victim.entity;render(victim);
  totals(5,5,2,"same-side opponent rejected at actual pairing boundary");
  integratedNative.opponent[1]=replacement.entity;render(victim);
  totals(5,5,2,"repairing pairing during held reaction does not manufacture edge");
  neutral(victim);IntegratedAction(1,25633,9);render(victim);
  totals(6,6,3,"leave/reenter valid PL reaction admits one new transition");
  reset(replacement);
  Put32(At(0x13ce9f8),2);Put32(At(0x13cea00),30);
  Put32(At(0x13cea08),0);Put32(At(0x13cea04),1);
  IntegratedAction(0,0x142,13);render(replacement);
  totals(7,7,3,"cliff runner dispatch emits only author Schoolgirl and sound");
  check(!victim.State(true),"cliff does not break victim costume");
  render(replacement);totals(7,7,3,"held cliff edge emits no duplicate");
  reset(replacement);totals(7,7,3,"final reset emits nothing and leaves neutral state");
  check(replacement.State(false)&&victim.State(false),"final controller states initial");
  check(integratedNative.renders>30&&integratedNative.resets==5,"real render/reset bridges executed all scenarios");
  const unsigned submissions=offlineEventSinks.sounds[0]+offlineEventSinks.sounds[1]
   +offlineEventSinks.schoolgirl[0]+offlineEventSinks.schoolgirl[1]
   +offlineEventSinks.destruction[0]+offlineEventSinks.destruction[1];
  check(submissions&&integratedRngMutationCalls==submissions&&!IntegratedRngEquals(rngBeforeOffline),
   "offline effects retain native RNG advancement instead of borrowing isolated streams");
 }

 StateCostumes::ResetAll();offlineEventSinks.enabled=false;controlActive=0;
 originalRender=nullptr;originalReset=nullptr;nativeBurstEnabled=nativeSoundEnabled=false;
 if(image)VirtualFree(image,0,MEM_RELEASE);image=nullptr;edges[0]={};edges[1]={};
 std::printf("{\"ok\":%s,\"checks\":%u,\"failures\":%u,\"real_tmc_fixtures\":2,\"render_calls\":%u,\"reset_calls\":%u,\"sound_submissions\":%u,\"schoolgirl_submissions\":%u,\"destruction_submissions\":%u,\"game_process_accessed\":false,\"game_functions_executed\":false,\"native_fx_audio_replaced_after_guards\":true}\n",
  failures?"false":"true",checks,failures,integratedNative.renders,integratedNative.resets,
  offlineEventSinks.sounds[0],offlineEventSinks.schoolgirl[0],offlineEventSinks.destruction[1]);
 return failures==0;
}
