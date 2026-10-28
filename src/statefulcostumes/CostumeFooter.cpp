#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include "StateCostumes.h"
#include "HotkeyEdge.h"
#include "CostumeFooter.h"
void JournalCostumes(const char* ligne);
namespace CostumeFooter { namespace {
unsigned char* base;
using UpdateFn=void(__thiscall*)(void*);
UpdateFn original;
volatile LONG playerStage=0,eligible=0,stamp=0,presses=0;
HWND overlay;
int fontHeight=22;
bool saveError=false;
void NativeLog(const char* line);
uintptr_t At(uint32_t va){return reinterpret_cast<uintptr_t>(base)+va-0x400000;}
template<class T> bool Read(uintptr_t p,T& value){
 SIZE_T n=0;return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(p),&value,sizeof value,&n)&&n==sizeof value;
}
bool CostumeStage(void* object){

 uintptr_t obj=reinterpret_cast<uintptr_t>(object),list=0;
 uint32_t count=0,id=0,vt=0,gui=0;
 if(!Read(At(0x138a7ac),gui)||gui!=4||!Read(obj,vt)||vt!=At(0x100cea4)
   ||!Read(obj+0x54,count)||count>16||!Read(obj+0x5c,list)
   ||!Read(At(0x247ee34),id))return false;

 bool any=false;LONG mask=0;
 for(unsigned i=0;i<count;++i){
  uintptr_t entry=0,state=0;uint32_t entryId=0,value=0;
  if(!Read(list+4*i,entry)||!entry||!Read(entry,entryId))continue;
  if(Read(entry+4,state)&&state&&Read(state,value)&&value>=5&&value<=8){any=true;if(entryId<2)mask|=1<<entryId;}
 }
 InterlockedExchange(&playerStage,mask);
 return any;
}
void __fastcall Update(void* object,void*){
 original(object);
 static HotkeyEdge edge;
 DWORD pid=0;GetWindowThreadProcessId(GetForegroundWindow(),&pid);
 unsigned char device=255;uint32_t buttons=0;

 const bool stage=CostumeStage(object);
 bool active=stage&&pid==GetCurrentProcessId();
 bool valid=Read(At(0x14877b8),device)&&device<4&&Read(At(0x2457928)+44*device,buttons);
 if(edge.Update(active,valid,(buttons&0x2000)!=0))InterlockedIncrement(&presses);
 InterlockedExchange(&eligible,stage);
 InterlockedExchange(&stamp,static_cast<LONG>(GetTickCount()));
}

const char* const kCharaNames[49]={"ZACK","TINA","JANNLEE","EIN","HAYABUSA","KASUMI","GENFU","HELENA","LEON","BASS",
 "KOKORO","HAYATE","LEIFANG","AYANE","ELIOT","LISA","ALPHA152",nullptr,nullptr,"BRAD","CHRISTIE","HITOMI",nullptr,nullptr,
 "BAYMAN",nullptr,nullptr,nullptr,nullptr,"RIG","MILA","AKIRA","SARAH","PAI",nullptr,nullptr,nullptr,nullptr,nullptr,"MOMIJI",
 "RACHEL","JACKY","MARIE","PHASE4","NYOTENGU","HONOKA","RAIDOU","NAOTORA","MAI"};
bool StarWanted(unsigned chara,int costume){

 if(chara>=49||!kCharaNames[chara])return false;
 unsigned char first=0;int extra=0;
 if(!Read(At(0xd89a91+chara*20),first)||!Read(At(0xd725f8+chara*4),extra))return false;
 const int slot=costume-(int(first)+extra);
 if(slot<0||slot>254)return false;
 const int kind=StateCostumes::ClassifyCostume(kCharaNames[chara],static_cast<unsigned>(slot+1));
 return kind&&StateCostumes::IsTransitionEnabled(kind==2);
}
#include "NativeFooter.inc"
LRESULT CALLBACK WindowProc(HWND w,UINT m,WPARAM wp,LPARAM lp){
 if(m==WM_NCHITTEST)return HTTRANSPARENT;
 if(m==WM_MOUSEACTIVATE)return MA_NOACTIVATE;
 if(m==WM_ERASEBKGND)return 1;
 if(m==WM_PAINT){
  PAINTSTRUCT ps;HDC dc=BeginPaint(w,&ps);RECT r;GetClientRect(w,&r);

  FillRect(dc,&r,static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
  HFONT font=CreateFontW(-fontHeight,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,ANTIALIASED_QUALITY,0,L"Segoe UI");
  HGDIOBJ prev=SelectObject(dc,font);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(240,240,240));
  bool a=StateCostumes::IsTransitionEnabled(false),b=StateCostumes::IsTransitionEnabled(true);
  const wchar_t* label=saveError?L"Save failed":a&&b?L"Costume effects: ON":!a&&!b?L"Costume effects: OFF":L"Costume effects: MIXED";
  SIZE size{};GetTextExtentPoint32W(dc,label,lstrlenW(label),&size);
  int badgeWidth=fontHeight*17/10,gap=fontHeight/3;
  int left=r.right-size.cx-gap-badgeWidth-fontHeight/3;
  if(left<3)left=3;
  int top=(r.bottom-fontHeight*13/10)/2;
  int bottom=top+fontHeight*13/10,bevel=fontHeight/7;
  HPEN pen=CreatePen(PS_SOLID,2,RGB(110,110,103));HGDIOBJ oldPen=SelectObject(dc,pen);
  HBRUSH badge=CreateSolidBrush(RGB(45,45,43));HGDIOBJ oldBrush=SelectObject(dc,badge);
  POINT shape[]={{left+bevel,top},{left+badgeWidth-bevel,top},{left+badgeWidth,bottom},{left,bottom}};
  Polygon(dc,shape,4);
  HPEN inner=CreatePen(PS_SOLID,1,RGB(155,155,143));SelectObject(dc,inner);
  SelectObject(dc,GetStockObject(NULL_BRUSH));
  POINT inset[]={{left+bevel+2,top+3},{left+badgeWidth-bevel-2,top+3},{left+badgeWidth-4,bottom-4},{left+4,bottom-4}};
  Polygon(dc,inset,4);
  SelectObject(dc,oldBrush);SelectObject(dc,oldPen);DeleteObject(badge);DeleteObject(pen);DeleteObject(inner);
  RECT keyRect{left,top,left+badgeWidth,top+fontHeight*13/10};
  SetTextColor(dc,RGB(15,15,15));
  for(int dx=-1;dx<=1;++dx)for(int dy=-1;dy<=1;++dy){RECT outline=keyRect;OffsetRect(&outline,dx,dy);DrawTextW(dc,L"R1",-1,&outline,DT_SINGLELINE|DT_VCENTER|DT_CENTER);}
  SetTextColor(dc,RGB(215,215,200));DrawTextW(dc,L"R1",-1,&keyRect,DT_SINGLELINE|DT_VCENTER|DT_CENTER);
  RECT textRect{left+badgeWidth+gap,0,r.right,r.bottom};
  RECT shadow=textRect;OffsetRect(&shadow,2,2);SetTextColor(dc,RGB(24,24,24));
  DrawTextW(dc,label,-1,&shadow,DT_SINGLELINE|DT_VCENTER|DT_LEFT);
  SetTextColor(dc,RGB(210,210,198));DrawTextW(dc,label,-1,&textRect,DT_SINGLELINE|DT_VCENTER|DT_LEFT);
  SelectObject(dc,prev);DeleteObject(font);EndPaint(w,&ps);return 0;
 }
 return DefWindowProcW(w,m,wp,lp);
}
}
bool Install(unsigned char* image) noexcept {
 base=image;

 auto slot=reinterpret_cast<void**>(At(0x100cf28));uintptr_t found=0;
 if(!Read(reinterpret_cast<uintptr_t>(slot),found)||found!=At(0xabdae0))return false;
 original=reinterpret_cast<UpdateFn>(found);
 DWORD old=0;if(!VirtualProtect(slot,sizeof(void*),PAGE_READWRITE,&old))return false;
 void* result=InterlockedCompareExchangePointer(slot,reinterpret_cast<void*>(&Update),reinterpret_cast<void*>(found));
 DWORD ignored;VirtualProtect(slot,sizeof(void*),old,&ignored);
 if(result==reinterpret_cast<void*>(found))InstallNativeText();
 return result==reinterpret_cast<void*>(found);
}
void Poll() noexcept { if(!original)return;
 if(InterlockedExchange(&presses,0)&1){
  bool enabled=!(StateCostumes::IsTransitionEnabled(false)&&StateCostumes::IsTransitionEnabled(true));
  StateCostumes::SetTransitionEnabled(false,enabled);StateCostumes::SetTransitionEnabled(true,enabled);
  saveError=!StateCostumes::SaveTransitionOptions();
  if(overlay)InvalidateRect(overlay,nullptr,FALSE);
 }
 HWND game=GetForegroundWindow();DWORD pid=0;GetWindowThreadProcessId(game,&pid);
 bool show=InterlockedCompareExchange(&eligible,0,0)!=0&&pid==GetCurrentProcessId()
  &&static_cast<DWORD>(GetTickCount()-static_cast<DWORD>(InterlockedCompareExchange(&stamp,0,0)))<200;
 if(nativeText&&static_cast<DWORD>(GetTickCount()-static_cast<DWORD>(InterlockedCompareExchange(&nativeStamp,0,0)))<200)show=false;
 if(show&&!overlay){
  WNDCLASSW wc{};wc.lpfnWndProc=WindowProc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"DOA5_CostumeFooter";
  RegisterClassW(&wc);
  overlay=CreateWindowExW(WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW,
   wc.lpszClassName,L"",WS_POPUP,0,0,1,1,game,nullptr,wc.hInstance,nullptr);
  if(overlay)SetLayeredWindowAttributes(overlay,RGB(0,0,0),255,LWA_COLORKEY);
 }
 if(overlay){
  if(show){
   RECT r{};GetClientRect(game,&r);POINT origin{};ClientToScreen(game,&origin);
   int width=r.right,height=r.bottom;
   int nextFont=height*30/1147;if(nextFont<14)nextFont=14;
   if(nextFont!=fontHeight){fontHeight=nextFont;InvalidateRect(overlay,nullptr,FALSE);}

   SetWindowPos(overlay,HWND_TOPMOST,origin.x+width*46/100,origin.y+height*885/1000,
    width*27/100,height*52/1000,SWP_NOACTIVATE|SWP_SHOWWINDOW);
  }else ShowWindow(overlay,SW_HIDE);
 }
 MSG msg;while(PeekMessageW(&msg,overlay,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}
}
}
