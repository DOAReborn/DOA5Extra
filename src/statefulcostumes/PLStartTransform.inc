struct PLStartAction {uint32_t character,charge,impact,chargeScript,impactScript;uint8_t chargeMode,impactMode;};
static const PLStartAction kPLStartActions[]={
#include "PLStartActions.inc"
};
static const PLStartAction* FindPLStartAction(uint32_t character,uint32_t action,uint8_t mode) noexcept {
 for(const auto& row:kPLStartActions)if(row.character==character
  &&((row.charge==action&&row.chargeMode==mode)||(row.impact==action&&row.impactMode==mode)))return &row;
 return nullptr;
}
struct PLStartState {
 void* entity{};const char* model{};uint32_t character{},generation{},impact{};LONG epoch{};
 bool active{},changed{},blocked{},effectAlready{},contact{};
};
static PLStartState plStartState[2]{};
static void ClearPLStartTracking() noexcept {
 for(unsigned side=0;side<2;++side){plStartState[side]={};confirmedPL[side]={};for(auto& token:previousPLScript[side])token={};}
}
static void PLStartTick(void* confirmedContactEntity=nullptr) noexcept {
 for(unsigned side=0;side<2;++side){
  auto& state=plStartState[side];EntityInfo actor{};

  const BYTE* player=At(0x13d0540+side*0x6c8);
  const uint32_t action=U32(player+0x6c);const uint8_t mode=player[0x70];
  bool potential=false;
  for(const auto& row:kPLStartActions)if((row.charge==action&&row.chargeMode==mode)
   ||(row.impact==action&&row.impactMode==mode)){potential=true;break;}
  if(!potential){state={};continue;}
  void* pe=reinterpret_cast<void*(__cdecl*)(int)>(At(0x982670))(int(side));
  if(!Entity(pe,actor,true)||actor.side!=side){state={};continue;}
  const auto* row=FindPLStartAction(actor.identity0,actor.action,actor.mode);
  if(!row){state={};continue;}
  const LONG epoch=InterlockedCompareExchange(&resetEpoch,0,0);
  const char* model=StateCostumes::ModelName(actor.entity);
  if(!model)continue;
  if(!UsesPowerTimingModelName(model)){state={};continue;}
  const bool same=state.active&&state.entity==actor.entity&&state.model==model
   &&state.character==actor.identity0&&state.generation==actor.identity4&&state.epoch==epoch&&state.impact==row->impact;
  if(!same){

   int32_t outputAction=int32_t(row->impact);
   if(!reinterpret_cast<BYTE(__cdecl*)(int32_t*,uint32_t)>(At(0x6b0330))(&outputAction,actor.selector)){state={};continue;}
   state={actor.entity,model,actor.identity0,actor.identity4,row->impact,epoch,true,false,false};
  }

  if(pbWas[side]){state.blocked=true;continue;}
  if(state.blocked)continue;
  const uint32_t number=actor.action==row->charge?row->chargeScript:row->impactScript;
  state.effectAlready|=BindPLScript(actor,number,model);

  if(actor.entity==confirmedContactEntity&&actor.action==row->impact)state.contact=true;
  if(!state.contact)continue;
  if(state.changed)continue;
  if(!StateCostumes::OnToggleSchoolgirl(actor.entity))continue;
  state.changed=true;CancelCliffForPB(side);
  if(!state.effectAlready){TransitionSound(actor,true);TransitionBurst(actor,true,true);}
  char line[192];std::snprintf(line,sizeof line,"PL contact toggle side=%u character=%u action=%u output=%u script=%u transformed=%u",
   side,actor.identity0,actor.action,row->impact,number,unsigned(StateCostumes::IsTransformed(actor.entity)));Queue(line);
 }
}
static void ConfirmPLContact(void* entity) noexcept {PLStartTick(entity);}
