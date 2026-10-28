bool originalSchoolgirlEnabled=false;

void* originalSchoolgirlVtable[8]{};
bool originalSchoolgirlVtableReady=false;

void* schoolgirlSphereVtable[6]{};
bool schoolgirlSphereVtableReady=false;
unsigned schoolgirlDrawDiagnostics=0;
uint32_t SchoolgirlSphereDrawTarget() noexcept {

 const BYTE cameraDrawPrefix[]={0x55,0x8b,0xec,0x81,0xec,0x88,0,0,0};
 return Signature(0x42d820,cameraDrawPrefix,sizeof cameraDrawPrefix)
  ?0x42d820:0x42d8b0;
}

#include "EffectTuningCache.h"
EffectTuning::Cache effectTuningCache;
EffectTuning::Snapshot RefreshEffectTuning() noexcept {return effectTuningCache.Read();}
void PollEffectTuningFromWorker() noexcept {
 effectTuningCache.PollFromWorker(GetTickCount(),[](const char* key,int fallback) noexcept {
  static char ini[MAX_PATH]{};
  if(!ini[0]){GetModuleFileNameA(nullptr,ini,MAX_PATH);char* e=std::strrchr(ini,92);if(e)std::snprintf(ini,MAX_PATH,"%s",Extra_Ini());}
  return static_cast<int>(GetPrivateProfileIntA("StatefulCostumesEffects",key,fallback,ini));
 });
}
void DrawSchoolgirlSphereFacingCamera(BYTE* sphere,void* context) noexcept {

 const uint32_t target=SchoolgirlSphereDrawTarget();
 reinterpret_cast<void(__thiscall*)(void*,void*)>(At(target))(sphere,context);
}
void __fastcall SchoolgirlSphereDraw(BYTE* sphere,void*,void* context) noexcept {

 float original=0;
 bool sample=false;
 float observedScale=0;uint32_t observedColor=0;
 if(schoolgirlDrawDiagnostics<16&&Read(sphere+0x1c,original)
  &&Read(sphere+0x10,observedScale)&&Read(sphere+0x18,observedColor)){
  if((schoolgirlDrawDiagnostics%8)==0){
   sample=true;
   char line[192];std::snprintf(line,sizeof line,
    "Sphere pre-override instance=%p envelope=%.4f native_scale=%.4f color=%08X alpha=%.4f",
    sphere,double(original),double(observedScale),unsigned(observedColor),double(original*observedScale));Queue(line);
  }
  ++schoolgirlDrawDiagnostics;
 }
 if(!Read(sphere+0x1c,original)||!std::isfinite(original)||original<0||original>1){
  DrawSchoolgirlSphereFacingCamera(sphere,context);return;
 }

 float nativeAlpha=0;
 if(!Read(sphere+0x10,nativeAlpha)||!std::isfinite(nativeAlpha)||nativeAlpha<0){
  DrawSchoolgirlSphereFacingCamera(sphere,context);return;
 }
 const auto tuning=RefreshEffectTuning();
 const float peakAlpha=tuning.peakAlpha;
 const float enhanced=(nativeAlpha>0&&original*nativeAlpha>peakAlpha)
  ?peakAlpha/nativeAlpha:original;
 uint32_t originalColor=0;
 if(!Read(sphere+0x18,originalColor))return;

 uint32_t softenedColor=originalColor&0xff000000u;
 for(unsigned shift=0;shift<24;shift+=8)
  softenedColor|=((((originalColor>>shift)&255u)*tuning.sphereBright/100u)&255u)<<shift;
 std::memcpy(sphere+0x1c,&enhanced,4);
 std::memcpy(sphere+0x18,&softenedColor,4);
 const uint32_t drawTarget=SchoolgirlSphereDrawTarget();
 if(sample){
  BYTE* state=nullptr;
  float inheritedAlpha=std::numeric_limits<float>::quiet_NaN();
  float material=std::numeric_limits<float>::quiet_NaN();
  if(Read(At(0x12c8f98),state)){
   Read(state+0x68,inheritedAlpha);Read(state+0x70,material);
  }

  char line[256];std::snprintf(line,sizeof line,
   "Sphere submission instance=%p draw=%08X alpha=%.5f context_alpha=%.5f product=%.5f rgb=%08X material=%.5f",
   sphere,unsigned(drawTarget),double(enhanced*nativeAlpha),double(inheritedAlpha),
   double(enhanced*nativeAlpha*inheritedAlpha),unsigned(softenedColor),double(material));Queue(line);
 }
 reinterpret_cast<void(__thiscall*)(void*,void*)>(At(drawTarget))(sphere,context);
 std::memcpy(sphere+0x18,&originalColor,4);
 std::memcpy(sphere+0x1c,&original,4);
}

void* schoolgirlRibbonVtable[6]{};
bool schoolgirlRibbonVtableReady=false;
void __fastcall SchoolgirlRibbonDraw(BYTE* child,void*,void* context) noexcept {

 const auto tuning=RefreshEffectTuning();
 uint32_t color=0;
 if(!Read(child+0x18,color))return;
 uint32_t balanced=color&0xff000000u;
 for(unsigned shift=0;shift<24;shift+=8)
  balanced|=((((color>>shift)&255u)*tuning.ribbonBright/100u)&255u)<<shift;
 std::memcpy(child+0x18,&balanced,4);
 reinterpret_cast<void(__thiscall*)(void*,void*)>(At(0x42d820))(child,context);
 std::memcpy(child+0x18,&color,4);
}
void TuneSchoolgirlSpheres(BYTE* object) noexcept {

 if(U32(object+ParticleResetLifetime::TuningDoneOffset))return;
 BYTE* node=nullptr;
 if(!Read(object+0x1c,node))return;
 for(unsigned count=0;node&&count<512;++count){
  BYTE* child=nullptr;BYTE* next=nullptr;BYTE* vtable=nullptr;
  if(!Read(node,next)||!Read(node+8,child)||!Read(child,vtable))return;
  if(vtable==At(0xd6cd80)&&Accessible(child,0xcc,true)
    &&Accessible(At(0xd6cd7c),sizeof schoolgirlSphereVtable)){
   if(!schoolgirlSphereVtableReady){
    std::memcpy(schoolgirlSphereVtable,At(0xd6cd7c),sizeof schoolgirlSphereVtable);
    schoolgirlSphereVtable[3]=reinterpret_cast<void*>(&SchoolgirlSphereDraw);
    schoolgirlSphereVtableReady=true;
   }
   *reinterpret_cast<void***>(child)=schoolgirlSphereVtable+1;
   static unsigned spheres=0;
   if(++spheres<=2)Queue("Schoolgirl sphere camera-facing draw wrapper attached to native child instance.");
  }
  if(vtable==At(0xd6cd3c)&&Accessible(child,0xcc,true)
    &&Accessible(At(0xd6cd38),sizeof schoolgirlRibbonVtable)){
   const BYTE cameraDrawPrefix[]={0x55,0x8b,0xec,0x81,0xec,0x88,0,0,0};
   if(Signature(0x42d820,cameraDrawPrefix,sizeof cameraDrawPrefix)){
    if(!schoolgirlRibbonVtableReady){
     std::memcpy(schoolgirlRibbonVtable,At(0xd6cd38),sizeof schoolgirlRibbonVtable);
     schoolgirlRibbonVtable[3]=reinterpret_cast<void*>(&SchoolgirlRibbonDraw);
     schoolgirlRibbonVtableReady=true;
    }
    *reinterpret_cast<void***>(child)=schoolgirlRibbonVtable+1;
    static unsigned rubans=0;
    if(++rubans<=2)Queue("Schoolgirl ribbon RGB balance attached to native child instance.");
   }
  }
  node=next;
 }

 if(!node&&!U32(object+0x24)){
  const uint32_t done=1;
  std::memcpy(object+ParticleResetLifetime::TuningDoneOffset,&done,4);
 }
}

void SchoolgirlBasis(BYTE* object,const EntityInfo& actor) noexcept {
 alignas(16) float matrix[20]={};
 matrix[0]=matrix[5]=matrix[10]=matrix[15]=1.0f;
 reinterpret_cast<void(__cdecl*)(float*,void*,int)>(At(0x8a3e60))(matrix+16,actor.entity,2);
 for(float value:matrix)if(!std::isfinite(value)||std::fabs(value)>10000.0f)return;
 std::memcpy(object+0x2c,matrix,sizeof matrix);
}
void __fastcall SchoolgirlUpdate(BYTE* object,void*,void* delta) noexcept {
 NativeCostumeEffectScope effects;
 if(!effects){*reinterpret_cast<uint32_t*>(object+4)=0;return;}
 if(!ParticleResetLifetime::Keep(object,static_cast<uint32_t>(InterlockedCompareExchange(&resetEpoch,0,0))))return;

 EntityInfo actor{};BYTE* owner=nullptr;
 if(!Read(object+0xc,owner)||!Entity(owner,actor,true)){
  *reinterpret_cast<uint32_t*>(object+4)=0;return;
 }
 reinterpret_cast<void(__thiscall*)(void*,void*)>(At(0x57edd0))(object,delta);
 TuneSchoolgirlSpheres(object);
 SchoolgirlBasis(object,actor);
}
void __fastcall SchoolgirlDraw(BYTE* object,void*,void* context) noexcept {
 NativeCostumeEffectScope effects;
 if(!effects){*reinterpret_cast<uint32_t*>(object+4)=0;return;}
 if(!ParticleResetLifetime::Keep(object,static_cast<uint32_t>(InterlockedCompareExchange(&resetEpoch,0,0))))return;

 BYTE* previous=nullptr;
 if(!Read(At(0x12c8f98),previous)||!Accessible(previous,124)
    ||!Accessible(At(0x12c8f98),4,true))return;
 alignas(16) BYTE state[124];std::memcpy(state,previous,sizeof state);

 reinterpret_cast<void(__fastcall*)(void*)>(At(0xbb97f0))(state);

 const float materialEffect=0.2f;std::memcpy(state+0x70,&materialEffect,4);
 *reinterpret_cast<BYTE**>(At(0x12c8f98))=state;
 reinterpret_cast<void(__thiscall*)(void*,void*)>(At(0x4264e0))(object+0x18,context);
 *reinterpret_cast<BYTE**>(At(0x12c8f98))=previous;
}
bool TryOriginalSchoolgirl(const EntityInfo& actor,uint32_t tick,BYTE* root) noexcept {
 if(!originalSchoolgirlEnabled)return false;
 if(!Accessible(root,0x170)||std::memcmp(root,"TMC",3)
    ||!Accessible(At(0xfdae70),sizeof originalSchoolgirlVtable))return false;
 static const uint32_t keyWords[]={0x05100415,0x2a24c618,0x348e8282,0x4441c473,0x2442e268};
 struct Key {const uint32_t* words;uint32_t hash;};
 const Key key{keyWords,0x3941725c};
 auto groupLookup=reinterpret_cast<BYTE*(__cdecl*)(void*,const Key*)>(At(0xbbbb70));
 BYTE* group=groupLookup(root,&key);

 if(!group||!Accessible(group,2616)||U32(group+0x10)!=29||U32(group+0x14)!=2
    ||U32(group+0x18)!=94||U32(group+0x1c)!=222)return false;
 BYTE* work=reinterpret_cast<BYTE*(__cdecl*)(void*)>(At(0xbbbb10))(group);
 if(!work||!Accessible(work,2464)||std::memcmp(work,"WorkPack",8)
    ||U32(work+12)!=2464||U32(work+0x24)!=0x50)return false;

 BYTE* bank=work+0x60;
 if(std::memcmp(bank,"BankPack",8)||U32(bank+12)!=0x940
    ||U32(bank+0x14)!=36||U32(bank+0x18)!=12||U32(bank+0x1c)!=28
    ||U32(bank+0x24)!=0x2c0)return false;
 const BYTE callbackPrefix[]={0x55,0x8b,0xec};
 if(!Signature(0x442d40,callbackPrefix,sizeof callbackPrefix)
    ||!Signature(0x442be0,callbackPrefix,sizeof callbackPrefix)
    ||!Signature(0x57edd0,callbackPrefix,sizeof callbackPrefix))return false;
 BYTE* node=reinterpret_cast<BYTE*(__cdecl*)(unsigned)>(At(0x855780))(ParticleResetLifetime::AllocationSize);
 if(!node)return false;
 BYTE* object=node+12;std::memset(object,0,ParticleResetLifetime::ObjectSize);
 reinterpret_cast<void(__thiscall*)(void*,void*,uint32_t,uint32_t)>(At(0x8453d0))
  (object,actor.entity,tick,0);
 ParticleResetLifetime::Stamp(object,static_cast<uint32_t>(InterlockedCompareExchange(&resetEpoch,0,0)));
 if(!originalSchoolgirlVtableReady){
  std::memcpy(originalSchoolgirlVtable,At(0xfdae70),sizeof originalSchoolgirlVtable);
  originalSchoolgirlVtable[2]=reinterpret_cast<void*>(&SchoolgirlUpdate);
  originalSchoolgirlVtable[3]=reinterpret_cast<void*>(&SchoolgirlDraw);
  originalSchoolgirlVtableReady=true;
 }
 *reinterpret_cast<void***>(object)=originalSchoolgirlVtable+1;
 *reinterpret_cast<BYTE**>(object+0x18)=At(0xfd8618);
 *reinterpret_cast<BYTE**>(object+0x20)=At(0xfd8618);
 SchoolgirlBasis(object,actor);
 struct Descriptor {float offset[4];uint32_t modelBase,textureBase;BYTE* root;BYTE* lists;};
 static_assert(sizeof(Descriptor)==32,"Native 32-bit particle descriptor");
 Descriptor descriptor{{0,0,0,0},94,222,root,object+0x18};
 uint32_t properties[72]={};
 reinterpret_cast<void(__thiscall*)(void*,BYTE**)>(At(0xbbda60))(properties,&bank);
 struct Row {BYTE* values;BYTE* bank;};
 using Callback=void(__cdecl*)(const Descriptor*,const Row*,const uint32_t*);
 for(unsigned channel=0;channel<12;++channel){
  Row row{bank+0x2c0+channel*28*4,bank};

  const uint32_t target=(channel==0||channel==3)?0x442d40:0x442be0;
  reinterpret_cast<Callback>(At(target))(&descriptor,&row,properties);
  if(channel==0||channel==3){
   char diagnostic[192];std::snprintf(diagnostic,sizeof diagnostic,
    "Schoolgirl sphere emitter channel=%u first_list=%08X second_list=%08X",channel,
    unsigned(U32(object+0x1c)),unsigned(U32(object+0x24)));Queue(diagnostic);
  }
 }
 *reinterpret_cast<uint32_t*>(object+4)=(U32(object+0x1c)||U32(object+0x24))?1:0;
 TuneSchoolgirlSpheres(object);

 BYTE* manager=At(0x246c510);
 *reinterpret_cast<BYTE**>(node+8)=object;
 *reinterpret_cast<uint32_t*>(node+4)=0;
 *reinterpret_cast<BYTE**>(node)=*reinterpret_cast<BYTE**>(manager+4);
 *reinterpret_cast<BYTE**>(manager+4)=node;
 Queue("Original Schoolgirl particles queued:12 channels; admitted online calls use isolated RNG.");
 return true;
}
