#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace cb {
constexpr float pi=3.14159265358979323846f;
struct Vec { float x=0,y=0,z=0; };
inline Vec operator+(Vec a,Vec b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline Vec operator-(Vec a,Vec b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline Vec operator*(Vec a,float b){return {a.x*b,a.y*b,a.z*b};}
inline float dot(Vec a,Vec b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline Vec cross(Vec a,Vec b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline bool normalize(Vec& v){float n=dot(v,v);if(!std::isfinite(n)||n<1e-12f)return false;v=v*(1/std::sqrt(n));return true;}

inline bool billboard(Vec position,Vec camera,Vec cameraUp,Vec rotation,float width,float height,float* m){
 Vec normal=camera-position;if(!normalize(normal))return false;
 Vec right=cross(cameraUp,normal);
 if(!normalize(right)){right=cross(std::abs(normal.y)<.9f?Vec{0,1,0}:Vec{1,0,0},normal);if(!normalize(right))return false;}
 Vec up=cross(normal,right);
 float cx=std::cos(rotation.x),sx=std::sin(rotation.x);
 float cy=std::cos(rotation.y),sy=std::sin(rotation.y);
 float cz=std::cos(rotation.z),sz=std::sin(rotation.z);

 Vec x=(right*(cz*cy)+up*(sz*cy)-normal*sy)*width;
 Vec y=(right*(cz*sy*sx-sz*cx)+up*(sz*sy*sx+cz*cx)+normal*(cy*sx))*height;
 Vec z=right*(cz*sy*cx+sz*sx)+up*(sz*sy*cx-cz*sx)+normal*(cy*cx);
 const float result[16]={x.x,x.y,x.z,0,y.x,y.y,y.z,0,z.x,z.y,z.z,0,position.x,position.y,position.z,1};
 std::copy(result,result+16,m);return true;
}
struct Quad {Vec center;Vec rotation;float width,height,alpha;unsigned texture;};

inline float mask(unsigned kind,float x,float y){

 struct Point{float x,y;};
 static constexpr Point chip[]={{-.26f,0},{-.12f,-.23f},{.01f,-.19f},{.09f,-.09f},{.24f,-.01f},{.23f,.17f},{.10f,.23f},{-.02f,.15f},{-.15f,.18f},{-.19f,.06f}};
 static constexpr Point sliver[]={{-.14f,.23f},{-.015f,-.25f},{.025f,-.22f},{.06f,.12f},{.14f,.23f},{.02f,.18f}};
 static constexpr Point shard[]={{-.18f,-.10f},{-.05f,-.12f},{.03f,-.26f},{.12f,-.07f},{.16f,.10f},{.02f,.25f},{-.13f,.12f}};
 const Point* p=kind==0?chip:(kind==1?sliver:shard);unsigned count=kind==0?10:(kind==1?6:7);

 float area=0,cx=0,cy=0;
 for(unsigned i=0;i<count;i++){auto a=p[i],b=p[(i+1)%count];float z=a.x*b.y-b.x*a.y;area+=z;cx+=(a.x+b.x)*z;cy+=(a.y+b.y)*z;}
 x=x/(kind==0?.964f:(kind==2?1.006f:1.f))+cx/(3*area);
 y=y/(kind==0?.964f:(kind==1?1.14f:1.006f))+cy/(3*area);
 bool inside=false;
 for(unsigned i=0,j=count-1;i<count;j=i++)if((p[i].y>y)!=(p[j].y>y)){
  if(x<(p[j].x-p[i].x)*(y-p[i].y)/(p[j].y-p[i].y)+p[i].x)inside=!inside;
 }
 return inside?1.f:0.f;
}
inline float coverage(unsigned kind,float x,float y){
 float sum=0;for(unsigned j=0;j<4;j++)for(unsigned i=0;i<4;i++)sum+=mask(kind,x+(float(i)-1.5f)/256,y+(float(j)-1.5f)/256);
 return sum/16;
}
inline float spriteAlpha(float x,float y,float rgb){

 float r2=x*x+y*y;float halo=r2<1?.38f*std::exp(-r2/(2*.36f*.36f)):0;
 return std::max(rgb,halo);
}
using DDS=std::array<uint8_t,16512>;
inline DDS texture(unsigned kind){
 DDS d{};auto put=[&](unsigned at,uint32_t v){for(unsigned k=0;k<4;k++)d[at+k]=uint8_t(v>>(8*k));};
 put(0,0x20534444);put(4,124);put(8,0x81007);put(12,128);put(16,128);put(20,16384);put(28,1);
 put(76,32);put(80,4);put(84,0x35545844);put(108,0x1000);
 for(unsigned by=0;by<32;by++)for(unsigned bx=0;bx<32;bx++){

  unsigned at=128+(by*32+bx)*16;uint32_t bits=0;unsigned alpha[16],lo=255,hi=0;
  const unsigned levels[4]={255,0,170,85};
  for(unsigned y=0;y<4;y++)for(unsigned x=0;x<4;x++){
   float u=(bx*4+x+.5f)/64-1,v=(by*4+y+.5f)/64-1,rgb=coverage(kind,u,v);
   unsigned a=unsigned(rgb*255+.5f),best=0,err=256;
   unsigned al=unsigned(spriteAlpha(u,v,rgb)*255+.5f);alpha[y*4+x]=al;lo=std::min(lo,al);hi=std::max(hi,al);
   for(unsigned j=0;j<4;j++){unsigned e=unsigned(std::abs(int(a)-int(levels[j])));if(e<err){err=e;best=j;}}
   bits|=uint32_t(best)<<(2*(4*y+x));
  }
  d[at]=uint8_t(hi);d[at+1]=uint8_t(lo);unsigned palette[8]={hi,lo};uint64_t alphaBits=0;
  for(unsigned i=2;i<8;i++)palette[i]=((8-i)*hi+(i-1)*lo)/7;
  for(unsigned i=0;i<16;i++){unsigned best=0,error=256;for(unsigned k=0;k<8;k++){unsigned e=unsigned(std::abs(int(alpha[i])-int(palette[k])));if(e<error){error=e;best=k;}}alphaBits|=uint64_t(best)<<(3*i);}
  for(unsigned i=0;i<6;i++)d[at+2+i]=uint8_t(alphaBits>>(8*i));
  for(unsigned k=0;k<4;k++)d[at+12+k]=uint8_t(bits>>(8*k));
  d[at+8]=d[at+9]=255;d[at+10]=d[at+11]=0;
 }
 return d;
}
}
