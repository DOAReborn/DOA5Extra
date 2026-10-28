#include <cmath>
#include <cstdlib>
namespace BodyBounce {
struct Settings {bool enabled=false,allModes=true;float gain=2,hipMax=.04f,thighMax=.03f;};
static Settings settings;
static constexpr const char* Names[]={"OPT_AS_L_Hip","OPT_AS_R_Hip","OPT_AS_L_UpLeg_B","OPT_AS_R_UpLeg_B"};
static float Setting(const char*name,float fallback,float low,float high){char def[32],value[64];snprintf(def,sizeof def,"%g",fallback);GetPrivateProfileStringA("BodyBounce",name,def,value,sizeof value,Ini);char*end=nullptr;float v=strtof(value,&end);return end!=value&&!*end&&std::isfinite(v)&&v>=low&&v<=high?v:fallback;}
static void Load(){settings.enabled=GetPrivateProfileIntA("BodyBounce","Enabled",0,Ini)!=0;settings.allModes=GetPrivateProfileIntA("BodyBounce","AllModes",1,Ini)!=0;settings.gain=Setting("Gain",2,0,3);settings.hipMax=Setting("HipMaxOffset",.04f,0,.06f);settings.thighMax=Setting("ThighMaxOffset",.03f,0,.06f);Log("Custom body bounce: enabled=%u allModes=%u gain=%.2f hipMax=%.3f thighMax=%.3f metres",settings.enabled,settings.allModes,settings.gain,settings.hipMax,settings.thighMax);}
struct Bone {int index=-1;bool initialized=false;float position[3]{},velocity[3]{},last[3]{};};
struct Actor {BYTE*bust=nullptr,*owner=nullptr,*tmc=nullptr;DWORD tag=0,size=0;BYTE identity[16]{};ULONGLONG seen=0;Bone bones[4];unsigned samples=0;float hipPeak=0,thighPeak=0;};
static thread_local Actor actors[16];
static bool Inside(DWORD total,DWORD offset,size_t count){return offset<=total&&count<=total-offset;}
static int FindNode(BYTE*tmc,DWORD size,const char*name){
 if(size<0x100||!Readable(tmc,size))return -1;
 DWORD sections=At<DWORD>(tmc,0x14),table=At<DWORD>(tmc,0x20);
 if(!sections||sections>64||!Inside(size,0xf0,sections*4)||!Inside(size,table,sections*4))return -1;
 for(DWORD i=0;i<sections;i++){
  if(At<DWORD>(tmc,0xf0+i*4)!=0x80000030u)continue;
  DWORD offset=At<DWORD>(tmc,table+i*4);if(!offset||!Inside(size,offset,0x24))return -1;
  BYTE*section=tmc+offset;DWORD remaining=size-offset,count=At<DWORD>(section,0x14),entries=At<DWORD>(section,0x20);
  if(!count||count>1024||!entries||!Inside(remaining,entries,count*4))return -1;
  for(DWORD j=0;j<count;j++){
   DWORD entry=At<DWORD>(section,entries+j*4);if(!entry)continue;
   for(DWORD field:{0x40u,0x48u,0x50u}){
    size_t bytes=strlen(name)+1;if(entry>remaining||!Inside(remaining-entry,field,bytes))continue;
    if(!memcmp(section+entry+field,name,bytes))return (int)j;
   }
  }return -1;
 }return -1;
}
static void Reset(Bone&b,const float*p){for(int a=0;a<3;a++){b.position[a]=b.last[a]=p[a];b.velocity[a]=0;}b.initialized=true;}

static float Step(Bone&b,const float*p,float frames,float stiffness,float damping,float limit,float gain,float*out){
 for(int a=0;a<3;a++)out[a]=0;
 if(!std::isfinite(frames)||frames<=0)return 0;
 for(int a=0;a<3;a++)if(!std::isfinite(p[a])){b.initialized=false;return 0;}
 float distance=0;for(int a=0;a<3;a++){float d=p[a]-b.last[a];distance+=d*d;}
 if(!b.initialized||distance>.25f*.25f||frames>6){Reset(b,p);return 0;}
 float h=frames/60.f,vp[3];for(int a=0;a<3;a++)vp[a]=(p[a]-b.last[a])/h;
 int sub=(int)ceilf(h/(1.f/240));if(sub<1)sub=1;float hh=h/sub;
 for(int k=0;k<sub;k++)for(int a=0;a<3;a++){float target=b.last[a]+vp[a]*hh*k;float acceleration=stiffness*(target-b.position[a])-damping*(b.velocity[a]-vp[a]);b.velocity[a]+=acceleration*hh;b.position[a]+=b.velocity[a]*hh;}
 float length=0;for(int a=0;a<3;a++){b.last[a]=p[a];out[a]=(b.position[a]-p[a])*gain;length+=out[a]*out[a];}length=sqrtf(length);
 if(!std::isfinite(length)){Reset(b,p);for(int a=0;a<3;a++)out[a]=0;return 0;}
 if(length>limit&&length>0){float ratio=limit/length;for(int a=0;a<3;a++){out[a]*=ratio;b.position[a]=p[a]+(gain>0?out[a]/gain:0);b.velocity[a]=vp[a];}length=limit;}
 return length;
}
static void Apply(BYTE*bust,float frames,float*matrices){
 if(!settings.enabled||(!settings.allModes&&!LR)||!bust||!matrices||frames<=0)return;
 if(!Readable(bust,8)||!*bust)return;BYTE*owner=At<BYTE*>(bust,4);if(!Readable(owner,0x2c))return;
 BYTE**holder=At<BYTE**>(owner,8);if(!Readable(holder,4))return;BYTE*tmc=*holder;
 if(!Readable(tmc,0x60)||memcmp(tmc,"TMC\0",4))return;DWORD size=At<DWORD>(tmc,0x10),tag=At<DWORD>(owner,4);
 if(size<0x100||size>0x20000000)return;
 Actor*slot=&actors[0];bool found=false;
 for(auto&entry:actors){if(entry.bust==bust&&entry.owner==owner&&entry.tmc==tmc&&entry.tag==tag&&entry.size==size&&!memcmp(entry.identity,tmc+0x50,16)){slot=&entry;found=true;break;}if(entry.seen<slot->seen)slot=&entry;}
 ULONGLONG now=GetTickCount64();
 if(!found){*slot=Actor{};slot->bust=bust;slot->owner=owner;slot->tmc=tmc;slot->tag=tag;slot->size=size;memcpy(slot->identity,tmc+0x50,16);for(unsigned i=0;i<4;i++)slot->bones[i].index=FindNode(tmc,size,Names[i]);Log("Body bounce model: actor=%p character=%lu hips=%d,%d thighs=%d,%d",owner,At<DWORD>(owner,0),slot->bones[0].index,slot->bones[1].index,slot->bones[2].index,slot->bones[3].index);}
 if(slot->seen&&now-slot->seen>250)for(auto&b:slot->bones)b.initialized=false;slot->seen=now;

 BYTE*flags=At<BYTE*>(owner,0x24),*end=At<BYTE*>(owner,0x28);uintptr_t count=(uintptr_t)end-(uintptr_t)flags;if(!flags||(uintptr_t)end<(uintptr_t)flags||count>4096)return;
 for(unsigned i=0;i<4;i++){auto&b=slot->bones[i];if(b.index<0||(unsigned)b.index>=count)continue;float*m=matrices+b.index*16;if(!Readable(m,64))continue;float p[3]={m[12],m[13],m[14]},offset[3];float magnitude=Step(b,p,frames,i<2?150.f:220.f,i<2?6.f:10.f,i<2?settings.hipMax:settings.thighMax,settings.gain,offset);for(int a=0;a<3;a++)m[12+a]=p[a]+offset[a];float&peak=i<2?slot->hipPeak:slot->thighPeak;if(magnitude>peak)peak=magnitude;}
 if(++slot->samples==600){Log("Body bounce applied: actor=%p LR=%u hipPeak=%.4fm thighPeak=%.4fm",owner,LR,slot->hipPeak,slot->thighPeak);slot->samples=0;slot->hipPeak=slot->thighPeak=0;}
}
}
