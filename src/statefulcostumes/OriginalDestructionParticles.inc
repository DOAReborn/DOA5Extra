#include "DestructionPerf.inc"
void* originalDestructionVtable[8]{};
bool originalDestructionVtableReady=false;
unsigned destructionDiagnosticCount=0;
void DestructionDiagnostic(BYTE* object,const char* phase,void* delta=nullptr) noexcept {
 if(destructionDiagnosticCount>=6)return;
 ++destructionDiagnosticCount;
 unsigned counts[2]={};
 for(unsigned list=0;list<2;++list){
  BYTE* node=nullptr;
  if(!Read(object+0x1c+list*8,node))continue;
  while(node&&counts[list]<64){
   ++counts[list];BYTE* next=nullptr;
   if(!Read(node,next)||next==node)break;
   node=next;
  }
 }
 uint32_t ticks=0; if(delta)Read(static_cast<BYTE*>(delta)+8,ticks);
 float step[3]={};
 if(delta)for(unsigned i=0;i<3;++i)Read(static_cast<BYTE*>(delta)+i*4,step[i]);
 char line[240];std::snprintf(line,sizeof line,
  "Destruction diagnostic phase=%s root=%p drawables=%u emitters=%u delta=%.4f/%.4f tick=%u",
  phase,object,counts[0],counts[1],double(step[0]),double(step[1]),unsigned(ticks));Queue(line);
}
void __fastcall DestructionUpdate(BYTE* object,void*,void* delta) noexcept {
 NativeCostumeEffectScope effects;
 if(!effects){*reinterpret_cast<uint32_t*>(object+4)=0;return;}
 DestructionPerfScope perf("update",1);
 if(!ParticleResetLifetime::Keep(object,static_cast<uint32_t>(InterlockedCompareExchange(&resetEpoch,0,0))))return;

 EntityInfo actor{};BYTE* owner=nullptr;
 if(!Read(object+0xc,owner)||!Entity(owner,actor,true)){
  *reinterpret_cast<uint32_t*>(object+4)=0;return;
 }
 reinterpret_cast<void(__thiscall*)(void*,void*)>(At(0x57edd0))(object,delta);
 DestructionDiagnostic(object,"updated",delta);
 float remaining=0,step=0;
 if(Read(object+0xc0,remaining)&&Read(delta,step)&&std::isfinite(step)
    &&step>=0&&step<=16&&remaining>0){
  SchoolgirlBasis(object,actor);
  remaining-=step;std::memcpy(object+0xc0,&remaining,4);
 }
}
void __fastcall DestructionDraw(BYTE* object,void*,void* context) noexcept {
 NativeCostumeEffectScope effects;
 if(!effects){*reinterpret_cast<uint32_t*>(object+4)=0;return;}
 DestructionPerfScope perf("draw",2);
 if(!ParticleResetLifetime::Keep(object,static_cast<uint32_t>(InterlockedCompareExchange(&resetEpoch,0,0))))return;

 DestructionDiagnostic(object,"draw");
 BYTE* previous=nullptr;
 if(!Read(At(0x12c8f98),previous)||!Accessible(previous,124)
    ||!Accessible(At(0x12c8f98),4,true))return;
 alignas(16) BYTE state[124];std::memcpy(state,previous,sizeof state);
 reinterpret_cast<void(__fastcall*)(void*)>(At(0xbb97f0))(state);

 *reinterpret_cast<BYTE**>(At(0x12c8f98))=state;
 reinterpret_cast<void(__thiscall*)(void*,void*)>(At(0x4264e0))(object+0x18,context);
 *reinterpret_cast<BYTE**>(At(0x12c8f98))=previous;
}
bool DestructionHeaderControlled(void* entity) noexcept {
 const BYTE* holder=nullptr;const BYTE* body=nullptr;
 if(!Read(static_cast<BYTE*>(entity)+8,holder)||!Read(holder,body)
    ||!Accessible(body,0x60)||std::memcmp(body,"TMC",3))return false;
 for(const auto& model:StateCostumes::Data::kModels)
  if(!model.schoolgirl&&model.size==U32(body+0x10)
     &&StateCostumes::Data::Fnv64(body+0x50,16)==model.identityHash)return true;
 return false;
}

static BYTE* mixedRoots[16]{};static unsigned mixedNext=0;
static bool CostumePieceRoot(void* entity,BYTE* common,BYTE*& mixed,uint32_t& textureBase) noexcept {
 const BYTE* holder=nullptr;BYTE* body=nullptr;
 if(!Read(static_cast<BYTE*>(entity)+8,holder)||!Read(holder,body)||!Accessible(body,0x100)||std::memcmp(body,"TMC",3))return false;
 const uint32_t count=U32(body+0x14),table=U32(body+0x20);
 if(count<2||count>64||!Accessible(body+table,count*4))return false;
 BYTE* geo=body+U32(body+table);BYTE* ttdm=body+U32(body+table+4);
 if(!Accessible(geo,0x30)||std::memcmp(geo,"MdlGeo",6)||!Accessible(ttdm,0x60)||std::memcmp(ttdm,"TTDM",4))return false;
 const uint32_t objects=U32(geo+0x14),objectTable=U32(geo+0x20);
 if(objects>256||!Accessible(geo+objectTable,objects*4))return false;
 bool found=false;
 for(uint32_t i=0;i<objects&&!found;++i){
  BYTE* object=geo+U32(geo+objectTable+i*4);
  if(!Accessible(object,0x60)||std::memcmp(object,"ObjGeo",6)
     ||(std::memcmp(object+0x50,"WGT_eff",8)&&std::memcmp(object+0x50,"WGT_eff1",9)))continue;
  const uint32_t subs=U32(object+0x14),subTable=U32(object+0x20);
  if(subs!=8||!Accessible(object+subTable,32))return false;

  uint32_t minimum=0xffffffffu,mask=0;uint32_t values[8]{};
  for(uint32_t k=0;k<8;++k){
   BYTE* sub=object+U32(object+subTable+4*k);
   if(!Accessible(sub,0x20)||U32(sub+0xc)<1)return false;
   BYTE* slot=sub+U32(sub+0x10);
   if(!Accessible(slot,0x10))return false;
   values[k]=U32(slot+8);if(values[k]<minimum)minimum=values[k];
  }
  for(uint32_t k=0;k<8;++k){if(values[k]-minimum>=8)return false;mask|=1u<<(values[k]-minimum);}
  if(mask!=255)return false;
  textureBase=minimum;found=true;
 }
 if(!found||textureBase+8>U32(ttdm+0x44))return false;
 const uint32_t commonCount=U32(common+0x14),commonTable=U32(common+0x20);
 if(commonCount<2||commonCount>64||!Accessible(common+commonTable,commonCount*4))return false;
 uint32_t header=commonTable+commonCount*4;
 for(uint32_t i=0;i<commonCount;++i){const uint32_t o=U32(common+commonTable+i*4);if(o&&o<header)return false;}
 header=(header+15)&~15u;if(header>0x400||!Accessible(common,header))return false;
 BYTE*& slotRoot=mixedRoots[mixedNext];mixedNext=(mixedNext+1)%16;
 if(!slotRoot)slotRoot=static_cast<BYTE*>(VirtualAlloc(nullptr,0x400,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
 if(!slotRoot)return false;
 std::memcpy(slotRoot,common,header);
 for(uint32_t i=0;i<commonCount;++i){
  const uint32_t o=U32(common+commonTable+i*4);if(!o)continue;
  Put32(slotRoot+commonTable+i*4,static_cast<uint32_t>(reinterpret_cast<uintptr_t>(common+o)-reinterpret_cast<uintptr_t>(slotRoot)));
 }
 Put32(slotRoot+commonTable+4,static_cast<uint32_t>(reinterpret_cast<uintptr_t>(ttdm)-reinterpret_cast<uintptr_t>(slotRoot)));
 mixed=slotRoot;return true;
}
bool TryOriginalDestruction(const EntityInfo& actor,uint32_t tick,BYTE* root) noexcept {
 DestructionPerfScope perf("create",0);
 if(!originalSchoolgirlEnabled||!DestructionHeaderControlled(actor.entity))return false;
 const BYTE guard855780[]={0x55,0x8b,0xec,0x83,0xec,0x8};
 if(!Signature(0x855780,guard855780,sizeof guard855780))return false;
 const BYTE guard8557b0[]={0x55,0x8b,0xec,0x8b,0x45,0x8};
 if(!Signature(0x8557b0,guard8557b0,sizeof guard8557b0))return false;
 const BYTE guard53db00[]={0x55,0x8b,0xec,0x6a,0xff,0x68};
 if(!Signature(0x53db00,guard53db00,sizeof guard53db00))return false;
 const BYTE guardbbda60[]={0x55,0x8b,0xec,0x83,0xec,0x8};
 if(!Signature(0xbbda60,guardbbda60,sizeof guardbbda60))return false;
 const BYTE guard8453d0[]={0x55,0x8b,0xec,0x8b,0x45,0x10};
 if(!Signature(0x8453d0,guard8453d0,sizeof guard8453d0))return false;
 const BYTE guardbb97f0[]={0xd9,0xee,0xd9,0x51,0x58,0xd9};
 if(!Signature(0xbb97f0,guardbb97f0,sizeof guardbb97f0))return false;
 const BYTE guard4264e0[]={0x55,0x8b,0xec,0x53,0x8b,0x5d};
 if(!Signature(0x4264e0,guard4264e0,sizeof guard4264e0))return false;
 const BYTE callbackPrefix[]={0x55,0x8b,0xec};
 const BYTE relativeTexturePrefix[]={0x55,0x8b,0xec,0x81,0xec,0x64,0x01,0x00,0x00};

 if(!Signature(0x443020,relativeTexturePrefix,sizeof relativeTexturePrefix)
    ||!Signature(0x57edd0,callbackPrefix,sizeof callbackPrefix)
    ||!Accessible(root,0x170)||std::memcmp(root,"TMC",3)
    ||!Accessible(At(0xfdae70),sizeof originalDestructionVtable)
    ||!Accessible(At(0x10b38fc),8)
    ||U32(At(0x10b38fc))!=uint32_t(reinterpret_cast<uintptr_t>(At(0x421a40)))
    ||U32(At(0x10b3900))!=uint32_t(reinterpret_cast<uintptr_t>(At(0x5754d0))))return false;
 static const uint32_t words[]={0x0458a810,0x5159a808,0x1424a321,0x03000000};
 struct Key {const uint32_t* words;uint32_t hash;};const Key key{words,0x67a59705};
 BYTE* group=reinterpret_cast<BYTE*(__cdecl*)(void*,const Key*)>(At(0xbbbb70))(root,&key);
 if(!group||!Accessible(group,2688)||U32(group+0x10)!=28||U32(group+0x14)!=2
    ||U32(group+0x18)!=94||U32(group+0x1c)!=214)return false;
 BYTE* work=reinterpret_cast<BYTE*(__cdecl*)(void*)>(At(0xbbbb10))(group);
 if(!work||!Accessible(work,2656)||std::memcmp(work,"WorkPack",8)
    ||U32(work+12)!=2656||U32(work+24)!=2||U32(work+28)!=1
    ||U32(work+36)!=80||U32(work+80)!=96||U32(work+84)!=1376)return false;
 unsigned textureMask=0;
 for(unsigned b=0;b<2;++b){
  BYTE* bank=work+(b?1376:96);
  if(std::memcmp(bank,"BankPack",8)||U32(bank+12)!=1280
     ||U32(bank+24)!=4||U32(bank+28)!=24||U32(bank+36)!=672)return false;
  for(unsigned c=0;c<4;++c){
   BYTE* row=bank+672+c*96;
   if(U32(row)!=1||U32(row+4)>=8||U32(row+8)!=1||U32(row+12)!=1)return false;
   const unsigned bit=1u<<U32(row+4);if(textureMask&bit)return false;textureMask|=bit;
  }
 }
 if(textureMask!=255)return false;
 BYTE* node=reinterpret_cast<BYTE*(__cdecl*)(unsigned)>(At(0x855780))(ParticleResetLifetime::AllocationSize);
 if(!node)return false;
 BYTE* object=node+12;std::memset(object,0,ParticleResetLifetime::ObjectSize);
 reinterpret_cast<void(__thiscall*)(void*,void*,uint32_t,uint32_t)>(At(0x8453d0))
  (object,actor.entity,tick,0);
 ParticleResetLifetime::Stamp(object,static_cast<uint32_t>(InterlockedCompareExchange(&resetEpoch,0,0)));
 if(!originalDestructionVtableReady){
  std::memcpy(originalDestructionVtable,At(0xfdae70),sizeof originalDestructionVtable);
  originalDestructionVtable[2]=reinterpret_cast<void*>(&DestructionUpdate);
  originalDestructionVtable[3]=reinterpret_cast<void*>(&DestructionDraw);
  originalDestructionVtableReady=true;
 }
 *reinterpret_cast<void***>(object)=originalDestructionVtable+1;
 *reinterpret_cast<BYTE**>(object+0x18)=At(0xfd8618);
 *reinterpret_cast<BYTE**>(object+0x20)=At(0xfd8618);
 SchoolgirlBasis(object,actor);
 const float anchorCounter=5.0f;std::memcpy(object+0xc0,&anchorCounter,4);
 struct Descriptor {float offset[4];uint32_t modelBase,textureBase;BYTE* root;BYTE* lists;};
 static_assert(sizeof(Descriptor)==32,"Native root-relative texture descriptor ABI");

 Descriptor descriptor{{0,0,0,0},94,214,root,object+0x18};
 const auto tuning=RefreshEffectTuning();
 if(tuning.costumePieces){
  BYTE* mixed=nullptr;uint32_t base=0;
  if(CostumePieceRoot(actor.entity,root,mixed,base)){
   descriptor.root=mixed;descriptor.textureBase=base;
   char line[128];std::snprintf(line,sizeof line,"Costume pieces: WGT_eff textures %u..%u of victim TMC (PS4 CBDF20).",base,base+7);Queue(line);
  }else Queue("Costume pieces: victim WGT_eff not usable, common white pieces kept.");
 }
 struct Row {BYTE* values;BYTE* bank;};
 using Callback=void(__cdecl*)(const Descriptor*,const Row*,const uint32_t*);
 for(unsigned b=0;b<2;++b){
  BYTE* bank=work+(b?1376:96);uint32_t properties[72]={};
  reinterpret_cast<void(__thiscall*)(void*,BYTE**)>(At(0xbbda60))(properties,&bank);
  for(unsigned c=0;c<4;++c){
   Row row{bank+672+c*96,bank};
   reinterpret_cast<Callback>(At(0x443020))(&descriptor,&row,properties);
  }
 }
 if(!U32(object+0x1c)&&!U32(object+0x24)){
  reinterpret_cast<void(__thiscall*)(void*,unsigned)>(At(0x53db00))(object,0);
  reinterpret_cast<void(__cdecl*)(void*)>(At(0x8557b0))(node);return false;
 }
 *reinterpret_cast<uint32_t*>(object+4)=1;
 BYTE* manager=At(0x246c510);
 *reinterpret_cast<BYTE**>(node+8)=object;*reinterpret_cast<uint32_t*>(node+4)=0;
 *reinterpret_cast<BYTE**>(node)=*reinterpret_cast<BYTE**>(manager+4);
 *reinterpret_cast<BYTE**>(manager+4)=node;
 Queue("OFFLINE original destruction candidate queued: eight root-relative texture channels via443020; runtime fidelity unvalidated.");
 DestructionDiagnostic(object,"queued");
 return true;
}
