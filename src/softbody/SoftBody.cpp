#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "../commun/journal_extra.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <cstdarg>
static_assert(sizeof(void*)==4,"x86 only");
#include "../commun/D5SD.h"

static BYTE* Base;
static char IniPath[MAX_PATH],LogPath[MAX_PATH];
static BYTE* VA(DWORD va){return Base+(va-0x400000);}

static uint64_t Fnv64(const BYTE* p,size_t n){uint64_t h=0xcbf29ce484222325ULL;while(n--)h=(h^*p++)*0x100000001b3ULL;return h;}
static void Log(const char* fmt,...){
    va_list a; va_start(a,fmt); JournalExtra_V("SoftBody",fmt,a); va_end(a);
}
static float IniF(const char* k,float def){
    char b[32],d[32]; snprintf(d,sizeof d,"%g",def);
    GetPrivateProfileStringA("SoftBody",k,d,b,sizeof b,IniPath); return (float)atof(b);
}
static bool Write(BYTE* p,const void* src,DWORD n){
    DWORD old; if(!VirtualProtect(p,n,PAGE_EXECUTE_READWRITE,&old)) return false;
    memcpy(p,src,n); VirtualProtect(p,n,old,&old); FlushInstructionCache(GetCurrentProcess(),p,n); return true;
}

static bool Same(BYTE* p,const char* hex){
    BYTE b[16]; size_t n=strlen(hex)/2; SIZE_T got=0;
    if(n>sizeof b||!ReadProcessMemory(GetCurrentProcess(),p,b,n,&got)||got!=n) return false;
    for(size_t i=0;i<n;i++){unsigned v; sscanf(hex+2*i,"%2x",&v); if(b[i]!=(BYTE)v) return false;}
    return true;
}

static bool NopJe(DWORD va,const char* orig,const char* what){
    BYTE* p=VA(va);
    if(!Same(p,orig)){Log("%s : octets inattendus a %08lX, rien modifie",what,va); return false;}
    static const BYTE nop6[6]={0x66,0x0F,0x1F,0x44,0x00,0x00};
    bool ok=Write(p,nop6,6); Log("%s : %s",what,ok?"active":"echec VirtualProtect"); return ok;
}

typedef void (__attribute__((thiscall)) *BustUpdateFn)(void* bust,float dt,float* mats);
static BustUpdateFn BustUpdate;
static bool Readable(const void* p,DWORD n){
    MEMORY_BASIC_INFORMATION m;
    if(!VirtualQuery(p,&m,sizeof m)||m.State!=MEM_COMMIT||(m.Protect&(PAGE_NOACCESS|PAGE_GUARD))) return false;
    return (const BYTE*)p+n<=(const BYTE*)m.BaseAddress+m.RegionSize;
}
struct JBone{char name[40]; float k,c,maxd,gain,grav;};
static JBone Cfg[8]; static int CfgN;
struct JState{int idx; bool init; float s[3],v[3],last[3];};
struct JChar{void* bust; void* tmc; DWORD seen; JState b[8];};
static JChar Chars[8];
static int DebugFrames;
static bool Ps4Soft;
static bool LogContact;
static struct{void* bust; bool was[2]; DWORD lastOn[2];} Contact[8];

static int FindNode(BYTE* tmc,const char* name){
    if(!Readable(tmc,0x100)) return -1;
    DWORD n=*(DWORD*)(tmc+0x14); if(n==0||n>64) return -1;
    for(DWORD i=0;i<n;i++){
        if(*(DWORD*)(tmc+0xF0+i*4)!=0x80000030u) continue;
        DWORD rel=*(DWORD*)(tmc+0x20); if(!rel) return -1;
        DWORD so=*(DWORD*)(tmc+rel+i*4); if(!so) return -1;
        BYTE* sec=tmc+so; if(!Readable(sec,0x24)) return -1;
        DWORD cnt=*(DWORD*)(sec+0x14), trel=*(DWORD*)(sec+0x20);
        if(cnt==0||cnt>1024||!trel||!Readable(sec+trel,cnt*4)) return -1;
        for(DWORD j=0;j<cnt;j++){
            DWORD eo=*(DWORD*)(sec+trel+j*4); if(!eo) continue;

            for(DWORD off=0x40;off<=0x50;off+=8){
                const char* nm=(const char*)(sec+eo+off);
                if(Readable(nm,48)&&!strncmp(nm,name,40)&&!nm[strlen(name)]) return (int)j;
            }
        }
        return -1;
    }
    return -1;
}

static JChar* CharFor(void* bust,BYTE* tmc){
    DWORD now=GetTickCount(); JChar* free_=nullptr;
    for(auto& c:Chars){
        if(c.bust==bust&&c.tmc==tmc){c.seen=now; return &c;}
        if(!free_||c.seen<free_->seen) free_=&c;
    }
    JChar& c=*free_; memset(&c,0,sizeof c); c.bust=bust; c.tmc=tmc; c.seen=now;
    for(int i=0;i<CfgN;i++){c.b[i].idx=FindNode(tmc,Cfg[i].name);}
    char line[256]="nouveau modele :"; for(int i=0;i<CfgN;i++){char t[64]; snprintf(t,sizeof t," %s=%d",Cfg[i].name,c.b[i].idx); strncat(line,t,sizeof line-strlen(line)-1);}
    Log("%s",line);
    return &c;
}

static void __attribute__((thiscall)) BustHook(void* bust,float dt,float* mats){
    if(LogContact&&bust){

        int slot=0; for(int k=0;k<8;k++){if(Contact[k].bust==bust){slot=k;break;} if(!Contact[k].bust){Contact[k].bust=bust;slot=k;break;}}
        bool* ContactWas=Contact[slot].was;
        for(int side=0;side<2;side++){
            BYTE* e=(BYTE*)bust+0x2E4+side*0x14;
            bool on=e[0]!=0;
            DWORD now=GetTickCount(); DWORD& last=Contact[slot].lastOn[side];
            if(on&&!ContactWas[side]&&now-last>500) Log("contact : poitrine %s, cible %.3f, vitesse %.3f (objet %p)",side?"droite":"gauche",*(float*)(e+8),*(float*)(e+0xC),bust);
            ContactWas[side]=on; if(on) last=now;
        }
    }
    if(Ps4Soft&&bust){

        int* preset=(int*)((BYTE*)bust+0x4C);
        if(*preset>=0&&*preset<5) *preset+=5;
    }
    BustUpdate(bust,dt,mats);
    if(!CfgN||!mats||!bust) return;
    BYTE* owner=*(BYTE**)((BYTE*)bust+4);
    if(!Readable(owner,0x10)) return;
    BYTE** pp=*(BYTE***)(owner+8); if(!Readable(pp,4)) return;
    BYTE* tmc=*pp;
    JChar* c=CharFor(bust,tmc);
    float h=dt; if(!(h>0.0001f&&h<0.2f)) h=1.0f/60;
    for(int i=0;i<CfgN;i++){
        JState& st=c->b[i]; const JBone& b=Cfg[i];
        if(st.idx<0) continue;
        float* m=mats+st.idx*16;
        if(!Readable(m,64)) continue;
        float p[3]={m[12],m[13],m[14]};
        if(DebugFrames>0&&i==0){Log("dt=%g os %s pos=(%.3f %.3f %.3f)",dt,b.name,p[0],p[1],p[2]); DebugFrames--;}
        float jump=0; for(int a=0;a<3;a++){float d=p[a]-st.last[a]; jump+=d*d;}
        if(!st.init||jump>0.25f*0.25f){for(int a=0;a<3;a++){st.s[a]=p[a];st.v[a]=0;st.last[a]=p[a];} st.init=true;}

        float vp[3]; for(int a=0;a<3;a++) vp[a]=(p[a]-st.last[a])/h;
        const int sub=4; float hh=h/sub;
        for(int k=0;k<sub;k++) for(int a=0;a<3;a++){
            float t=st.last[a]+vp[a]*hh*k;
            float acc=b.k*(t-st.s[a])-b.c*(st.v[a]-vp[a])+(a==1?-b.grav:0);
            st.v[a]+=acc*hh; st.s[a]+=st.v[a]*hh;
        }
        for(int a=0;a<3;a++) st.last[a]=p[a];
        float off[3],len=0; for(int a=0;a<3;a++){off[a]=st.s[a]-p[a]; len+=off[a]*off[a];}
        len=sqrtf(len);
        if(len>b.maxd){float r=b.maxd/len; for(int a=0;a<3;a++){off[a]*=r; st.s[a]=p[a]+off[a]; st.v[a]=vp[a];}}
        for(int a=0;a<3;a++) m[12+a]=p[a]+off[a]*b.gain;
    }
}

static void LoadBones(){
    char keys[2048]; DWORD n=GetPrivateProfileSectionNamesA(keys,sizeof keys,IniPath);
    for(char* s=keys;s<keys+n&&*s&&CfgN<8;s+=strlen(s)+1){
        if(strncmp(s,"Bone:",5)) continue;
        JBone& b=Cfg[CfgN]; strncpy(b.name,s+5,39);
        auto F=[&](const char* k,float d){char v[32],dd[32]; snprintf(dd,sizeof dd,"%g",d); GetPrivateProfileStringA(s,k,dd,v,sizeof v,IniPath); return (float)atof(v);};
        if(!GetPrivateProfileIntA(s,"Enabled",1,IniPath)) continue;
        b.k=F("Stiffness",180); b.c=F("Damping",9); b.maxd=F("MaxOffset",0.03f); b.gain=F("Gain",1); b.grav=F("Gravity",0);
        Log("os %s : raideur %g amortissement %g decalage max %g gain %g",b.name,b.k,b.c,b.maxd,b.gain);
        CfgN++;
    }
}

static void InstallJiggle(){
    LoadBones();
    DebugFrames=GetPrivateProfileIntA("SoftBody","DebugFrames",0,IniPath);
    LogContact=GetPrivateProfileIntA("SoftBody","LogContact",1,IniPath)!=0;
    if(!CfgN&&!Ps4Soft&&!LogContact) return;
    BYTE* call=VA(0x98221B);
    if(!Same(call,"e8b042b4ff")){Log("rebond : appel 0x98221B inattendu, rien modifie"); return;}
    BustUpdate=(BustUpdateFn)VA(0x4C64D0);
    BYTE patch[5]={0xE8}; *(DWORD*)(patch+1)=(DWORD)(BYTE*)&BustHook-(DWORD)(call+5);
    Log("crochet poitrine (rebond %d os, PS4 %d, contacts %d) : %s",CfgN,(int)Ps4Soft,(int)LogContact,Write(call,patch,5)?"actif":"echec");
}

static DWORD WINAPI Worker(LPVOID){
    if(Extra_SepareCharge("DOA5LR-SoftBody.asi")){Log("ancien DOA5LR-SoftBody.asi charge : rien modifie"); return 0;}

    BYTE* probe=VA(0x4C3135);
    for(int i=0;i<600;i++){
        if(Same(probe,"0f8475100000")) break;
        Sleep(100);
    }
    if(!Same(probe,"0f8475100000")){Log("code du jeu non reconnu (version differente ?) : arret"); return 0;}

    float* table=(float*)VA(0x10BD720);

    if(Fnv64((const BYTE*)table,sizeof(float)*32)!=0x29f9c9af0fc71f6aULL){Log("table d options inattendue : arret"); return 0;}
    float ref[4][8]; memcpy(ref,table,sizeof ref);
    float mul[8];
    for(int f=0;f<8;f++){char k[16]; snprintf(k,sizeof k,"Field%d",f); mul[f]=IniF(k,1.0f);}
    mul[0]*=IniF("Swing",1.0f); mul[5]*=IniF("Deform",1.0f); mul[6]*=IniF("Deform",1.0f); mul[7]*=IniF("Deform",1.0f);
    Ps4Soft=GetPrivateProfileIntA("SoftBody","PS4Soft",0,IniPath)!=0;
    float out[4][8]; memcpy(out,ref,sizeof ref);
    if(Ps4Soft){

        char racine[MAX_PATH]; GetModuleFileNameA(nullptr,racine,MAX_PATH); if(char* e=strrchr(racine,'\\')) e[1]=0;
        static D5SD donnees; const D5SDBloc *opt=nullptr,*ps4=nullptr;
        if(donnees.Charger(racine,"maps\\DOA5Stages\\SoftEngineLR.d5sd")){opt=donnees.Bloc("opt_lr");ps4=donnees.Bloc("swing_ps4");}
        float* sw=(float*)VA(0x10BD7A0)+5*17;
        if(!opt||opt->taille!=48||!ps4||ps4->taille!=5*72){Ps4Soft=false; Log("donnees PS4 absentes (SoftEngineLR.d5sd) : mode PS4 desactive");}
        else if(Fnv64((const BYTE*)sw,5*17*4)!=0x324ca34392400230ULL){Ps4Soft=false; Log("table de balancement inattendue : mode PS4 desactive");}
        else {
            for(int r=1;r<4;r++) memcpy(out[r],opt->octets,8*sizeof(float));
            float pres[5][17]; for(int r=0;r<5;r++) memcpy(pres[r],ps4->octets+r*72,17*sizeof(float));
            Write((BYTE*)sw,pres,sizeof pres); Log("presets souples PS4 1.19 (5..9) ecrits, mode 4 PS4 actif");
        }
    }
    for(int r=1;r<4;r++) for(int f=0;f<8;f++) out[r][f]*=mul[f];
    Write((BYTE*)table,out,sizeof out);
    Log("table modifiee : swing x%.2f, deformation x%.2f/%.2f/%.2f",mul[0],mul[5],mul[6],mul[7]);

    if(GetPrivateProfileIntA("SoftBody","FloorPressAllCostumes",1,IniPath)) NopJe(0x4C3135,"0f8475100000","ecrasement au sol pour tous les costumes");
    if(GetPrivateProfileIntA("SoftBody","FloorPressAllModes",1,IniPath))    NopJe(0x4C3694,"0f84160b0000","ecrasement au sol dans tous les modes");
    InstallJiggle();
    return 0;
}

extern "C" void SoftBody_Demarrer(void){
    Base=(BYTE*)GetModuleHandleA(nullptr);
    snprintf(IniPath,sizeof IniPath,"%s",Extra_Ini()); (void)LogPath;
    if(HANDLE t=CreateThread(nullptr,0,Worker,nullptr,0,nullptr)) CloseHandle(t);
}
