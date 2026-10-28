struct CliffTiming { uint32_t character,script,frame; };
static const CliffTiming kCliffTiming[]={
 {0,850,35},{1,860,38},{2,900,35},{3,820,35},{4,850,35},{5,846,35},
 {6,820,35},{7,850,35},{8,820,35},{9,820,35},{10,1820,35},{11,850,35},
 {12,850,35},{13,850,35},{14,820,35},{15,850,35},{16,890,35},
 {19,820,35},{20,850,40},{21,900,35},{24,850,35},{29,2500,35},
 {30,2500,35},{39,2500,35},{40,2500,35},{42,2500,35},{43,846,35},
 {44,2500,35},{45,2500,35},{46,2500,35},{47,2500,35}
};
static const CliffTiming* FindCliffTiming(uint32_t character) noexcept {
 for(const auto& row:kCliffTiming)if(row.character==character)return &row;
 return nullptr;
}
struct CliffScriptToken {
 void* entity{};const char* model{};uint32_t character{},generation{},number{},serial{};
 LONG epoch{};bool valid{};
};
static CliffScriptToken cliffScriptToken[2][2]{};
static bool MatchesCliffScriptToken(const CliffScriptToken& token,const EntityInfo& actor,uint32_t number,uint32_t serial,LONG epoch) noexcept {
 return token.valid&&token.entity==actor.entity&&token.character==actor.identity0
  &&token.generation==actor.identity4&&token.epoch==epoch&&token.number==number&&token.serial==serial;
}
static bool OwnsCliffScriptKnownModel(const EntityInfo& actor,uint32_t number,uint32_t serial,const char* model,LONG epoch) noexcept {
 for(const auto& token:cliffScriptToken[actor.side])
  if(MatchesCliffScriptToken(token,actor,number,serial,epoch)&&token.model==model)return true;
 return false;
}
static bool OwnsCliffScript(const EntityInfo& actor,uint32_t number,uint32_t serial) noexcept {
 const LONG epoch=InterlockedCompareExchange(&resetEpoch,0,0);
 for(const auto& token:cliffScriptToken[actor.side]){

  if(MatchesCliffScriptToken(token,actor,number,serial,epoch)
   &&token.model==StateCostumes::ModelName(actor.entity))return true;
 }
 return false;
}
struct CliffTransition {
 void* entity{};const char* model{};uint32_t character{},generation{};
 LONG epoch{};float lastClock{},armedClock{};int32_t lastFrame{};
 float pendingElapsed{};ULONGLONG armedWall{};
 rb::Handle h{};
 bool tracking{},armed{},applied{},cancelled{};
};
static CliffTransition cliffTransition[2]{};
static void ClearCliffTracking() noexcept {
 for(unsigned side=0;side<2;++side){cliffTransition[side]={};for(auto& token:cliffScriptToken[side])token={};}
}
static void CancelCliffForPB(unsigned side) noexcept {
 if(side<2){cliffTransition[side].armed=false;cliffTransition[side].cancelled=true;}
}
static bool ReadCliffClock(unsigned side,int32_t& frame,float& elapsed,bool& interpolating) noexcept {

 uint8_t blend=0;int16_t steps=0;
 if(!Read(At(0x13cab10+side*0x3c),elapsed)||!std::isfinite(elapsed)||elapsed<0||elapsed>100000
  ||!Read(At(0x13cc5e0+side),blend)||!Read(At(0x13cc208+side*2),steps))return false;
 interpolating=blend&&steps>0;
 if(interpolating){int16_t current=0;if(!Read(At(0x13cac8c+side*2),current))return false;frame=current;}
 else if(!Read(At(0x13cab24+side*0x3c),frame))return false;
 return frame>=0&&frame<100000;
}
static void CliffAbandonLog(unsigned side,const char* why,const EntityInfo& actor) noexcept {
 char line[160];std::snprintf(line,sizeof line,"Cliff D1 abandon side=%u raison=%s action=%u mode=%u",side,why,actor.action,unsigned(actor.mode));Queue(line);
}
static void CliffTick() noexcept {
 for(unsigned side=0;side<2;++side){
  CliffTransition& state=cliffTransition[side];
  void* pe=reinterpret_cast<void*(__cdecl*)(int)>(At(0x982670))(int(side));EntityInfo actor{};
  const bool alive=Entity(pe,actor,true)&&actor.side==side;

  if(alive&&state.armed&&!(actor.action==321&&actor.mode==13)){
   const LONG epoch=InterlockedCompareExchange(&resetEpoch,0,0);const char* why=nullptr;
   int32_t frame=0;float clock=0;bool interpolating=false;
   const bool clockOk=ReadCliffClock(side,frame,clock,interpolating);
   if(actor.entity!=state.entity)why="entity";
   else if(actor.identity0!=state.character||actor.identity4!=state.generation)why="identity";
   else if(epoch!=state.epoch)why="reset";
   else if(GetTickCount64()-state.armedWall>5000)why="delai";
   else if(StateCostumes::ModelName(actor.entity)!=state.model)why="model";
   if(why){char line[192];std::snprintf(line,sizeof line,"Cliff D1 abandon side=%u raison=%s action=%u mode=%u cumul=%.3f",
     side,why,actor.action,unsigned(actor.mode),double(state.pendingElapsed));Queue(line);state={};continue;}
   if(!clockOk)continue;
   state.pendingElapsed+=clock>=state.lastClock?clock-state.lastClock:clock;
   state.lastClock=clock;
   if(state.pendingElapsed<10.f)continue;
   if(!rb::Live(state.h)){CliffAbandonLog(side,"rollback-image-predite",actor);state={};continue;}
   if(!StateCostumes::OnToggleSchoolgirl(actor.entity)){
    if(!state.cancelled){state.cancelled=true;char line[160];std::snprintf(line,sizeof line,"Cliff D1 refus OnToggleSchoolgirl side=%u action=%u (nouvel essai)",side,actor.action);Queue(line);}
    continue;
   }
   char line[192];std::snprintf(line,sizeof line,"Cliff D1 apply (apres changement d'action) side=%u character=%u action=%u cumul=%.3f transformed=%u",
    side,actor.identity0,actor.action,double(state.pendingElapsed),unsigned(StateCostumes::IsTransformed(actor.entity)));Queue(line);
   rb::MarkApplied(state.h);state={};continue;
  }
  if(!alive||actor.action!=321||actor.mode!=13){
   if(state.armed){char line[160];std::snprintf(line,sizeof line,"Cliff D1 abandon side=%u raison=%s action=%u mode=%u",
    side,!alive?"acteur":"?",actor.action,unsigned(actor.mode));Queue(line);}
   state={};continue;
  }

  const char* model=StateCostumes::ModelName(actor.entity);
  if(!UsesSchoolgirlTimingModelName(model)){if(state.armed)CliffAbandonLog(side,"modele-321",actor);state={};continue;}
  const auto* row=FindCliffTiming(actor.identity0);int32_t frame=0;float clock=0;bool interpolating=false;
  if(!row){state={};continue;}
  if(!ReadCliffClock(side,frame,clock,interpolating)){if(state.armed)CliffAbandonLog(side,"horloge-script",actor);state.armed=false;state.cancelled=true;continue;}
  const LONG epoch=InterlockedCompareExchange(&resetEpoch,0,0);

  const ScriptRun& run=scriptRun[side];
  if(run.serial&&run.number==row->script&&!OwnsCliffScriptKnownModel(actor,run.number,run.serial,model,epoch)){
   cliffScriptToken[side][1]=cliffScriptToken[side][0];
   cliffScriptToken[side][0]={actor.entity,model,actor.identity0,actor.identity4,run.number,run.serial,epoch,true};
  }
  const bool same=state.tracking&&state.entity==actor.entity&&state.model==model
   &&state.character==actor.identity0&&state.generation==actor.identity4&&state.epoch==epoch;
  if(!same||clock<state.lastClock){
   if(state.armed)CliffAbandonLog(side,same?"horloge-recule":"suivi",actor);
   state={};state.entity=actor.entity;state.model=model;state.character=actor.identity0;
   state.generation=actor.identity4;state.epoch=epoch;state.tracking=true;
  }
  state.lastClock=clock;state.lastFrame=frame;
  if(state.cancelled)continue;

  const bool reached=interpolating?frame>int32_t(row->frame):frame>=int32_t(row->frame);
  if(!state.armed&&!state.applied&&reached){
   if(run.number==row->script&&!rb::Claim(rb::KCliff,side,row->script,0,run.startF,actor.entity,state.h)){state.applied=true;continue;}
   state.armed=true;state.armedClock=clock;state.armedWall=GetTickCount64();state.pendingElapsed=0;
   TransitionSound(actor,true);TransitionBurst(actor,true,true);
   char line[176];std::snprintf(line,sizeof line,"Cliff D1 arm side=%u character=%u script=%u frame=%d PS4frame=%u clock=%.3f",
    side,actor.identity0,row->script,int(frame),row->frame,double(clock));Queue(line);
  }

  if(state.armed&&clock-state.armedClock>=10.f){
   if(!rb::Live(state.h)){CliffAbandonLog(side,"rollback-image-predite",actor);state.armed=false;state.applied=true;continue;}
   if(!StateCostumes::OnToggleSchoolgirl(actor.entity))continue;
   state.armed=false;state.applied=true;rb::MarkApplied(state.h);
   char line[176];std::snprintf(line,sizeof line,"Cliff D1 apply side=%u character=%u frame=%d clock=%.3f elapsed=%.3f transformed=%u",
    side,actor.identity0,int(frame),double(clock),double(clock-state.armedClock),unsigned(StateCostumes::IsTransformed(actor.entity)));Queue(line);
  }
 }
}
