#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <tlhelp32.h>
#include <d3d9.h>
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <cmath>
#include <vector>
#include <unordered_map>
#include <string>
#include <regex>
#include "SkinSSSData.h"
#include "../commun/journal_extra.h"
static_assert(sizeof(void*)==4,"x86 required");
static BYTE*Base;
static char Ini[MAX_PATH];
static void Log(const char*fmt,...){va_list v;va_start(v,fmt);JournalExtra_V("SkinSSS",fmt,v);va_end(v);}
static bool Readable(const void*p,size_t n){uintptr_t a=(uintptr_t)p,e=a+n;if(!a||e<a)return false;while(a<e){MEMORY_BASIC_INFORMATION m{};if(!VirtualQuery((void*)a,&m,sizeof m)||m.State!=MEM_COMMIT||(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;uintptr_t next=(uintptr_t)m.BaseAddress+m.RegionSize;if(next<=a)return false;a=next;}return true;}
template<class T>T& At(void*p,size_t o){return *(T*)((BYTE*)p+o);}
static BYTE* VA(DWORD a){return Base+a-0x400000;}

struct SkinSettings{bool enabled=true;float width=1.0f;int taps=20;float material=0.25f;float tolerance=0.02f;int toggleKey=0;bool fade=true;bool debug=false;
 int diag=0;bool sweat=true;float sweatGain=1.0f,sweatAdd=0.0f,sweatMax=1.0f,sweatOverride=-1.0f;bool sweatProbe=true;};
static SkinSettings S;
static volatile LONG Active=1;
static float IniFloat(const char*sec,const char*key,const char*def){char b[64];GetPrivateProfileStringA(sec,key,def,b,sizeof b,Ini);return (float)atof(b);}
static void LoadSettings(bool quiet=false){
 SkinSettings old=S;
 S.enabled=GetPrivateProfileIntA("SkinSSS","Enabled",1,Ini)!=0;
 S.width=IniFloat("SkinSSS","Width","1.0");
 S.taps=GetPrivateProfileIntA("SkinSSS","Taps",20,Ini);if(S.taps<2)S.taps=2;if(S.taps>20)S.taps=20;
 S.tolerance=IniFloat("SkinSSS","Tolerance","0.02");
 S.toggleKey=GetPrivateProfileIntA("SkinSSS","ToggleKey",0,Ini);
 S.fade=GetPrivateProfileIntA("SkinSSS","CloseUpFade",1,Ini)!=0;S.debug=GetPrivateProfileIntA("SkinSSS","Debug",0,Ini)!=0;
 S.diag=GetPrivateProfileIntA("SkinSSSDiag","Mode",0,Ini);
 S.sweat=GetPrivateProfileIntA("SkinSSSSweat","Enabled",1,Ini)!=0;
 S.sweatGain=IniFloat("SkinSSSSweat","Gain","1.0");S.sweatAdd=IniFloat("SkinSSSSweat","Add","0.0");S.sweatMax=IniFloat("SkinSSSSweat","Max","1.0");
 S.sweatOverride=IniFloat("SkinSSSSweat","Override","-1");S.sweatProbe=GetPrivateProfileIntA("SkinSSSSweat","Probe",0,Ini)!=0;
 if(quiet&&!memcmp(&old,&S,sizeof S))return;
 Log("Reglages : Diag=%d Enabled=%u Width=%g Taps=%d Tolerance=%g ToggleKey=0x%x CloseUpFade=%u Debug=%u | Sweat Enabled=%u Gain=%g Add=%g Max=%g Override=%g Probe=%u",
  S.diag,S.enabled,S.width,S.taps,S.tolerance,S.toggleKey,S.fade,S.debug,S.sweat,S.sweatGain,S.sweatAdd,S.sweatMax,S.sweatOverride,S.sweatProbe);
}

static float Kernel[20][4];
static void BuildKernel(int n){
 const float falloff[3]={1.001f,0.371f,0.301f},strength[3]={0.47f,0.41f,0.28f};
 const float var[5]={0.0484f,0.187f,0.567f,1.99f,7.41f},wt[5]={0.100f,0.118f,0.113f,0.358f,0.078f};
 float k[20][4]={};float range=n>20?3.0f:2.0f,step=2*range/(n-1);
 for(int i=0;i<n;i++){float o=-range+i*step;k[i][3]=range*(o<0?-1.0f:1.0f)*o*o/(range*range);}
 for(int i=0;i<n;i++){
  float area=0;if(i>0)area+=fabsf(k[i][3]-k[i-1][3]);if(i<n-1)area+=fabsf(k[i][3]-k[i+1][3]);
  for(int c=0;c<3;c++){float r=k[i][3]/falloff[c],p=0;for(int g=0;g<5;g++)p+=wt[g]*expf(-r*r/(2*var[g]))/(2*3.14159265f*var[g]);k[i][c]=area*0.5f*p;}
 }
 int c=n/2;float center[4];memcpy(center,k[c],16);for(int i=c;i>0;i--)memcpy(k[i],k[i-1],16);memcpy(k[0],center,16);
 for(int ch=0;ch<3;ch++){float s=0;for(int i=0;i<n;i++)s+=k[i][ch];for(int i=0;i<n;i++)k[i][ch]/=s;
  k[0][ch]=k[0][ch]*strength[ch]+(1-strength[ch]);for(int i=1;i<n;i++)k[i][ch]*=strength[ch];}
 memset(Kernel,0,sizeof Kernel);memcpy(Kernel,k,sizeof(float)*4*n);
}

using SetPSFn=HRESULT(WINAPI*)(IDirect3DDevice9*,IDirect3DPixelShader9*);
using SetRTFn=HRESULT(WINAPI*)(IDirect3DDevice9*,DWORD,IDirect3DSurface9*);
using ResetFn=HRESULT(WINAPI*)(IDirect3DDevice9*,D3DPRESENT_PARAMETERS*);
using PresentFn=HRESULT(WINAPI*)(IDirect3DDevice9*,const RECT*,const RECT*,HWND,const RGNDATA*);
using StretchFn=HRESULT(WINAPI*)(IDirect3DDevice9*,IDirect3DSurface9*,const RECT*,IDirect3DSurface9*,const RECT*,D3DTEXTUREFILTERTYPE);
using SetDSFn=HRESULT(WINAPI*)(IDirect3DDevice9*,IDirect3DSurface9*);
using SetTexFn=HRESULT(WINAPI*)(IDirect3DDevice9*,DWORD,IDirect3DBaseTexture9*);
static StretchFn StretchOriginal;static SetDSFn SetDSOriginal;static SetTexFn SetTexOriginal;
using SetPSConstFn=HRESULT(WINAPI*)(IDirect3DDevice9*,UINT,const float*,UINT);
static SetPSFn SetPSOriginal;static SetRTFn SetRTOriginal;static ResetFn ResetOriginal;static PresentFn PresentOriginal;static SetPSConstFn SetPSConstOriginal;
static IDirect3DDevice9*Device;static void**Vtable;

struct Target{IDirect3DTexture9*tex=nullptr;IDirect3DSurface9*surf=nullptr;void Release(){if(surf)surf->Release();if(tex)tex->Release();surf=nullptr;tex=nullptr;}};
static Target Diffuse,Albedo,Spec,Temp,SceneCopy;
static UINT Width,Height;static D3DFORMAT SceneFormat=D3DFMT_UNKNOWN;
static IDirect3DPixelShader9*Variants[6]{};static IDirect3DPixelShader9*BlurShader;static IDirect3DVertexShader9*QuadShader;
static IDirect3DStateBlock9*Saved;
static std::unordered_map<IDirect3DPixelShader9*,int> Known;
static IDirect3DSurface9*CurrentRT0;static bool RT0Checked,RT0Fits;
static bool Bound,Drawn,GameMRT,Broken;static unsigned Binds,Passes;
static bool SceneKnown;static D3DSURFACE_DESC SceneDesc;

static int TraceState;static unsigned TraceCount;
static void Trace(const char*fmt,...){if(TraceState!=2||TraceCount>3000)return;TraceCount++;char b[256];va_list v;va_start(v,fmt);vsnprintf(b,sizeof b,fmt,v);va_end(v);Log("TRACE %s",b);}
static void TraceSurface(const char*what,DWORD i,IDirect3DSurface9*s){if(TraceState!=2)return;D3DSURFACE_DESC d{};if(s)s->GetDesc(&d);Trace("%s %lu -> %p %ux%u fmt %d",what,i,s,d.Width,d.Height,(int)d.Format);}

static void ReleaseResources(){
 for(auto*t:{&Diffuse,&Albedo,&Spec,&Temp,&SceneCopy})t->Release();
 for(auto&v:Variants)if(v){v->Release();v=nullptr;}
 if(BlurShader){BlurShader->Release();BlurShader=nullptr;}if(QuadShader){QuadShader->Release();QuadShader=nullptr;}
 if(Saved){Saved->Release();Saved=nullptr;}
 Width=Height=0;SceneFormat=D3DFMT_UNKNOWN;Bound=Drawn=false;RT0Checked=false;CurrentRT0=nullptr;SceneKnown=false;
}
static bool MakeTarget(Target&t,UINT w,UINT h,D3DFORMAT f){
 if(FAILED(Device->CreateTexture(w,h,1,D3DUSAGE_RENDERTARGET,f,D3DPOOL_DEFAULT,&t.tex,nullptr))||FAILED(t.tex->GetSurfaceLevel(0,&t.surf))){t.Release();return false;}
 return true;
}

static bool EnsureResources(const D3DSURFACE_DESC&d){
 if(Broken)return false;
 if(Width==d.Width&&Height==d.Height&&SceneFormat==d.Format&&Diffuse.surf)return true;
 for(auto*t:{&Diffuse,&Albedo,&Spec,&Temp,&SceneCopy})t->Release();
 D3DCAPS9 caps{};Device->GetDeviceCaps(&caps);
 if(caps.NumSimultaneousRTs<4){Log("Carte : %lu cibles simultanees (4 requises) ; SSS inactif",caps.NumSimultaneousRTs);Broken=true;return false;}
 bool mixed=(caps.PrimitiveMiscCaps&D3DPMISCCAPS_MRTINDEPENDENTBITDEPTHS)!=0;
 D3DFORMAT f=mixed?D3DFMT_A16B16G16R16F:d.Format;
 if(!MakeTarget(Diffuse,d.Width,d.Height,f)||!MakeTarget(Albedo,d.Width,d.Height,f)||!MakeTarget(Spec,d.Width,d.Height,f)||!MakeTarget(Temp,d.Width,d.Height,f)||!MakeTarget(SceneCopy,d.Width,d.Height,d.Format)){
  Log("Creation des cibles %ux%u format %d impossible ; SSS inactif",d.Width,d.Height,(int)f);for(auto*t:{&Diffuse,&Albedo,&Spec,&Temp,&SceneCopy})t->Release();Broken=true;return false;}
 if(!BlurShader&&FAILED(Device->CreatePixelShader(BlurPS,&BlurShader))){Log("Shader de flou refuse");Broken=true;return false;}
 if(!QuadShader&&FAILED(Device->CreateVertexShader(QuadVS,&QuadShader))){Log("Shader de quad refuse");Broken=true;return false;}
 if(!Saved&&FAILED(Device->CreateStateBlock(D3DSBT_ALL,&Saved))){Log("Bloc d'etat impossible");Broken=true;return false;}
 Width=d.Width;Height=d.Height;SceneFormat=d.Format;
 Log("Cibles SSS %ux%u : scene format %d, cibles format %d (%s)",Width,Height,(int)d.Format,(int)f,mixed?"profondeurs independantes":"meme format que la scene");
 return true;
}
static void ClearTargets(){
 for(auto*t:{&Diffuse,&Albedo,&Spec})if(t->surf)Device->ColorFill(t->surf,nullptr,0);
 Drawn=false;
}
static void Unbind(IDirect3DDevice9*d){
 if(!Bound)return;for(DWORD i=1;i<4;i++)SetRTOriginal(d,i,nullptr);Bound=false;
}

static bool RT0Suits(IDirect3DDevice9*d){
 if(RT0Checked)return RT0Fits;
 RT0Checked=true;RT0Fits=false;IDirect3DSurface9*rt=nullptr;
 if(FAILED(d->GetRenderTarget(0,&rt))||!rt)return false;
 D3DSURFACE_DESC desc{};rt->GetDesc(&desc);rt->Release();

 bool same=SceneKnown&&desc.Width==SceneDesc.Width&&desc.Height==SceneDesc.Height&&desc.Format==SceneDesc.Format&&desc.MultiSampleType==D3DMULTISAMPLE_NONE;
 if(!same){static unsigned refusals;if(++refusals<=12)Log("Peau dessinee hors cible de scene : RT0 %ux%u format %d msaa %d (scene %s %ux%u format %d)",desc.Width,desc.Height,(int)desc.Format,(int)desc.MultiSampleType,SceneKnown?"connue":"pas encore vue",SceneDesc.Width,SceneDesc.Height,(int)SceneDesc.Format);return false;}
 RT0Fits=EnsureResources(desc);return RT0Fits;
}

static std::vector<BYTE> Originaux[6],Codes[6];
static bool Tampon(void*b,std::string*texte,std::vector<BYTE>*octets){
 if(!b)return false;void**vt=*(void***)b;
 const void*p=((const void*(WINAPI*)(void*))vt[3])(b);const DWORD n=((DWORD(WINAPI*)(void*))vt[4])(b);
 if(p&&texte)texte->assign((const char*)p,strnlen((const char*)p,n));if(p&&octets)octets->assign((const BYTE*)p,(const BYTE*)p+n);
 ((ULONG(WINAPI*)(void*))vt[2])(b);return p!=nullptr;
}
static bool Variante(int k){
 if(!Codes[k].empty())return true;
 HMODULE x=GetModuleHandleA("d3dx9_43.dll");
 using DisFn=HRESULT(WINAPI*)(const DWORD*,BOOL,LPCSTR,void**);using AsmFn=HRESULT(WINAPI*)(LPCSTR,UINT,const void*,void*,DWORD,void**,void**);
 DisFn dis=x?(DisFn)GetProcAddress(x,"D3DXDisassembleShader"):nullptr;AsmFn as=x?(AsmFn)GetProcAddress(x,"D3DXAssembleShader"):nullptr;
 if(!dis||!as||Originaux[k].empty()){Log("Variante %s : d3dx9_43 ou shader d'origine absent",SkinShaders[k].tag);return false;}
 void*buf=nullptr;std::string texte;
 if(FAILED(dis((const DWORD*)Originaux[k].data(),FALSE,nullptr,&buf))||!Tampon(buf,&texte,nullptr)){Log("Variante %s : desassemblage refuse",SkinShaders[k].tag);return false;}
 std::vector<std::string>lignes;size_t a=0;
 while(a<texte.size()){size_t e=texte.find('\n',a);if(e==std::string::npos)e=texte.size();std::string l=texte.substr(a,e-a);a=e+1;
  size_t d=l.find_first_not_of(" \t\r");size_t f=l.find_last_not_of(" \t\r");if(d==std::string::npos)continue;l=l.substr(d,f-d+1);
  if(l.compare(0,2,"//"))lignes.push_back(l);}
 static const std::regex registre("\\b([rc])(\\d+)");
 for(const auto&l:lignes){std::smatch m;std::string r=l;
  for(std::sregex_iterator i(r.begin(),r.end(),registre),e;i!=e;++i){int n=atoi((*i)[2].str().c_str());
   if(((*i)[1]=="r"&&n>=27&&n<=30)||((*i)[1]=="c"&&(n==222||n==223))){Log("Variante %s : registres reserves deja utilises",SkinShaders[k].tag);return false;}}}
 static const std::regex fin("mad oC0\\.xyz, (r\\d+(?:\\.\\w+)?), (r\\d+(?:\\.\\w+)?), (r\\d+(?:\\.\\w+)?)"),
  melange("mad (r\\d+)\\.xyz, (r\\d+(?:\\.\\w+)?), (r\\d+(?:\\.\\w+)?), (r\\d+)"),prof("dp4 (r\\d+)\\.(\\w), c37, .*");
 std::vector<std::string>sortie;bool depth=false,mad=false,final=false;
 for(size_t i=0;i<lignes.size();i++){const std::string&l=lignes[i];std::smatch m;
  if(std::regex_match(l,m,fin)){const std::string mul=m[2],add=m[3];
   for(const std::string&t:{std::string("max r27.xyz, r27, c223.x"),std::string("mov r27.w, c222.x"),std::string("mov oC1, r27"),
     "mul r28.xyz, r28, "+mul,std::string("mov r28.w, r30.w"),std::string("mov oC2, r28"),"mad r29.xyz, r29, "+mul+", "+add,
     std::string("mov r29.w, c223.y"),std::string("mov oC3, r29")})sortie.push_back(t);
   final=true;}
  if(std::regex_match(l,m,melange)&&m[1]==m[4]&&i+1<lignes.size()&&std::regex_match(lignes[i+1],std::regex("max r\\d+\\.xyz, "+m[1].str()+", c\\d+\\.\\w"))){
   sortie.push_back("mov r27.xyz, "+m[2].str());sortie.push_back("mov r28.xyz, "+m[3].str());sortie.push_back("mov r29.xyz, "+m[1].str());mad=true;}
  sortie.push_back(l);
  if(std::regex_match(l,m,prof)){sortie.push_back("mov r30.w, "+m[1].str()+"."+m[2].str());depth=true;}
  if(l=="ps_3_0")sortie.push_back("def c223, 0, 1, 0, 0");
 }
 if(!depth||!mad||!final){Log("Variante %s : motifs absents (profondeur %d melange %d sortie %d)",SkinShaders[k].tag,depth,mad,final);return false;}
 std::string src;for(const auto&l:sortie){src+=l;src+='\n';}
 void*code=nullptr,*err=nullptr;std::vector<BYTE>octets;
 const HRESULT hr=as(src.c_str(),(UINT)src.size(),nullptr,nullptr,0,&code,&err);if(err)Tampon(err,nullptr,nullptr);
 if(FAILED(hr)||!Tampon(code,nullptr,&octets)){Log("Variante %s : assemblage refuse",SkinShaders[k].tag);return false;}
 unsigned long long h=0xcbf29ce484222325ULL;for(BYTE b:octets)h=(h^b)*0x100000001b3ULL;
 if(octets.size()!=SkinShaders[k].variantSize||h!=SkinShaders[k].variantFnv){Log("Variante %s : resultat different de la reference (%u octets) ; SSS inactif",SkinShaders[k].tag,(unsigned)octets.size());return false;}
 Codes[k]=octets;Log("Variante %s fabriquee (%u octets, identique a la reference)",SkinShaders[k].tag,(unsigned)octets.size());return true;
}
static int Recognize(IDirect3DPixelShader9*p){
 auto it=Known.find(p);if(it!=Known.end())return it->second;
 int found=-1;UINT n=0;
 if(SUCCEEDED(p->GetFunction(nullptr,&n))){
  for(int k=0;k<6;k++)if(n==SkinShaders[k].size){
   std::vector<BYTE>b(n);UINT m=n;
   if(SUCCEEDED(p->GetFunction(b.data(),&m))&&m==n){unsigned long long h=0xcbf29ce484222325ULL;for(BYTE x:b)h=(h^x)*0x100000001b3ULL;
    for(int j=0;j<6;j++)if(SkinShaders[j].size==n&&SkinShaders[j].fnv==h)found=j;
    if(found>=0&&Originaux[found].empty())Originaux[found]=b;}
   break;}
 }
 Known[p]=found;
 if(found>=0)Log("Shader peau du jeu reconnu : %s (%p)",SkinShaders[found].tag,p);
 return found;
}
static int CurrentSkin=-1;
static HRESULT WINAPI SetPS(IDirect3DDevice9*d,IDirect3DPixelShader9*p){
 CurrentSkin=p?Recognize(p):-1;
 int k=S.enabled&&Active&&!Broken&&!GameMRT&&S.diag!=3?CurrentSkin:-1;
 if(k>=0&&RT0Suits(d)){
  if(!Variants[k]&&(!Variante(k)||FAILED(d->CreatePixelShader((const DWORD*)Codes[k].data(),&Variants[k])))){Log("Variante %s refusee ; SSS inactif",SkinShaders[k].tag);Broken=true;Unbind(d);return SetPSOriginal(d,p);}
  HRESULT hr=SetPSOriginal(d,Variants[k]);
  if(!Bound){SetRTOriginal(d,1,Diffuse.surf);SetRTOriginal(d,2,Albedo.surf);SetRTOriginal(d,3,Spec.surf);Bound=true;}
  float id[4]={S.material,0,0,0};d->SetPixelShaderConstantF(222,id,1);
  Trace("peau %s",SkinShaders[k].tag);
  Drawn=true;if(++Binds==1)Log("Premier dessin de peau avec cibles SSS (%s)",SkinShaders[k].tag);
  return hr;
 }
 Unbind(d);return SetPSOriginal(d,p);
}
static bool InPasses;
static void RunPasses();
static HRESULT WINAPI SetRT(IDirect3DDevice9*d,DWORD i,IDirect3DSurface9*s){
 TraceSurface("SetRenderTarget",i,s);

 if(i==0&&Drawn&&!InPasses){Trace("passes SSS");if(S.diag==1){Unbind(d);ClearTargets();}else RunPasses();}
 if(i==0){Unbind(d);CurrentRT0=s;RT0Checked=false;}
 else{Unbind(d);GameMRT=s!=nullptr;}
 return SetRTOriginal(d,i,s);
}

struct SweatStats{unsigned sets=0,changed=0,draws=0;float lo=2,hi=-1;};
static SweatStats Sweat;
static bool IsSweatParam(const float*v){
 return fabsf(v[2]-0.25f)<1e-4f&&fabsf(v[3]-0.75f)<1e-4f&&fabsf(v[0]+v[1]-1.0f)<1e-3f&&v[0]>=-1e-4f&&v[0]<=1.0001f;
}
static float SweatValue(float s){
 float v=S.sweatOverride>=0?S.sweatOverride:s*S.sweatGain+S.sweatAdd;
 if(S.sweatOverride<0&&v>S.sweatMax)v=S.sweatMax;
 return v<0?0:v>1?1:v;
}
static HRESULT WINAPI SetPSConst(IDirect3DDevice9*d,UINT start,const float*data,UINT count){
 if(!data||start>62||62>=start+count)return SetPSConstOriginal(d,start,data,count);
 const float*v=data+4*(62-start);
 if(!IsSweatParam(v))return SetPSConstOriginal(d,start,data,count);
 Sweat.sets++;if(v[0]<Sweat.lo)Sweat.lo=v[0];if(v[0]>Sweat.hi)Sweat.hi=v[0];
 float s=S.sweat?SweatValue(v[0]):v[0];
 if(s==v[0]||count>64)return SetPSConstOriginal(d,start,data,count);
 Sweat.changed++;
 float c[64*4];memcpy(c,data,16*count);float*w=c+4*(62-start);w[0]=s;w[1]=1-s;
 return SetPSConstOriginal(d,start,c,count);
}

using DIPFn=HRESULT(WINAPI*)(IDirect3DDevice9*,D3DPRIMITIVETYPE,INT,UINT,UINT,UINT,UINT);
static DIPFn DIPOriginal;
static unsigned TexProbes;
static void ProbeTextures(IDirect3DDevice9*d){
 static unsigned long long seen[48];static unsigned nseen;
 for(DWORD st=1;st<=3;st++){
  IDirect3DBaseTexture9*t=nullptr;if(FAILED(d->GetTexture(st,&t))||!t){if(t)t->Release();continue;}
  D3DRESOURCETYPE ty=t->GetType();UINT w=0,h=0,dp=1;D3DFORMAT f=D3DFMT_UNKNOWN;
  if(ty==D3DRTYPE_TEXTURE){D3DSURFACE_DESC s{};((IDirect3DTexture9*)t)->GetLevelDesc(0,&s);w=s.Width;h=s.Height;f=s.Format;}
  else if(ty==D3DRTYPE_VOLUMETEXTURE){D3DVOLUME_DESC s{};((IDirect3DVolumeTexture9*)t)->GetLevelDesc(0,&s);w=s.Width;h=s.Height;dp=s.Depth;f=s.Format;}
  else if(ty==D3DRTYPE_CUBETEXTURE){D3DSURFACE_DESC s{};((IDirect3DCubeTexture9*)t)->GetLevelDesc(0,&s);w=s.Width;h=s.Height;f=s.Format;}
  t->Release();
  unsigned long long key=((unsigned long long)st<<60)|((unsigned long long)ty<<56)|((unsigned long long)(w&0xffff)<<40)|((unsigned long long)(h&0xffff)<<24)|((dp&0xff)<<16)|((unsigned)f&0xffff);
  bool known=false;for(unsigned i=0;i<nseen;i++)known|=seen[i]==key;
  if(!known&&nseen<48){seen[nseen++]=key;Log("Sueur : texture de peau etage %lu type %d %ux%ux%u format %d (%s)",st,(int)ty,w,h,dp,(int)f,
   st==1?"carte de sueur":st==2?"volume normales peau/gouttes":"salete");}
 }
}
static HRESULT WINAPI DIP(IDirect3DDevice9*d,D3DPRIMITIVETYPE t,INT base,UINT mn,UINT nv,UINT si,UINT pc){
 Trace("DIP %u tri ps=%d",pc,CurrentSkin);
 if(CurrentSkin>=0){Sweat.draws++;if(S.sweatProbe&&TexProbes<4000){TexProbes++;ProbeTextures(d);}}
 return DIPOriginal(d,t,base,mn,nv,si,pc);
}

static void SweatReport(){
 static float lastHi=-2;static int lastOpt=-9,lastFlag=-9;static unsigned quiet;static bool lastSent;
 int opt=At<int>(VA(0x1374268),0),flag=At<BYTE>(VA(0x13d2080),0);
 char who[160]="";size_t n=0;
 BYTE*m=At<BYTE*>(VA(0x24720d4),0);
 for(int i=0;i<8&&m&&Readable(m,0x64);i++,m=At<BYTE*>(m,0x10))
  n+=snprintf(who+n,sizeof who-n,"%sJ%u/%u %.3f",n?" ":"",At<BYTE>(m,0x36)+1,At<BYTE>(m,0x34),At<float>(m,0x60));
 bool sent=Sweat.sets>0;
 bool log=opt!=lastOpt||flag!=lastFlag||sent!=lastSent||(sent&&fabsf(Sweat.hi-lastHi)>=0.01f)||(sent&&++quiet>=15);
 if(log&&S.sweatProbe){
  if(sent)Log("Sueur : option %d drapeau %d | c62 envoye %u fois (%u dessins peau), s %.3f..%.3f, retouche %u fois -> %.3f | modeles : %s",
   opt,flag,Sweat.sets,Sweat.draws,Sweat.lo,Sweat.hi,Sweat.changed,SweatValue(Sweat.hi),n?who:"aucun");
  else Log("Sueur : option %d drapeau %d | aucun c62 de sueur envoye (%u dessins peau) | modeles : %s",opt,flag,Sweat.draws,n?who:"aucun");
  quiet=0;lastHi=Sweat.hi;
 }
 lastOpt=opt;lastFlag=flag;lastSent=sent;Sweat=SweatStats();
}

static float ProjectionScale(){

 BYTE*cam=Readable(VA(0x1198ed8),4)?At<BYTE*>(VA(0x1198ed8),0):nullptr;
 float p11=cam&&Readable(cam+0x74,4)?At<float>(cam,0x74):0;
 if(!(fabsf(p11)>0.2f&&fabsf(p11)<20))p11=2.747f;
 return 2.0f/fabsf(p11);
}
static void Draw(float w,float h){
 struct V{float x,y,z,u,v;}q[4]={{-1,1,0,0,0},{1,1,0,1,0},{-1,-1,0,0,1},{1,-1,0,1,1}};
 float off[4]={-1.0f/w,1.0f/h,0,0};Device->SetVertexShaderConstantF(0,off,1);
 Device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,q,sizeof(V));
}

extern "C" void __cdecl SkinPasses(){
 Trace("point 0x77D4D0 (peau dessinee=%u)",Drawn);
 if(!Device||Broken||!S.enabled)return;
 IDirect3DDevice9*d=Device;Unbind(d);
 IDirect3DSurface9*rt=nullptr;
 if(FAILED(d->GetRenderTarget(0,&rt))||!rt){ClearTargets();return;}
 D3DSURFACE_DESC desc{};rt->GetDesc(&desc);

 if(!SceneKnown||desc.Width!=SceneDesc.Width||desc.Height!=SceneDesc.Height||desc.Format!=SceneDesc.Format||desc.MultiSampleType!=SceneDesc.MultiSampleType){
  SceneDesc=desc;SceneKnown=true;RT0Checked=false;
  IDirect3DSurface9*bb=nullptr;D3DSURFACE_DESC back{};if(SUCCEEDED(d->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&bb))&&bb){bb->GetDesc(&back);bb->Release();}
  Log("Cible de la scene avant 0x77D4D0 : %ux%u format %d msaa %d (ecran %ux%u)%s",desc.Width,desc.Height,(int)desc.Format,(int)desc.MultiSampleType,back.Width,back.Height,
   desc.MultiSampleType!=D3DMULTISAMPLE_NONE?" : multi-echantillonnee, SSS impossible tant que c'est le cas":"");
 }
 rt->Release();
 if(Drawn)ClearTargets();
}

static const D3DRENDERSTATETYPE PassRS[]={D3DRS_ZENABLE,D3DRS_ZWRITEENABLE,D3DRS_ALPHABLENDENABLE,D3DRS_ALPHATESTENABLE,D3DRS_CULLMODE,D3DRS_STENCILENABLE,
 D3DRS_SCISSORTESTENABLE,D3DRS_SRGBWRITEENABLE,D3DRS_FOGENABLE,D3DRS_SEPARATEALPHABLENDENABLE,D3DRS_CLIPPLANEENABLE,D3DRS_COLORWRITEENABLE};
static const D3DSAMPLERSTATETYPE PassSS[]={D3DSAMP_MINFILTER,D3DSAMP_MAGFILTER,D3DSAMP_MIPFILTER,D3DSAMP_ADDRESSU,D3DSAMP_ADDRESSV,D3DSAMP_SRGBTEXTURE};
struct PassState{
 DWORD rs[sizeof PassRS/sizeof*PassRS];DWORD ss[5][sizeof PassSS/sizeof*PassSS];IDirect3DBaseTexture9*tex[5]{};
 IDirect3DVertexShader9*vs=nullptr;IDirect3DPixelShader9*ps=nullptr;IDirect3DVertexDeclaration9*decl=nullptr;DWORD fvf=0;
 IDirect3DVertexBuffer9*vb=nullptr;UINT vbOff=0,vbStride=0;float psc[24][4];float vsc[4];D3DVIEWPORT9 vp{};RECT sc{};bool consts=false;
 void Save(IDirect3DDevice9*d){
  for(unsigned i=0;i<sizeof PassRS/sizeof*PassRS;i++)d->GetRenderState(PassRS[i],&rs[i]);
  for(DWORD t=0;t<5;t++){for(unsigned i=0;i<sizeof PassSS/sizeof*PassSS;i++)d->GetSamplerState(t,PassSS[i],&ss[t][i]);d->GetTexture(t,&tex[t]);}
  d->GetVertexShader(&vs);d->GetPixelShader(&ps);d->GetVertexDeclaration(&decl);d->GetFVF(&fvf);d->GetStreamSource(0,&vb,&vbOff,&vbStride);
  consts=SUCCEEDED(d->GetPixelShaderConstantF(0,psc[0],24))&&SUCCEEDED(d->GetVertexShaderConstantF(0,vsc,1));
  d->GetViewport(&vp);d->GetScissorRect(&sc);
 }
 void Restore(IDirect3DDevice9*d){
  for(unsigned i=0;i<sizeof PassRS/sizeof*PassRS;i++)d->SetRenderState(PassRS[i],rs[i]);
  for(DWORD t=0;t<5;t++){for(unsigned i=0;i<sizeof PassSS/sizeof*PassSS;i++)d->SetSamplerState(t,PassSS[i],ss[t][i]);SetTexOriginal(d,t,tex[t]);if(tex[t])tex[t]->Release();}
  d->SetVertexShader(vs);SetPSOriginal(d,ps);if(decl)d->SetVertexDeclaration(decl);else d->SetFVF(fvf);
  d->SetStreamSource(0,vb,vbOff,vbStride);
  if(consts){SetPSConstOriginal(d,0,psc[0],24);d->SetVertexShaderConstantF(0,vsc,1);}
  d->SetViewport(&vp);d->SetScissorRect(&sc);
  if(vs)vs->Release();if(ps)ps->Release();if(decl)decl->Release();if(vb)vb->Release();
 }
};
static void RunPasses(){
 struct Guard{Guard(){InPasses=true;}~Guard(){InPasses=false;}}guard;
 if(!Device||Broken||!S.enabled||!SceneKnown)return;
 IDirect3DDevice9*d=Device;Unbind(d);
 if(!Active){ClearTargets();return;}
 IDirect3DSurface9*rt=nullptr,*ds=nullptr;
 if(FAILED(d->GetRenderTarget(0,&rt))||!rt){ClearTargets();return;}
 D3DSURFACE_DESC desc{};rt->GetDesc(&desc);
 if(desc.Width!=Width||desc.Height!=Height||desc.Format!=SceneFormat||desc.MultiSampleType!=D3DMULTISAMPLE_NONE){rt->Release();ClearTargets();return;}
 d->GetDepthStencilSurface(&ds);
 PassState st;st.Save(d);
 if(S.diag==7){Saved->Capture();Saved->Apply();}
 d->StretchRect(rt,nullptr,SceneCopy.surf,nullptr,D3DTEXF_NONE);
 d->ColorFill(Temp.surf,nullptr,0);
 if(S.diag!=6)d->SetDepthStencilSurface(nullptr);
 d->SetVertexShader(QuadShader);SetPSOriginal(d,BlurShader);d->SetFVF(D3DFVF_XYZ|D3DFVF_TEX1);
 for(auto [s,v]:{std::pair<D3DRENDERSTATETYPE,DWORD>{D3DRS_ZENABLE,FALSE},{D3DRS_ZWRITEENABLE,FALSE},{D3DRS_ALPHABLENDENABLE,FALSE},{D3DRS_ALPHATESTENABLE,FALSE},
   {D3DRS_CULLMODE,D3DCULL_NONE},{D3DRS_STENCILENABLE,FALSE},{D3DRS_SCISSORTESTENABLE,FALSE},{D3DRS_SRGBWRITEENABLE,FALSE},{D3DRS_FOGENABLE,FALSE},
   {D3DRS_SEPARATEALPHABLENDENABLE,FALSE},{D3DRS_CLIPPLANEENABLE,0}})d->SetRenderState(s,v);
 for(DWORD i=0;i<5;i++){d->SetSamplerState(i,D3DSAMP_MINFILTER,D3DTEXF_LINEAR);d->SetSamplerState(i,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR);d->SetSamplerState(i,D3DSAMP_MIPFILTER,D3DTEXF_NONE);
  d->SetSamplerState(i,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP);d->SetSamplerState(i,D3DSAMP_ADDRESSV,D3DTADDRESS_CLAMP);d->SetSamplerState(i,D3DSAMP_SRGBTEXTURE,FALSE);}
 float c[24][4]={};
 c[0][0]=0;c[0][1]=(float)S.taps;c[0][2]=S.width;c[0][3]=S.material;
 c[2][0]=S.fade?ProjectionScale():0;c[2][1]=1;c[2][2]=2;
 bool unorm=SceneFormat==D3DFMT_A8R8G8B8||SceneFormat==D3DFMT_X8R8G8B8||SceneFormat==D3DFMT_A2R10G10B10||SceneFormat==D3DFMT_A2B10G10R10;
 c[3][0]=unorm?1.0f:65504.0f;c[3][1]=S.tolerance;c[3][2]=S.debug?1.0f:0.0f;
 memcpy(c[4],Kernel,sizeof Kernel);
 d->SetTexture(1,Diffuse.tex);d->SetTexture(2,Albedo.tex);d->SetTexture(3,Spec.tex);d->SetTexture(4,SceneCopy.tex);

 c[1][0]=1;c[1][1]=0;d->SetPixelShaderConstantF(0,c[0],24);
 SetRTOriginal(d,0,Temp.surf);d->SetRenderState(D3DRS_COLORWRITEENABLE,0xF);d->SetTexture(0,Diffuse.tex);Draw((float)Width,(float)Height);

 c[1][0]=0;c[1][1]=1;d->SetPixelShaderConstantF(1,c[1],1);
 SetRTOriginal(d,0,rt);d->SetRenderState(D3DRS_COLORWRITEENABLE,D3DCOLORWRITEENABLE_RED|D3DCOLORWRITEENABLE_GREEN|D3DCOLORWRITEENABLE_BLUE);d->SetTexture(0,Temp.tex);if(S.diag!=2&&S.diag!=6)Draw((float)Width,(float)Height);
 for(DWORD i=0;i<5;i++)d->SetTexture(i,nullptr);
 SetRTOriginal(d,0,rt);d->SetDepthStencilSurface(ds);
 st.Restore(d);
 if(ds)ds->Release();rt->Release();
 ClearTargets();
 if(++Passes==1)Log("Premieres passes SSS jouees (%ux%u, echelle de vue %g)",Width,Height,c[2][0]);
}
extern "C" void* SkinCallTarget;void* SkinCallTarget;
extern "C" void SkinPassStub();
__asm__(".text\n.globl _SkinPassStub\n_SkinPassStub:\n"
 "pushal\npushfl\ncall _SkinPasses\npopfl\npopal\njmp *_SkinCallTarget\n");

static HRESULT WINAPI Present(IDirect3DDevice9*d,const RECT*a,const RECT*b,HWND w,const RGNDATA*r){
 {static DWORD last;DWORD now=GetTickCount();if(now-last>1000){last=now;int taps=S.taps;bool en=S.enabled;LoadSettings(true);S.enabled=en;if(S.taps!=taps)BuildKernel(S.taps);if(Base)SweatReport();}}
 Trace("Present");
 {char tf[MAX_PATH];strcpy(tf,Ini);char*e=strrchr(tf,'.');if(e)strcpy(e,".trace");if(TraceState==3||TraceState==0){if(GetFileAttributesA(tf)!=INVALID_FILE_ATTRIBUTES){DeleteFileA(tf);TraceState=1;TraceCount=0;Log("TRACE demandee");}}}if(TraceState==2){TraceState=3;Log("TRACE fin");}else if(TraceState==1)TraceState=2;
 HRESULT hr=PresentOriginal(d,a,b,w,r);
 if(Drawn)ClearTargets();
 static SHORT last;SHORT now=GetAsyncKeyState(S.toggleKey);HWND fg=GetForegroundWindow();DWORD pid=0;GetWindowThreadProcessId(fg,&pid);
 if((now&0x8000)&&!(last&0x8000)&&pid==GetCurrentProcessId()){InterlockedXor(&Active,1);Log("Touche : SSS %s",Active?"ACTIVE":"COUPE");}
 last=now;return hr;
}
static HRESULT WINAPI Stretch(IDirect3DDevice9*d,IDirect3DSurface9*a,const RECT*ra,IDirect3DSurface9*b,const RECT*rb,D3DTEXTUREFILTERTYPE f){
 if(TraceState==2&&!InPasses){TraceSurface("StretchRect de",0,a);TraceSurface("StretchRect vers",0,b);}return StretchOriginal(d,a,ra,b,rb,f);}
static HRESULT WINAPI SetDS(IDirect3DDevice9*d,IDirect3DSurface9*s){if(!InPasses)TraceSurface("SetDepthStencil",0,s);return SetDSOriginal(d,s);}
static HRESULT WINAPI SetTex(IDirect3DDevice9*d,DWORD st,IDirect3DBaseTexture9*t){
 if(TraceState==2&&!InPasses&&t&&t->GetType()==D3DRTYPE_TEXTURE){IDirect3DSurface9*s=nullptr;if(SUCCEEDED(((IDirect3DTexture9*)t)->GetSurfaceLevel(0,&s))&&s){D3DSURFACE_DESC dd{};s->GetDesc(&dd);if(dd.Usage&(D3DUSAGE_RENDERTARGET|D3DUSAGE_DEPTHSTENCIL))Trace("SetTexture %lu <- cible %p %ux%u fmt %d",st,s,dd.Width,dd.Height,(int)dd.Format);s->Release();}}
 return SetTexOriginal(d,st,t);}
static void Arm(void**vt);
static HRESULT WINAPI Reset(IDirect3DDevice9*d,D3DPRESENT_PARAMETERS*p){
 Unbind(d);ReleaseResources();Known.clear();Broken=false;HRESULT hr=ResetOriginal(d,p);Arm(Vtable);Log("Reset %08lx : ressources SSS recreees au besoin",(DWORD)hr);return hr;
}
static void Arm(void**vt){
 DWORD old;if(!VirtualProtect(vt+16,4*92,PAGE_READWRITE,&old))return;
 InterlockedExchangePointer(vt+107,(void*)SetPS);InterlockedExchangePointer(vt+37,(void*)SetRT);
 InterlockedExchangePointer(vt+16,(void*)Reset);
 InterlockedExchangePointer(vt+17,(void*)Present);
 DWORD unused;VirtualProtect(vt+16,4*92,old,&unused);
}

static void ArmSweat(void**vt){
 DWORD old;if(!VirtualProtect(vt+82,4*28,PAGE_READWRITE,&old))return;
 InterlockedExchangePointer(vt+109,(void*)SetPSConst);InterlockedExchangePointer(vt+82,(void*)DIP);
 DWORD o2;if(VirtualProtect(vt+34,4*32,PAGE_READWRITE,&o2)){InterlockedExchangePointer(vt+34,(void*)Stretch);InterlockedExchangePointer(vt+39,(void*)SetDS);InterlockedExchangePointer(vt+65,(void*)SetTex);DWORD u2;VirtualProtect(vt+34,4*32,o2,&u2);}
 DWORD unused;VirtualProtect(vt+82,4*28,old,&unused);
}
static bool HookDevice(IDirect3DDevice9*d){
 void**vt=*(void***)d;if(!Readable(vt,119*4))return false;
 SetPSOriginal=(SetPSFn)vt[107];SetRTOriginal=(SetRTFn)vt[37];ResetOriginal=(ResetFn)vt[16];PresentOriginal=(PresentFn)vt[17];
 SetPSConstOriginal=(SetPSConstFn)vt[109];DIPOriginal=(DIPFn)vt[82];StretchOriginal=(StretchFn)vt[34];SetDSOriginal=(SetDSFn)vt[39];SetTexOriginal=(SetTexFn)vt[65];
 Device=d;Vtable=vt;Arm(vt);ArmSweat(vt);return true;
}

static bool PatchCall(){
 BYTE*at=VA(0x76c52a);if(!Readable(at,5)||at[0]!=0xe8||at+5+At<int>(at,1)!=VA(0x77d4d0)){Log("Appel 0x76C52A inattendu (autre module ?) ; SSS inactif");return false;}
 SkinCallTarget=VA(0x77d4d0);BYTE now[5]={0xe8};At<int>(now,1)=(int)((BYTE*)SkinPassStub-(at+5));

 std::vector<HANDLE>th;HANDLE snap=CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD,0);THREADENTRY32 e{};e.dwSize=sizeof e;
 if(snap!=INVALID_HANDLE_VALUE&&Thread32First(snap,&e))do{if(e.th32OwnerProcessID==GetCurrentProcessId()&&e.th32ThreadID!=GetCurrentThreadId()){HANDLE h=OpenThread(THREAD_SUSPEND_RESUME|THREAD_GET_CONTEXT,FALSE,e.th32ThreadID);if(h){if(SuspendThread(h)!=DWORD(-1))th.push_back(h);else CloseHandle(h);}}}while(Thread32Next(snap,&e));
 if(snap!=INVALID_HANDLE_VALUE)CloseHandle(snap);
 bool busy=false;for(HANDLE h:th){CONTEXT c{};c.ContextFlags=CONTEXT_CONTROL;if(GetThreadContext(h,&c)&&c.Eip>=(DWORD)at&&c.Eip<(DWORD)at+5)busy=true;}
 bool ok=false;if(!busy){DWORD old;if(VirtualProtect(at,5,PAGE_EXECUTE_READWRITE,&old)){memcpy(at,now,5);DWORD u;VirtualProtect(at,5,old,&u);FlushInstructionCache(GetCurrentProcess(),at,5);ok=true;}}
 for(HANDLE h:th){ResumeThread(h);CloseHandle(h);}
 return ok;
}
static DWORD WINAPI Worker(void*){
 if(Extra_SepareCharge("DOA5LR-SkinSSS.asi")){Log("Refused: separate DOA5LR-SkinSSS.asi is loaded");return 0;}
 Log("SkinSSS revision 4d (peau SSS PS4 maison : 6 shaders peau a 4 sorties + flou Jimenez PS4 ; sueur du jeu reglable + sonde) starting");
 LoadSettings();BuildKernel(S.taps);
 if(!S.enabled&&!S.sweat&&!S.sweatProbe){Log("Enabled=0 et [Sweat] coupe : rien n'est installe");return 0;}
 for(unsigned i=0;i<1200;i++){
  if(Readable(VA(0x12f10f0),4)){BYTE*r=At<BYTE*>(VA(0x12f10f0),0);if(Readable(r,0xc8)){auto*d=At<IDirect3DDevice9*>(r,0xc4);if(d&&Readable(d,4)){
   Sleep(500);if(!HookDevice(d)){Log("Table du peripherique illisible");return 0;}
   if(S.enabled)for(unsigned t=0;t<10&&!PatchCall();t++)Sleep(20);
   Log("Installe : peripherique %p, crochets SetPixelShader/SetRenderTarget/Reset/Present + SetPixelShaderConstantF/DrawIndexedPrimitive (sueur), %s",d,
    S.enabled?"cible de scene reperee en 0x77D4D0, passes SSS a sa sortie":"SSS coupe (Enabled=0), sueur seule");return 0;}}}
  Sleep(100);}
 Log("Aucun peripherique D3D9 vu en 2 minutes ; inactif");return 0;
}
#ifndef SKIN_TEST
extern "C" void SkinSSS_Demarrer(void){
 Base=(BYTE*)GetModuleHandleA(nullptr);snprintf(Ini,sizeof Ini,"%s",Extra_Ini());
 HANDLE t=CreateThread(nullptr,0,Worker,nullptr,0,nullptr);if(t)CloseHandle(t);
}
#endif
