#ifndef PREFLIGHT_TOOL
static void __cdecl CommonEffectSizes() {
 Put32(At(0x124b2a0)+0x2a73*8,1133872);
 Put32(At(0x124b2a0)+0x2a74*8,3493248);
 Put32(At(0x1268708)+18041*8,399604);
 Put32(At(0x1268708)+18042*8,1119092);
 Log("Original particle candidate: common-load decoded sizes 1133872/3493248 applied before requests.");
}
static bool ArmCommonEffectArchive() {
 if(!HashFile(std::string(Root)+"common.lnk",282050560ull,
   "91ac20f972cbb2f9256b4d2c15e4817c5140ae800589f5ec8cfa7d42b1271f90")){
  Log("Refused original particles: candidate archive fingerprint differs.");return false;
 }
 BYTE original[5]={0xa1,0,0,0,0};Put32(original+1,(uint32_t)(uintptr_t)At(0x2453870));
 if(!Signature(0x8306a0,original,5)
  ||U32(At(0x124b2a0)+0x2a73*8)!=1023488
  ||U32(At(0x124b2a0)+0x2a74*8)!=2926592
  ||U32(At(0x1268708)+18041*8+4)!=0xc0002a73
  ||U32(At(0x1268708)+18042*8+4)!=0xc0002a74){
  Log("Refused original particles: common-load signature or resource bindings differ.");return false;
 }
 BYTE* gateway=(BYTE*)VirtualAlloc(nullptr,32,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
 if(!gateway)return false;
 gateway[0]=0x9c;gateway[1]=0x60;gateway[2]=0xe8;
 Put32(gateway+3,(uint32_t)((BYTE*)&CommonEffectSizes-(gateway+7)));
 gateway[7]=0x61;gateway[8]=0x9d;memcpy(gateway+9,original,5);
 gateway[14]=0xe9;Put32(gateway+15,(uint32_t)(At(0x8306a5)-(gateway+19)));
 DWORD old=0;
 if(!VirtualProtect(gateway,32,PAGE_EXECUTE_READ,&old)){VirtualFree(gateway,0,MEM_RELEASE);return false;}
 FlushInstructionCache(GetCurrentProcess(),gateway,19);
 BYTE patch[5]={0xe9,0,0,0,0};Put32(patch+1,(uint32_t)(gateway-(At(0x8306a0)+5)));
 if(!VirtualProtect(At(0x8306a0),5,PAGE_EXECUTE_READWRITE,&old)){VirtualFree(gateway,0,MEM_RELEASE);return false;}
 memcpy(At(0x8306a0),patch,5);DWORD ignored;
 VirtualProtect(At(0x8306a0),5,old,&ignored);FlushInstructionCache(GetCurrentProcess(),At(0x8306a0),5);
 Log("Armed original particle candidate common-load metadata hook; offline runtime test pending.");
 return true;
}
#endif
