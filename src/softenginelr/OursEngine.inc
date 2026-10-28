#include "PS4LRDonnees.h"
#include "../statefulcostumes/StateCostumesData.h"
namespace OursLR {
namespace D2=StateCostumes::Data;
static float*Opt,*Swing,*DefA,*State;static DWORD*ModeVar;
static float PcOpt2[8],PcSwing[10][17],PcDefA[4][19],PcState[20][5],LrState[20][5];
struct Info{BYTE*obj,*owner,*tmc;int preset;float vib[14];int model,form,row;DWORD key;bool natives;float peak[4];};
static Info Infos[16];static unsigned InfoNext;static SRWLOCK InfoLock=SRWLOCK_INIT;
static thread_local unsigned Depth;
static bool Metrics;

extern "C" { float LR_LimitB=0.04f,LR_LimitC=0.04f,LR_PressLimit=0.04f,LR_PressScale=1.0f;float LR_PressVec[4];BYTE LR_Split;float*LR_Peak;
 void* LR_ClampOriginal;void* LR_PressReturn; }
static volatile LONG OptLimitC=1,OptBody=1,OptEcho=1;
static constexpr float PeakRelease=0.1f;

static bool Larger(float now,float kept){return (kept<now&&now>=0&&kept>=0)||(now<kept&&now<0&&kept<0)||(now*kept<0&&fabsf(kept)<fabsf(now));}

using ClampFn=float*(__cdecl*)(float*,float,float,float,float,float);
struct ClampArgs{float*out;float x,y,z,w,limit;};
extern "C" float* __cdecl LR_ClampB(ClampArgs*a,float g){
 auto clamp=(ClampFn)LR_ClampOriginal;
 if(!LR_Split)return clamp(a->out,a->x,a->y,a->z,a->w,a->limit);

 float k=LR_PressScale,p[4]={};memcpy(LR_PressVec,clamp(p,a->x*k,k*a->y+(k-1.0f)*g,a->z*k,a->w*k,LR_PressLimit),16);

 float v[4]={a->x,a->y,a->z,a->w};
 if(LR_Peak){
  for(int i=0;i<3;i++)if(Larger(v[i],LR_Peak[i]))LR_Peak[i]=v[i];
  if(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]<a->limit*a->limit)for(int i=0;i<3;i++){float d=LR_Peak[i]*PeakRelease;LR_Peak[i]-=d;v[i]+=d;}
 }
 return clamp(a->out,v[0],v[1],v[2],v[3],a->limit);
}
extern "C" void LR_ClampBStub();
__asm__(".text\n.globl _LR_ClampBStub\n_LR_ClampBStub:\n"
 "movl -0x60(%ebp), %eax\n"
 "leal 4(%esp), %ecx\n"
 "pushl %eax\npushl %ecx\ncall _LR_ClampB\naddl $8, %esp\nret\n");

extern "C" void LR_PressStub();
__asm__(".text\n.globl _LR_PressStub\n_LR_PressStub:\n"
 "pushl %edx\n"
 "leal -0x48(%ebp), %edx\n"
 "cmpb $0, _LR_Split\nje 1f\n"
 "movl -0x48(%ebp), %eax\norl -0x44(%ebp), %eax\norl -0x40(%ebp), %eax\norl -0x3c(%ebp), %eax\nandl $0x7fffffff, %eax\njz 1f\n"
 "leal _LR_PressVec, %edx\n"
 "1:\n"
 "movl -0x10(%ebp), %ecx\n"
 "movq (%edx), %xmm1\n"
 "movl 0x48(%ecx), %eax\n"
 "movss (%edx), %xmm2\n"
 "movss -0x104(%ebp), %xmm5\n"
 "movss -0xf4(%ebp), %xmm6\n"
 "movq %xmm1, -0x130(%ebp)\n"
 "movq 8(%edx), %xmm1\n"
 "popl %edx\n"
 "leal (%eax,%eax,4), %eax\n"
 "movq %xmm1, -0x128(%ebp)\n"
 "addl %eax, %eax\n"
 "jmp *_LR_PressReturn\n");
static void ReadOptions(bool log){
 LONG c=GetPrivateProfileIntA("SoftEngineLR","PS4LimitC",1,Ini)!=0,b=GetPrivateProfileIntA("SoftEngineLR","PS4BodyInput",1,Ini)!=0,e=GetPrivateProfileIntA("SoftEngineLR","PS4BodyEcho",1,Ini)!=0;
 bool m=GetPrivateProfileIntA("SoftEngineLR","Metrics",0,Ini)!=0;
 if(log||c!=OptLimitC||b!=OptBody||e!=OptEcho||m!=Metrics)Log("LR maison : PS4LimitC=%ld PS4BodyInput=%ld PS4BodyEcho=%ld Metrics=%u",c,b,e,m);
 OptLimitC=c;OptBody=b;OptEcho=e;Metrics=m;
}

struct Peak{BYTE*obj;unsigned frames;float peak[2][18];};
static Peak Peaks[16];static unsigned PeakNext;
static void Measure(BYTE*obj,const char*engine){
 if(!Metrics||!LR||!Readable(obj,0x30c)||!obj[0])return;
 Peak*p=nullptr;for(auto&q:Peaks)if(q.obj==obj){p=&q;break;}
 if(!p){p=&Peaks[PeakNext++%16];*p=Peak{};p->obj=obj;}
 for(int s=0;s<2;s++)for(int k=0;k<18;k++){float a=At<float>(obj,0x8c+s*0x148+k*4);if(a<0)a=-a;if(a==a&&a>p->peak[s][k])p->peak[s][k]=a;}
 if(++p->frames<300)return;
 BYTE*owner=At<BYTE*>(obj,4);DWORD ch=Readable(owner,0x40)?At<DWORD>(owner,0):255;unsigned costume=Readable(owner,0x40)?owner[0x37]:255;
 char line[1024];int n=snprintf(line,sizeof line,"LR mesure moteur=%s obj=%p personnage=%lu costume=%u",engine,obj,ch,costume);
 for(int s=0;s<2;s++){n+=snprintf(line+n,sizeof line-n," | cote%d",s);for(int k=0;k<18&&n<(int)sizeof line-16;k++)n+=snprintf(line+n,sizeof line-n," %.4f",p->peak[s][k]);}
 Log("%s",line);p->frames=0;memset(p->peak,0,sizeof p->peak);
}
static Info* Slot(BYTE*obj,bool create){
 for(auto&i:Infos)if(i.obj==obj)return &i;
 if(!create)return nullptr;
 Info*s=nullptr;for(auto&i:Infos)if(!i.obj){s=&i;break;}if(!s)s=&Infos[InfoNext++%16];
 *s=Info{};s->obj=obj;s->model=-2;s->row=-1;return s;
}
static void Capture(Info&i,BYTE*obj){
 i.preset=At<int>(obj,0x4c);for(int s=0;s<2;s++)for(int k=0;k<7;k++)i.vib[s*7+k]=At<float>(obj,0x70+s*0x148+k*4);i.natives=true;
}

static int Model(BYTE*tmc){
 if(!Readable(tmc,0x60)||memcmp(tmc,"TMC\0",4))return -1;DWORD size=At<DWORD>(tmc,0x10);
 for(size_t j=0;j<std::size(D2::kModels);j++){const auto&m=D2::kModels[j];if(m.size!=size||D2::Fnv64(tmc+0x50,16)!=m.identityHash)continue;
  bool good=Readable(tmc,size);uint64_t h=D2::kFnvBasis;for(unsigned g=0;good&&g<m.guardCount;g++){const auto&q=D2::kGuards[m.guardStart+g];good=q.offset<=size&&q.size<=size-q.offset;if(good)h=D2::Fnv64(tmc+q.offset,q.size,h);}
  return good&&h==m.guardHash?(int)j:-1;}
 return -1;
}
static int Form(Info&i,BYTE*owner,BYTE*tmc){
 if(i.model==-2||i.tmc!=tmc){i.tmc=tmc;i.model=Model(tmc);i.form=0;}
 if(i.model<0)return 0;const auto&m=D2::kModels[i.model];
 BYTE*flags=At<BYTE*>(owner,0x24),*end=At<BYTE*>(owner,0x28);
 if((uintptr_t)end<(uintptr_t)flags||(uintptr_t)end-(uintptr_t)flags<m.nodes||!Readable(flags,m.nodes))return i.form;
 bool initial=true,changed=true,disc=false;
 for(unsigned k=0;k<m.opCount;k++){const auto&o=D2::kOps[m.opStart+k];if(o.node>=m.nodes)return i.form;if(o.initial<=1&&o.transformed<=1&&o.initial!=o.transformed){disc=true;initial&=flags[o.node]==o.initial;changed&=flags[o.node]==o.transformed;}}
 if(disc&&initial!=changed)i.form=changed?1:0;
 return i.form;
}

static int Row(DWORD ch,BYTE costume,int form){
 if(ch>=64||((PS4LR::kExclude>>ch)&1)||!((PS4LR::kInclude>>ch)&1))return -1;
 for(unsigned n=0;n<PS4LR::kCount[ch];n++){const auto&r=PS4LR::kRows[PS4LR::kFirst[ch]+n];if(r.costume==costume&&r.form==form)return PS4LR::kFirst[ch]+n;}
 return -1;
}
struct Scope{
 BYTE*obj=nullptr;Info*info=nullptr;int swingRow=-1,defRow=-1;DWORD mode=0;bool entered=false;
 explicit Scope(BYTE*o){
  if(!LR)return;entered=true;if(Depth++||!Readable(o,0x30c)||!o[0])return;
  {static DWORD last;DWORD now=GetTickCount();if(now-last>1000){last=now;ReadOptions(false);}}
  BYTE*owner=At<BYTE*>(o,4);if(!Readable(owner,0x40))return;BYTE**holder=At<BYTE**>(owner,8);
  BYTE*tmc=Readable(holder,4)?*holder:nullptr;
  AcquireSRWLockExclusive(&InfoLock);
  Info*i=Slot(o,true);if(i->owner!=owner||!i->natives){i->owner=owner;Capture(*i,o);}
  DWORD ch=At<DWORD>(owner,0);BYTE costume=owner[0x37];int form=tmc?Form(*i,owner,tmc):0;
  DWORD key=ch<<16|costume<<8|form;if(key!=i->key||i->row==-1){i->key=key;int r=Row(ch,costume,form);
   const auto*p=r>=0?&PS4LR::kRows[r]:nullptr;
   Log("LR maison : obj=%p personnage=%lu costume=%u forme=%d fiche=%d vib=%u def=%u bal=%u modele=%s",o,ch,costume,form,r,p?p->vib:0,p?p->def:0,p?p->swing:0,i->model>=0?D2::kModels[i->model].name:"natif");
   i->row=r>=0?r:-3;}
  info=i;obj=o;
  const PS4LR::Row*row=i->row>=0?&PS4LR::kRows[i->row]:nullptr;
  mode=*ModeVar;*ModeVar=2;memcpy(Opt+16,PS4LR::kOptLR,32);memcpy(State,LrState,sizeof LrState);
  int eff=i->preset>=0&&i->preset<=4?i->preset+5:i->preset;At<int>(o,0x4c)=eff;
  if(eff>=0&&eff<10){swingRow=eff;memcpy(Swing+eff*17,row&&row->swing?row->swing18:PS4LR::kSwing[eff],68);}
  int h=At<int>(o,0x50);if(row&&row->def&&h>=0&&h<4){defRow=h;memcpy(DefA+h*19,row->defa,76);}
  const float*v=row&&row->vib?row->vibs:i->vib;for(int s=0;s<2;s++)memcpy(o+0x70+s*0x148,v+s*7,28);
  if(OptLimitC)LR_LimitC=PS4LR::kOptLR[9];
  if(OptBody){LR_LimitB=PS4LR::kOptLR[8];LR_PressLimit=PS4LR::kOptLR[10];LR_PressScale=PS4LR::kOptLR[11];LR_Peak=OptEcho?i->peak:nullptr;LR_Split=1;}
  ReleaseSRWLockExclusive(&InfoLock);
 }
 ~Scope(){
  if(entered)Depth--;if(!obj)return;
  At<int>(obj,0x4c)=info->preset;for(int s=0;s<2;s++)memcpy(obj+0x70+s*0x148,info->vib+s*7,28);
  if(swingRow>=0)memcpy(Swing+swingRow*17,PcSwing[swingRow],68);
  if(defRow>=0)memcpy(DefA+defRow*19,PcDefA[defRow],76);
  memcpy(Opt+16,PcOpt2,32);memcpy(State,PcState,sizeof PcState);*ModeVar=mode;
  LR_LimitB=LR_LimitC=LR_PressLimit=0.04f;LR_PressScale=1.0f;LR_Split=0;LR_Peak=nullptr;
 }
};
using ConstructFn=BYTE*(__attribute__((thiscall))*)(BYTE*,DWORD);
using ResetFn=void(__attribute__((thiscall))*)(BYTE*);
static ConstructFn ConstructOriginal;static ResetFn ResetOriginal;
static BYTE* __fastcall Construct(BYTE*self,void*,DWORD owner){
 BYTE*r=ConstructOriginal(self,owner);
 AcquireSRWLockExclusive(&InfoLock);Info*i=Slot(self,true);*i=Info{};i->obj=self;i->model=-2;i->row=-1;i->owner=(BYTE*)owner;Capture(*i,self);ReleaseSRWLockExclusive(&InfoLock);
 return r;
}
static void __fastcall Reset(BYTE*self,void*){QueryScope q;Scope s(self);ResetOriginal(self);}

static bool PrepareLimits(){
 BYTE expected[6]={0xd9,0x05};At<DWORD>(expected,2)=(DWORD)VA(0x103afa8);
 if(!Readable(VA(0x103afa8),4)||At<float>(VA(0x103afa8),0)!=0.04f){Log("LR maison : constante 0,04 inattendue");return false;}
 for(auto [va,target]:{std::pair<DWORD,float*>{0x4c4859,&LR_LimitB},{0x4c4a79,&LR_LimitC}}){
  BYTE*at=VA(va);if(!Readable(at,6)||memcmp(at,expected,6)){Log("LR maison : signature fld %lx inattendue",va);return false;}
  Change c{at,std::vector<BYTE>(expected,expected+6),std::vector<BYTE>(expected,expected+6)};At<DWORD>(c.now.data(),2)=(DWORD)target;Changes.push_back(c);}
 BYTE*call=VA(0x4c48bc);BYTE e[5]={0xe8};At<DWORD>(e,1)=(DWORD)VA(0x4c2730)-(DWORD)(call+5);
 if(!Readable(call,5)||memcmp(call,e,5)){Log("LR maison : appel 0x4C48BC inattendu");return false;}
 LR_ClampOriginal=VA(0x4c2730);
 Change c{call,std::vector<BYTE>(e,e+5),std::vector<BYTE>(e,e+5)};At<DWORD>(c.now.data(),1)=(DWORD)LR_ClampBStub-(DWORD)(call+5);Changes.push_back(c);

 static const BYTE press[]={0x8b,0x4d,0xf0,0xf3,0x0f,0x7e,0x4d,0xb8,0x8b,0x41,0x48,0xf3,0x0f,0x10,0x55,0xb8,0xf3,0x0f,0x10,0xad,0xfc,0xfe,0xff,0xff,
  0xf3,0x0f,0x10,0xb5,0x0c,0xff,0xff,0xff,0x66,0x0f,0xd6,0x8d,0xd0,0xfe,0xff,0xff,0xf3,0x0f,0x7e,0x4d,0xc0,0x8d,0x04,0x80,0x66,0x0f,0xd6,0x8d,0xd8,0xfe,0xff,0xff,0x03,0xc0};
 static_assert(sizeof press==0x4c5568-0x4c552e);
 LR_PressReturn=VA(0x4c5568);
 if(!::Prepare(VA(0x4c552e),press,sizeof press,(void*)LR_PressStub)){Log("LR maison : lecture 0x4C552E inattendue");return false;}
 return true;
}
static bool Prepare(){
 Opt=(float*)VA(0x10bd720);Swing=(float*)VA(0x10bd7a0);DefA=(float*)VA(0x10bda48);State=(float*)VA(0x10bde60);ModeVar=(DWORD*)VA(0x10bdff0);
 if(!Readable(Opt,0x8d4)){Log("LR maison : tables PC illisibles");return false;}

 if(const char* e=PS4LR::Charger(Root,VA)){Log("LR maison : %s ; mode LR inactif",e);return false;}
 memcpy(PcOpt2,Opt+16,32);memcpy(PcSwing,Swing,sizeof PcSwing);memcpy(PcDefA,DefA,sizeof PcDefA);memcpy(PcState,State,sizeof PcState);
 for(int r=0;r<20;r++){const float*s=PS4LR::kState[r];LrState[r][0]=s[0]>PS4LR::kClamp?PS4LR::kClamp:s[0];LrState[r][1]=s[1];LrState[r][2]=s[2]>PS4LR::kClamp?PS4LR::kClamp:s[2];LrState[r][3]=PS4LR::kState3;LrState[r][4]=PS4LR::kState4;}
 ReadOptions(true);
 if(!PrepareLimits())return false;
 ConstructOriginal=(ConstructFn)Hook(VA(0x4c6730),"558bec6aff",(void*)Construct);
 ResetOriginal=(ResetFn)Hook(VA(0x4c6520),"538bd9803b00",(void*)Reset);
 return ConstructOriginal&&ResetOriginal;
}
}
