#include "../commun/extra.h"
#include "RuntimeGlue.h"
#include "StateCostumes.h"
#include "StateCostumesData.h"
#include "ProtectedPointer.h"
#include "AudioBridgeValidation.h"
#include "ParticleResetLifetime.h"
#include "OnlineCostumePolicy.h"
#include "StatefulReload.h"
#include "NativeEffectRngTls.h"
#include "NativeEffectRngHookInstall.h"
#include "FastRegion.h"
#include <optional>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <cmath>
#ifdef PREFLIGHT_TOOL
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#endif

static_assert(sizeof(void*) == 4, "32-bit DOA5LR only");
namespace RuntimeGlue {
static bool UsesSchoolgirlTiming(void* entity) noexcept;
static void ConfirmPLContact(void* entity) noexcept;
namespace {
struct AlternatePair {uint32_t h,hl;const char* hName;const char* hlName;};
struct Alternate {const char* name;uint32_t fieldRva,expectedPointerRva;AlternatePair pairs[4];};
struct TauntId {uint8_t character;uint32_t action;};
static const Alternate kAlternates[]={
{"MARIE_DLCU_028",0xbb48b0u,0u,{{5498,5499,"7R61Y@T21210U1@8I1T]]L129J4S3H1R1J199T5I1Q{UC61KL913","BR61Y@T21210U1@EI1]U]D129J4S3H1R1BJ1BAT6I1Q{UD61KL913"},{5500,5501,"7S61Y@T2121051@7H1S]]K129I4R3G1Q1I199S5I1P{TC61KL913","BS61Y@T2121051@DH1]T]C129I4R3G1Q1BI1BAS6I1P{TD61KL913"},{5502,5503,"7T61Y@10T100V1@8I1T]]L129J4S3H1R1J199T5I1Q{UC61KL913","BT61Y@10T100V1@EI1]U]D129J4S3H1R1BJ1BAT6I1Q{UD61KL913"},{5504,5505,"7U61Y@10T100V1@8I1T]]L129J4S3H1R1J199T5I1Q{UC61KL913","BU61Y@10T100V1@EI1]U]D129J4S3H1R1BJ1BAT6I1Q{UD61KL913"}}},
{"MOMIJI_DLCU_032",0xbb10d0u,0u,{{5322,5323,"7R97S@T212U121@9T]]K128LSH1L412G1J1TP1CB6182{UC51LLB1C","BRA7S@T212U121@F]U]D128LSH1L412G1BJ1TP1EC7192{UD51LLB1C"},{5324,5325,"7S973@S212U121@8S]]J128KRG1K412G1I1SO1CB6182{TC51LLB1C","BSA73@S212U121@E]T]C128KRG1K412G1BI1SO1EC7192{TD51LLB1C"},{5326,5327,"7T97T@T2161310@8S]]J128KRG1K412G1I1SO1CB6182{TC51LLB1C","BTA7T@T2161310@E]T]C128KRG1K412G1BI1SO1EC7192{TD51LLB1C"},{5328,5329,"7U97T@10TV1210@9T]]K128LSH1L412G1J1TP1CB6182{UC51LLB1C","BUA7T@10TV1210@F]U]D128LSH1L412G1BJ1TP1EC7192{UD51LLB1C"}}},
{"HELENA_DLCU_032",0xb991b0u,0u,{{4442,4443,"7R06S@T212U121@8J1T]]L129M8110SH187I1G1T003P{UA511K71K","BR06S@T212U121@EJ1]U]D129M8110SH9735I1G1T005P{UB511K71K"},{4444,4445,"7S062@S212U121@7I1S]]K129L8110RG187H1G1S003O{TA511K71K","BS062@S212U121@DI1]T]C129L8110RG9735H1G1S005O{TB511K71K"},{4446,4447,"7T06T@T2151310@7I1S]]K129L8110RG187H1G1S003O{TA511K71K","BT06T@T2151310@DI1]T]C129L8110RG9735H1G1S005O{TB511K71K"},{4448,4449,"7U06T@10TV1210@8J1T]]L129M8110SH187I1G1T003P{UA511K71K","BU06T@10TV1210@EJ1]U]D129M8110SH9735I1G1T005P{UB511K71K"}}},
{"KOKORO_DLCU_032",0xb9cce8u,0xb9bca8u,{{4552,4553,"7RF7Z@T212V121S1@A0]]L12810MSH176F1I1TSP10053510{UB6171LLA15Z1","BRF7Z@T212V121S1@G]0]D12810MSH176F19I1TSP10063510{UC6171LLA15Z1"},{4554,4555,"7SF7Z@T212V121S1@A0]]L12810MSH176F1I1TSP10053510{UB6171LLA15M1","BSF7Z@T212V121S1@G]0]D12810MSH176F19I1TSP10063510{UC6171LLA15M1"},{4556,4557,"7TF7Z@T2161310R1@90]]K12810LRG176F1H1SRO10053510{TB6171LLA15Z1","BTF7Z@T2161310R1@F]0]C12810LRG176F19H1SRO10063510{TC6171LLA15Z1"},{4558,4559,"7UF7Z@10TW1210S1@A0]]L12810MSH176F1I1TSP10053510{UB6171LLA15_1","BUF7Z@10TW1210S1@G]0]D12810MSH176F19I1TSP10063510{UC6171LLA15_1"}}},
{"CHRISTIE_DLCU_032",0xba857cu,0u,{{4992,4993,"7RS71@T212U121@8T]1]1K118ILS3H10031R1J1T4D171Q{U10971JLA13","BRS71@T212U121@E]U2]1C118ILS3H10031R1BJ1T54171Q{U10971JLA13"},{4994,4995,"7S161@S212U121@7S]1]1J118HKR3G10031Q1I1S4D171P{T10971JKA13","BS161@S212U121@D]T2]1B118HKR3G10031Q1BI1S54171P{T10971JKA13"},{4996,4997,"7TT71@T2151310@7S]1]1J118HKR3G10031Q1I1S4D171P{T10971JLA13","BTT71@T2151310@D]T2]1B118HKR3G10031Q1BI1S54171P{T10971JLA13"},{4998,4999,"7UT71@10TV1210@8T]1]1K118ILS3H10031R1J1T4D171Q{U10971JLA13","BUT71@10TV1210@E]U2]1C118ILS3H10031R1BJ1T54171Q{U10971JLA13"}}},
{"LISA_DLCU_032",0xba4ec0u,0xba41d8u,{{4882,4883,"7R06Z@T212V121S1@9P11]]L12910MSH141I187T5M1Q{DB6161KK91KZ1","BR06Z@T212V121S1@FP1]2]D12910MSH1419I1A8T6M1Q{DC6161KK91KZ1"},{4884,4885,"7S06Z@T212V121S1@9P11]]L12910MSH141I187T5M1Q{DB6161KK91KL1","BS06Z@T212V121S1@FP1]2]D12910MSH1419I1A8T6M1Q{DC6161KK91KL1"},{4886,4887,"7T06Z@T2151310R1@8O11]]K12910LRG141H187S5L1P{DB6161KK91KZ1","BT06Z@T2151310R1@EO1]2]C12910LRG1419H1A8S6L1P{DC6161KK91KZ1"},{4888,4889,"7U06Z@10TW1210S1@9P11]]L12910MSH141I187T5M1Q{DB6161KK91K_1","BU06Z@10TW1210S1@FP1]2]D12910MSH1419I1A8T6M1Q{DC6161KK91K_1"}}},
{"KASUMI_DLCU_032",0xb96810u,0u,{{4332,4333,"7R97S@T212U121@9G1T]]K129LSI13176J1T105PG1{UB2161KK71L","BRA7S@T212U121@FH1]U]E129LSI13186AJ1T106PG1{UC2161KK71L"},{4334,4335,"7S973@S212U121@8F1S]]J129KRH13176I1S105OG1{TB2161KK71L","BSA73@S212U121@EG1]T]D129KRH13186AI1S106OG1{TC2161KK71L"},{4336,4337,"7T97T@T2161310@8F1S]]J129KRH13176I1S105OG1{TB2161KK71L","BTA7T@T2161310@EG1]T]D129KRH13186AI1S106OG1{TC2161KK71L"},{4338,4339,"7U97T@10TV1210@9G1T]]K129LSI13176J1T105PG1{UB2161KK71L","BUA7T@10TV1210@FH1]U]E129LSI13186AJ1T106PG1{UC2161KK71L"}}},
{"RACHEL_DLCU_032",0xbb28d0u,0u,{{5432,5433,"7RC7S@T212U121@9N1T]81]C12AM91SH100FI12T3CAQ{UA719KA1C","BR26S@T212U121@EN1]71U]C12AM91SH1001FI12T3DAQ{UA719KA1C"},{5434,5435,"7SC73@S212U121@8M1S]81]C12AL91RG100FH12S3CAP{TA719KA1C","BS263@S212U121@DM1]71T]C12AL91RG1001FH12S3DAP{TA719KA1C"},{5436,5437,"7TC7T@T2161310@8M1S]81]C12AL91RG100FH12S3CAP{TA719KA1C","BT26T@T2161310@DM1]71T]C12AL91RG1001FH12S3DAP{TA719KA1C"},{5438,5439,"7UC7T@10TV1210@9N1T]81]C12AM91SH100FI12T3CAQ{UA719KA1C","BU26T@10TV1210@EN1]71U]C12AM91SH1001FI12T3DAQ{UA719KA1C"}}},
{"NYOTENGU_DLCU_028",0xbb8ac4u,0u,{{5960,5961,"7RL7Y@T21210U1@9T]]L128MT10O1I1J1HH1BA5W1Q10{VPC7137C1MD10","BRL7Y@T21210U1@F]U]D128MT10O1I1AJ1HH1DB5W1Q10{VPD7137D1MD10"},{5962,5963,"7SL7Y@T2121061@8S]]K128LS10N1H1I1HH1BA5V1P10{UOC7137C1MD10","BSL7Y@T2121061@E]T]C128LS10N1H1AI1HH1DB5V1P10{UOD7137D1MD10"},{5964,5965,"7TL7Y@10T100V1@9T]]L128MT10O1I1J1HH1BA5W1Q10{VPC7137C1MD10","BTL7Y@10T100V1@F]U]D128MT10O1I1AJ1HH1DB5W1Q10{VPD7137D1MD10"},{5966,5967,"7UL7Y@10T100V1@9T]]L128MT10O1I1J1HH1BA5W1Q10{VPC7137C1MD10","BUL7Y@10T100V1@F]U]D128MT10O1I1AJ1HH1DB5W1Q10{VPD7137D1MD10"}}},
{"MILA_DLCU_032",0xbadcd0u,0u,{{5212,5213,"7R06S@T212U121@8H1T]]0018LSG131K1I1T996P{UB51KKA1K","BR06S@T212U121@E91]U]0018LSG1319K1I1TBA7P{UC51KKA1K"},{5214,5215,"7S062@S212U121@7G1S]]0018KRF131J1H1S996O{TB51KKA1K","BS062@S212U121@D81]T]0018KRF1319J1H1SBA7O{TC51KKA1K"},{5216,5217,"7T06T@T2151310@7G1S]]0018KRF131J1H1S996O{TB51KKA1K","BT06T@T2151310@D81]T]0018KRF1319J1H1SBA7O{TC51KKA1K"},{5218,5219,"7U06T@10TV1210@8H1T]]0018LSG131K1I1T996P{UB51KKA1K","BU06T@10TV1210@E91]U]0018LSG1319K1I1TBA7P{UC51KKA1K"}}},
{"AYANE_DLCU_032",0xba28b4u,0u,{{4772,4773,"7RS71@T212U1217@U1]]W33L129NNT3I1J1UH13Q{V951K21L914","BRS71@T212U121D@U1]]2W3D129NNT3I1CJ1UH13Q{VA51K21L914"},{4774,4775,"7S161@S212U1216@T1]]V33K129MMS3H1I1TI13P{U951K21L914","BS161@S212U121C@T1]]2V3C129MMS3H1CI1TI13P{UA51K21L914"},{4776,4777,"7TT71@T21513106@T1]]V33K129MMS3H1I1TH13P{U951K21L914","BTT71@T2151310C@T1]]2V3C129MMS3H1CI1TH13P{UA51K21L914"},{4778,4779,"7UT71@10TV12107@U1]]W33L129NNT3I1J1UH13Q{V951K21L914","BUT71@10TV1210D@U1]]2W3D129NNT3I1CJ1UH13Q{VA51K21L914"}}},
{"HITOMI_DLCU_032",0xbaa9f8u,0u,{{5102,5103,"7R97S@T212U121@9T]]K128LSH1765912I1TP19162{J1U251KK914","BRA7S@T212U121@F]U]D128LSH86359120I1TP19172{J1U251KK914"},{5104,5105,"7S973@S212U121@8S]]J128KRG1765912H1SO19162{J1T251KK914","BSA73@S212U121@E]T]C128KRG86359120H1SO19172{J1T251KK914"},{5106,5107,"7T97T@T2161310@8S]]J128KRG1765912H1SO19162{J1T251KK914","BTA7T@T2161310@E]T]C128KRG86359120H1SO19172{J1T251KK914"},{5108,5109,"7U97T@10TV1210@9T]]K128LSH1765912I1TP19162{J1U251KK914","BUA7T@10TV1210@F]U]D128LSH86359120I1TP19172{J1U251KK914"}}},
{"HONOKA_DLCU_017",0xbb9b38u,0u,{{6363,6364,"7R06X@T2121051@7F1S]]J129KRG176N1I1S21O1E153{TB51LLA1L","BR06X@T2121051@DG1]T]C129KRG186N1BI1S21O14153{TB51LLA1L"},{6365,6366,"7S06X@10T100U1@8G1T]]K129LSH176O1J1T21P1E153{UB51LLA1L","BS06X@10T100U1@EH1]U]D129LSH186O1BJ1T21P14153{UB51LLA1L"},{6367,6368,"7T06X@10T100U1@8G1T]]K129LSH176O1J1T21P1E153{UB51LLA1L","BT06X@10T100U1@EH1]U]D129LSH186O1BJ1T21P14153{UB51LLA1L"},{6369,6370,"7U06X@10T100U1@8G1T]]K129LSH176O1J1T21P1E153{UB51LLA1L","BU06X@10T100U1@EH1]U]D129LSH186O1BJ1T21P14153{UB51LLA1L"}}},
{"PHASE4_DLCU_028",0xbb63c0u,0u,{{5608,5609,"7RU7Y@U2121041V1@910U]]L129MS1TI1R1K1U3BAH1S{9A81IKB1L","BRU7Y@U2121041V1@F10]V]D129MS1TIR128K1U4CBH1S{AB81IKB1L"},{5610,5611,"7SU7Y@U212104161@810T]]K129LR1SH1Q1J1T3BAH1R{9A81IKB1L","BSU7Y@U212104161@E10]U]C129LR1SHQ128J1T4CBH1R{AB81IKB1L"},{5612,5613,"7TU7Y@10U10041W1@910U]]L129MS1TI1R1K1U3BAH1S{9A81IKB1L","BTU7Y@10U10041W1@F10]V]D129MS1TIR128K1U4CBH1S{AB81IKB1L"},{5614,5615,"7U16Y@10T10041V1@810T]]K129LR1SH1Q1J1T3BAJ1R{9A81ILB1L","BU16Y@10T10041V1@E10]U]C129LR1SHQ128J1T4CBJ1R{AB81ILB1L"}}},
{"LEIFANG_DLCU_032",0xb9ff40u,0u,{{4662,4663,"7R87S@T212U121@9D1T]]K129L00SK1N1K1H1L1BATI16P{UD51AM91N","BR87S@T212U121@EE1]U]E129L00SK1N1K1H1DL1DBTI17P{UE51AM91N"},{4664,4665,"7S873@S212U121@8C1S]]J129K00RJ1M1J1H1K1BASI16O{TD51AM91N","BS873@S212U121@DD1]T]D129K00RJ1M1J1H1DK1DBSI17O{TE51AM91N"},{4666,4667,"7T87T@T2161310@8C1S]]J129K00RJ1M1J1H1K1BASI16O{TD51AM91N","BT87T@T2161310@DD1]T]D129K00RJ1M1J1H1DK1DBSI17O{TE51AM91N"},{4668,4669,"7U87T@10TV1210@9D1T]]K129L00SK1N1K1H1L1BATI16P{UD51AM91N","BU87T@10TV1210@EE1]U]E129L00SK1N1K1H1DL1DBTI17P{UE51AM91N"}}},
{"TINA_DLCU_032",0xb9277cu,0u,{{4222,4223,"7R06S@T212U121@8J1T]]L129MSH121I1TM14P{CBU971LLB1L","BR06S@T212U121@EJ1]U]D129MSH121AI1TM15P{ECU971LLB1L"},{4224,4225,"7S062@S212U121@7I1S]]K129LRG121H1SL14O{CBT971LLB1L","BS062@S212U121@DI1]T]C129LRG121AH1SL15O{ECT971LLB1L"},{4226,4227,"7T06T@T2151310@7I1S]]K129LRG121H1SL14O{CBT971LLB1L","BT06T@T2151310@DI1]T]C129LRG121AH1SL15O{ECT971LLB1L"},{4228,4229,"7U06T@10TV1210@8J1T]]L129MSH121I1TM14P{CBU971LLB1L","BU06T@10TV1210@EJ1]U]D129MSH121AI1TM15P{ECU971LLB1L"}}},
};
static const TauntId kTaunts[]={
{0,8021u},
{0,8023u},
{0,8024u},
{0,8025u},
{1,8015u},
{1,8016u},
{1,8581u},
{2,8020u},
{2,8021u},
{2,8022u},
{2,8025u},
{2,8119u},
{3,8008u},
{3,8009u},
{3,8267u},
{4,8013u},
{4,8014u},
{4,8015u},
{5,8015u},
{5,8504u},
{5,8505u},
{5,8506u},
{6,8012u},
{6,8013u},
{7,8012u},
{7,8474u},
{7,8475u},
{7,8476u},
{8,8018u},
{8,8020u},
{8,8507u},
{9,8018u},
{9,8020u},
{9,8558u},
{10,8009u},
{10,8010u},
{10,8346u},
{11,8013u},
{11,8016u},
{11,8477u},
{11,8478u},
{12,8013u},
{12,8014u},
{12,8015u},
{12,8016u},
{12,8387u},
{13,8020u},
{13,8021u},
{13,8023u},
{13,8497u},
{13,8498u},
{13,8499u},
{14,8012u},
{14,8013u},
{14,8354u},
{14,8362u},
{15,8012u},
{15,8013u},
{19,8012u},
{19,8013u},
{19,8014u},
{19,8015u},
{19,8551u},
{20,8015u},
{20,8501u},
{21,8017u},
{21,8018u},
{21,8433u},
{21,8448u},
{24,8014u},
{24,8015u},
{24,8016u},
{24,8523u},
{29,8172u},
{29,8173u},
{30,8198u},
{30,8256u},
{39,8228u},
{39,8229u},
{40,8149u},
{40,8150u},
{40,8151u},
{40,8152u},
{42,8000u},
{42,8001u},
{42,8002u},
{42,8004u},
{42,8005u},
{42,8006u},
{43,8015u},
{43,8505u},
{44,8345u},
{44,8346u},
{44,8347u},
{44,8348u},
{44,8349u},

{44,8352u},
{44,8353u},
{45,8350u},
{45,8351u},
{45,8353u},
{45,8354u},
{45,8355u},
{45,8356u},
{45,8358u},
{45,8359u},
{45,8360u},
{46,8200u},
{47,8159u},
{47,8160u},
{47,8161u},
{47,8162u},
{48,8089u},
{48,8090u},
};

static const TauntId kPowerLaunchers[]={
 {0,8503},{1,8594},{2,8416},{3,8258},{4,8606},{5,8487},{6,8303},
 {7,8465},{8,8462},{9,8568},{10,8350},{11,8499},{12,8414},{13,8515},
 {14,8360},{15,8435},{16,8600},{19,8556},{20,8505},{21,8441},{24,8533},
 {29,8285},{30,8269},{31,8219},{32,8299},{33,8234},{39,8213},{40,8121},
 {41,8225},{42,8158},{43,8487},{43,8534},{44,8282},{45,8041},{46,8045},
 {47,8157},{48,8086}
};
struct PowerLauncherScript {uint8_t character;uint32_t action;uint16_t number;};
static const PowerLauncherScript kPowerLauncherScripts[]={
 {0,8503,1113},
 {1,8594,1113},
 {2,8416,1073},
 {3,8258,1110},
 {4,8606,1103},
 {5,8487,1111},
 {6,8303,1100},
 {7,8465,1214},
 {8,8462,1002},
 {9,8568,1203},
 {10,8350,1111},
 {11,8499,1113},
 {12,8414,1203},
 {13,8515,1111},
 {14,8360,1101},
 {15,8435,1103},
 {16,8600,913},
 {19,8556,1101},
 {20,8505,1213},
 {21,8441,1203},
 {24,8533,1113},
 {29,8285,1923},
 {30,8269,1923},
 {31,8219,1533},
 {32,8299,1933},
 {33,8234,1723},
 {39,8213,1923},
 {40,8121,1143},
 {41,8225,2023},
 {42,8158,1923},
 {43,8487,1111},
 {43,8534,1412},
 {44,8282,1203},
 {45,8041,3203},
 {46,8045,1903},
 {47,8157,1951},
 {48,8086,1923},
};

BYTE* image=nullptr;
Logger destination=nullptr;
bool prepared=false,rendererArmed=false,alternatesInstalled=false;
BYTE* gateway=nullptr;
BYTE* resetGateway=nullptr;
uint32_t* alternateStorage=nullptr;
uint32_t* alternatePointers[16]{};
using RenderFunction=void(__cdecl*)(void*,void*,uint32_t);
RenderFunction originalRender=nullptr;
using ResetFunction=void(__cdecl*)();
ResetFunction originalReset=nullptr;
volatile LONG resetFull=1;
BYTE savedRender[6]{};
BYTE savedReset[8]{};
volatile LONG resetEpoch=0;
volatile LONG lastUnknownReset=0;
volatile LONG controlActive=0;
LONG observedResetEpoch=0;
SRWLOCK renderLock=SRWLOCK_INIT;
BYTE* At(uint32_t va) {return image+va-0x400000;}
uint32_t U32(const void* p) {uint32_t n;std::memcpy(&n,p,4);return n;}
void Put32(void* p,uint32_t n) {std::memcpy(p,&n,4);}

bool Accessible(const void* p,size_t count,bool writing=false) noexcept {
 uintptr_t first=reinterpret_cast<uintptr_t>(p),end=first+count;
 if(!first||!count||end<first)return false;
 while(first<end){

  MEMORY_BASIC_INFORMATION info{};
  if(FastRegion::Query(reinterpret_cast<void*>(first),&info,sizeof info)!=sizeof info)return false;
  if(info.State!=MEM_COMMIT||(info.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;
  DWORD mode=info.Protect&0xff;
  bool rw=mode==PAGE_READWRITE||mode==PAGE_WRITECOPY||mode==PAGE_EXECUTE_READWRITE||mode==PAGE_EXECUTE_WRITECOPY;
  if(!(rw||mode==PAGE_READONLY||mode==PAGE_EXECUTE_READ)||(writing&&!rw))return false;
  uintptr_t next=reinterpret_cast<uintptr_t>(info.BaseAddress)+info.RegionSize;
  if(next<=first)return false;first=next<end?next:end;
 }
 return true;
}
template<class T> bool Read(const void* p,T& value) noexcept {
 if(!Accessible(p,sizeof value))return false;std::memcpy(&value,p,sizeof value);return true;
}
bool Signature(uint32_t va,const unsigned char* bytes,size_t count) noexcept {
 return Accessible(At(va),count)&&std::memcmp(At(va),bytes,count)==0;
}

SRWLOCK queueLock=SRWLOCK_INIT;
char messages[128][256]{};
unsigned queueRead=0,queueWrite=0;
void Queue(const char* message) noexcept {
 if(!TryAcquireSRWLockExclusive(&queueLock))return;
 unsigned next=(queueWrite+1)%128;
 if(next!=queueRead){std::snprintf(messages[queueWrite],256,"%s",message);queueWrite=next;}
 ReleaseSRWLockExclusive(&queueLock);
}
void Notify(const char* message) noexcept {if(destination)destination(message);}
struct DiagnosticRateLimit {
 uint64_t last=0;bool initialized=false;
 bool Permit(uint64_t now) noexcept {
  if(initialized&&now-last<1000)return false;
  last=now;initialized=true;return true;
 }
};

DiagnosticRateLimit suppressedDrawLog;
DiagnosticRateLimit rejectedEventLog;
DiagnosticRateLimit kokoroActionLog;
#include "OnlineTrialRuntime.inc"

bool NativeGuards() noexcept {
 const BYTE render[]={0x55,0x8b,0xec,0x8b,0x45,0x0c,0x83,0xec,0x40,0x53,0x8b,0x58,0x08,0x56};
 const BYTE pb[]={0x55,0x8b,0xec,0x56,0x8b,0x75,0x08,0x8b,0xc6,0x69,0xc0,0xc8,0x06,0x00,0x00};
 const BYTE taunt[]={0x55,0x8b,0xec,0x8b,0x45,0x08,0x8b,0xc8,0x69,0xc9,0xc8,0x06,0x00,0x00};
 const BYTE pl[]={0x55,0x8b,0xec,0x8b,0x45,0x0c,0x8b,0x4d,0x08,0x50,0x51,0x68,0xae,0x01,0,0,0xe8,0x7b,0xd8,0xff,0xff,0x83,0xc4,0x0c};
 const BYTE active[]={0x55,0x8b,0xec,0x56,0x8b,0x75,0x08,0x83,0xfe,0x02,0x73,0x4f,0x8b,0x0d};
 BYTE opponent[]={0x55,0x8b,0xec,0xa1,0,0,0,0,0x56,0x85,0xc0,0x74,0x18,0x8b,0x4d,0x08};
 BYTE reset[]={0xf3,0x0f,0x10,0x05,0,0,0,0,0x53,0x56,0x57,0xb8,0,0,0,0,0xbe,0x02,0,0,0,0x33,0xc9,0xeb};
 Put32(opponent+4,reinterpret_cast<uint32_t>(At(0x24720d4)));
 Put32(reset+4,reinterpret_cast<uint32_t>(At(0x103aeb0)));
 Put32(reset+12,reinterpret_cast<uint32_t>(At(0x13d0588)));
 return Signature(0x76e960,render,sizeof render)&&Signature(0x6b3730,pb,sizeof pb)
  &&Signature(0x6b20c0,taunt,sizeof taunt)&&Signature(0x6b0330,pl,sizeof pl)&&Signature(0x982670,active,sizeof active)
  &&Accessible(At(0x98267e),4)&&U32(At(0x98267e))==reinterpret_cast<uint32_t>(At(0x24720d4))
  &&Signature(0x982640,opponent,sizeof opponent)&&Signature(0x68b380,reset,sizeof reset);
}

bool AssetName(uint32_t id,const char* expected,bool allowRedirected=false) noexcept {
 if(id>=0x705b)return false;
 const char* actual=nullptr;
 size_t length=std::strlen(expected)+1;
 if(!Read(At(0x11dacf0)+16*id,actual))return false;
 if(Accessible(actual,length)&&std::memcmp(actual,expected,length)==0)return true;
 const char* alias=StatefulReload::AlternateAlias(id,expected,allowRedirected);
 if(!alias)return false;
 length=std::strlen(alias)+1;
 return Accessible(actual,length)&&std::memcmp(actual,alias,length)==0;
}
bool AlternateGuard(bool installed) noexcept {

 const bool allowRedirected=installed&&InterlockedCompareExchange(&controlActive,0,0)!=0;
 for(unsigned i=0;i<16;i++){
  const auto& plan=kAlternates[i];uint32_t value=0;
  if(!Read(image+plan.fieldRva,value))return false;
  uint32_t wanted=plan.expectedPointerRva?reinterpret_cast<uint32_t>(image+plan.expectedPointerRva):0;
  if(installed&&!wanted)wanted=reinterpret_cast<uint32_t>(alternatePointers[i]);
  if(value!=wanted)return false;
  if(value){
   if(!Accessible(reinterpret_cast<void*>(value),32))return false;
   for(unsigned j=0;j<4;j++)if(U32(reinterpret_cast<BYTE*>(value)+j*8)!=plan.pairs[j].h||U32(reinterpret_cast<BYTE*>(value)+j*8+4)!=plan.pairs[j].hl)return false;
  } else if(!Accessible(image+plan.fieldRva,4))return false;
  for(const auto& pair:plan.pairs)if(!AssetName(pair.h,pair.hName,allowRedirected)||!AssetName(pair.hl,pair.hlName,allowRedirected))return false;
 }
 return true;
}

bool HeaderControlled(void* entity) noexcept {
 const BYTE* e=static_cast<const BYTE*>(entity);const BYTE* holder=nullptr;const BYTE* body=nullptr;
 if(!e||!Read(e+8,holder)||!Read(holder,body)||!Accessible(body,0x60)||std::memcmp(body,"TMC\0",4))return false;
 uint32_t size=U32(body+0x10);

 for(const auto& h:StateCostumes::Data::kModels)if(h.size==size&&StateCostumes::Data::Fnv64(body+0x50,16)==h.identityHash)return true;
 return false;
}
struct EntityInfo {BYTE* entity;BYTE* player;uint32_t identity0,identity4,action;uint8_t side,actorIndex,member,mode,selector;};
bool Entity(void* value,EntityInfo& out,bool requireActive) noexcept {
 BYTE* e=static_cast<BYTE*>(value);uint16_t kind=0;BYTE side=0,actorIndex=0,member=0;BYTE* player=nullptr;
 if(!e||!Accessible(e,0x60)||!Read(e+4,kind)||kind!=1||!Read(e+0x36,side)||side>=2
  ||!Read(e+0x34,actorIndex)||!Read(e+0x35,member)||!Read(e+0x40,player))return false;
 BYTE* expected=At(0x13d0540)+side*0x6c8;
 if(player!=expected||!Accessible(player,0x104)||player[0x28]!=actorIndex||player[0x29]!=member)return false;
 if(requireActive&&reinterpret_cast<void*(__cdecl*)(int)>(At(0x982670))(side)!=e)return false;
 out={e,player,U32(e),U32(e+4),U32(player+0x6c),side,actorIndex,member,player[0x70],player[0x100]};return true;
}
bool AllowedTaunt(uint32_t character,uint32_t action) noexcept {
 for(const auto& item:kTaunts)if(item.character==character&&item.action==action)return true;
 return false;
}
bool AllowedPL(uint32_t character,uint32_t action) noexcept {
 for(const auto& item:kPowerLaunchers)if(item.character==character&&item.action==action)return true;
 return false;
}

bool PLReaction(uint32_t action,uint8_t mode) noexcept {return (action==25633||action==25637)&&mode==9;}
bool PLContact(bool wasReaction,uint32_t victimAction,uint8_t victimMode,bool opponentValid,
 uint32_t attackerCharacter,uint32_t attackerAction,bool nativeProperty1AE) noexcept {
 return !wasReaction&&PLReaction(victimAction,victimMode)&&opponentValid
  &&nativeProperty1AE&&AllowedPL(attackerCharacter,attackerAction);
}
bool CliffAuthor(uint32_t side,uint32_t authorSide,uint32_t victimSide,
 uint32_t choice,uint32_t remaining,uint32_t action,uint8_t mode,bool paired) noexcept {

 return paired&&side<2&&authorSide==side&&victimSide<2&&victimSide!=side
  &&remaining>0&&remaining<=30&&mode==13
  &&((choice==2&&action==0x142)||(choice==3&&action==0x143));
}
bool nativeBurstEnabled=false;
bool nativeSoundEnabled=false;
#ifdef PREFLIGHT_TOOL

struct OfflineEventSinks {
 bool enabled=false;
 void (*beforeSubmit)() noexcept=nullptr;
 unsigned sounds[2]{},schoolgirl[2]{},destruction[2]{},wrongDestructionHeader=0;
 void* soundActor[2]{},*schoolgirlActor[2]{},*destructionActor[2]{};
};
OfflineEventSinks offlineEventSinks;
#endif
bool BurstPolicy(bool enabled,bool changed,bool offline,bool active,bool recognized,bool supported) noexcept {
 return enabled&&changed&&offline&&active&&recognized&&supported;
}
#include "OnlineEffectsRuntime.inc"
#include "OriginalSchoolgirlParticles.inc"
#include "OriginalDestructionParticles.inc"
void TransitionBurst(const EntityInfo& actor,bool changed,bool schoolgirl=false) noexcept {
 if(!StateCostumes::IsTransitionEnabled(schoolgirl))return;
 if(!nativeBurstEnabled||!changed)return;
 NativeCostumeEffectScope effects;
 if(!effects)return;

 const BYTE factory[]={0x55,0x8b,0xec,0x6a,0xff};
 const BYTE position[]={0x55,0x8b,0xec,0x83,0xec,0x54};
 const BYTE allowed[]={0x55,0x8b,0xec,0x6a,0xff};
 uint32_t phase=0;
 EntityInfo live{};
 const bool active=Read(At(0x1487a4c),phase)&&phase==4&&Entity(actor.entity,live,true)
  &&live.identity0==actor.identity0&&live.identity4==actor.identity4;
 const bool contextAllowed=OnlineStateAllowed();
 const bool supported=Signature(0x8a6f00,factory,sizeof factory)
  &&Signature(0x8a6ae0,factory,sizeof factory)&&Signature(0x8a3e60,position,sizeof position)
  &&Signature(0x8a34c0,allowed,sizeof allowed);
 if(!BurstPolicy(nativeBurstEnabled,changed,contextAllowed,active,HeaderControlled(actor.entity),supported)){
  Queue("Transition burst refused: context, active actor, model or native signature guard.");return;
 }
#ifdef PREFLIGHT_TOOL
 if(offlineEventSinks.enabled){
  if(offlineEventSinks.beforeSubmit)offlineEventSinks.beforeSubmit();
  if(schoolgirl){++offlineEventSinks.schoolgirl[actor.side];offlineEventSinks.schoolgirlActor[actor.side]=actor.entity;}
  else{
   ++offlineEventSinks.destruction[actor.side];offlineEventSinks.destructionActor[actor.side]=actor.entity;
   if(!DestructionHeaderControlled(actor.entity))++offlineEventSinks.wrongDestructionHeader;
  }
  return;
 }
#endif
 BYTE* clock=nullptr;uint32_t tick=0;BYTE* resources=nullptr;BYTE* root=nullptr;
 if(!Read(At(0x246d008),clock)||!Read(clock,tick)
  ||!Read(At(0x245385c),resources)||!Read(resources,root)||!root
  ||!Accessible(At(0x246c510),8,true)){
  Queue("Transition burst refused: effect clock, resource manager or native effect list unavailable.");return;
 }
 if(!reinterpret_cast<BYTE(__cdecl*)()>(At(0x8a34c0))()){
  Queue("Transition burst refused: native visual-effects mode predicate.");return;
 }
 alignas(16) float position4[4]={};
 reinterpret_cast<void(__cdecl*)(float*,void*,int)>(At(0x8a3e60))(position4,actor.entity,2);
 for(float v:position4)if(!std::isfinite(v)||std::fabs(v)>10000.0f){
  Queue("Transition burst refused: invalid native bone position.");return;
 }
 if(schoolgirl&&TryOriginalSchoolgirl(actor,tick,root))return;
 if(!schoolgirl&&TryOriginalDestruction(actor,tick,root))return;

 if(IsNetworkContext()){
  Queue("Online costume particles refused: supported port resources unavailable; unisolated fallback not submitted.");return;
 }

 struct Descriptor {void* owner;uint32_t tick;float* position;};
 Descriptor descriptor{actor.entity,tick,position4};void* result[2]={};
 using Factory=void(__thiscall*)(void*,void**,Descriptor*,void*);
 void* manager=At(0x246c510);
 reinterpret_cast<Factory>(At(0x8a6f00))(manager,result,&descriptor,manager);
 char line[192];std::snprintf(line,sizeof line,
  "OFFLINE experimental native transition burst actor=%p result=%p node=%p; visual resemblance unvalidated",
  actor.entity,result[0],result[1]);Queue(line);
}

static BYTE transformationSoundScript[]={0x0b,0x00,0x05,0x00,0x35,0xc9,0x00,0x00,0x1e};
bool AudioRegion(const void* pointer,size_t length,DWORD type,const void* allocation,bool executable) noexcept {
 if(!Accessible(pointer,length))return false;
 MEMORY_BASIC_INFORMATION region{};
 if(VirtualQuery(pointer,&region,sizeof region)!=sizeof region||region.Type!=type
   ||region.State!=MEM_COMMIT||region.AllocationBase!=allocation)return false;
 const uintptr_t first=reinterpret_cast<uintptr_t>(pointer);
 const uintptr_t end=reinterpret_cast<uintptr_t>(region.BaseAddress)+region.RegionSize;
 if(end<first||end-first<length||(region.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;
 const DWORD mode=region.Protect&0xff;
 return executable?(mode==PAGE_EXECUTE_READ||mode==PAGE_EXECUTE_READWRITE||mode==PAGE_EXECUTE_WRITECOPY)
  :(mode==PAGE_READONLY||mode==PAGE_READWRITE||mode==PAGE_WRITECOPY);
}
bool SoundSubmissionSignature(const BYTE* native,unsigned length) noexcept {
 if(Signature(0x957c40,native,length))return true;

 using namespace AudioBridgeValidation;
 BYTE* entry=At(0x957c40);
 if(!Accessible(entry,length)||!Entry(entry,length,native,length)){
  Queue("Audio bridge rejected: patched entry or native continuation differs.");return false;
 }
 const uint32_t entryAddress=uint32_t(reinterpret_cast<uintptr_t>(entry));
 BYTE* target=reinterpret_cast<BYTE*>(uintptr_t(Relative(entryAddress+5,AudioBridgeValidation::U32(entry+1))));
 HMODULE module=nullptr;
 if(!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
   reinterpret_cast<LPCSTR>(target),&module)||!module
   ||!AudioRegion(target,58,MEM_IMAGE,module,true)){
  Queue("Audio bridge rejected: target is not executable code in a loaded image.");return false;
 }
 Kind kind=Classify(target,58);
 if(kind==Kind::Unsupported){Queue("Audio bridge rejected: unsupported wrapper instructions.");return false;}
 if(kind==Kind::RollbackChainedFilter){
  if(module!=GetModuleHandleA("DOA5Rollback.asi")){Queue("Audio bridge rejected: chained wrapper is not owned by DOA5Rollback.");return false;}
  BYTE* filter=target+15+int32_t(AudioBridgeValidation::U32(target+11));
  BYTE *chainSlot=nullptr,*resumeSlot=nullptr,*chain=nullptr,*resume=nullptr;
  if(!AudioRegion(filter,1,MEM_IMAGE,module,true)||!Read(target+29,chainSlot)||!Read(target+44,resumeSlot)
    ||(uintptr_t(chainSlot)&3)||(uintptr_t(resumeSlot)&3)
    ||!AudioRegion(chainSlot,4,MEM_IMAGE,module,false)||!AudioRegion(resumeSlot,4,MEM_IMAGE,module,false)
    ||!Read(chainSlot,chain)||!Read(resumeSlot,resume)){
   Queue("Audio bridge rejected: chained rollback filter/slots invalid.");return false;
  }
  if(!chain){
   if(resume!=entry+6){Queue("Audio bridge rejected: chained rollback native fallback differs.");return false;}
   Queue("Audio bridge validated: current rollback native fallback.");return true;
  }

  HMODULE tools=GetModuleHandleA("DOA5Tools.asi");
  if(!tools||!AudioRegion(chain,43,MEM_IMAGE,tools,true)||Classify(chain,43)!=Kind::ToolsMenuFilter){
   Queue("Audio bridge rejected: rollback continuation is not a recognized DOA5Tools menu filter.");return false;
  }
  target=chain;module=tools;kind=Kind::ToolsMenuFilter;
 }
 BYTE* slot=nullptr;BYTE* continuation=nullptr;
 const size_t slotOffset=kind==Kind::RollbackFilter?35:31;
 if(!Read(target+slotOffset,slot)||(reinterpret_cast<uintptr_t>(slot)&3)
   ||!AudioRegion(slot,4,MEM_IMAGE,module,false)||!Read(slot,continuation)){
  Queue("Audio bridge rejected: continuation slot is not aligned data in the wrapper image.");return false;
 }
 if(kind==Kind::RollbackFilter){
  const uint32_t targetAddress=uint32_t(reinterpret_cast<uintptr_t>(target));
  BYTE* filter=reinterpret_cast<BYTE*>(uintptr_t(Relative(targetAddress+15,AudioBridgeValidation::U32(target+11))));
  if(!AudioRegion(filter,1,MEM_IMAGE,module,true)||continuation!=entry+6){
   Queue("Audio bridge rejected: rollback filter owner or native continuation differs.");return false;
  }
 }else{
  BYTE* flag=nullptr;

  if(!Read(target+2,flag)||(reinterpret_cast<uintptr_t>(flag)&3)
    ||!AudioRegion(flag,4,MEM_IMAGE,module,false)
    ||!AudioRegion(continuation,11,MEM_PRIVATE,continuation,true)
    ||!ToolsTrampoline(continuation,11,uint32_t(reinterpret_cast<uintptr_t>(continuation)),entryAddress,native,length)){
   Queue("Audio bridge rejected: menu flag owner or native trampoline differs.");return false;
  }
 }
 char diagnostic[192];std::snprintf(diagnostic,sizeof diagnostic,
  "Audio bridge validated kind=%s owner=%p target=%p offset=%X slot=%p continuation=%p",
  kind==Kind::RollbackFilter?"rollback":"tools-menu",module,target,
  unsigned(reinterpret_cast<uintptr_t>(target)-reinterpret_cast<uintptr_t>(module)),slot,continuation);Queue(diagnostic);
 return true;
}
void TransitionSound(const EntityInfo& actor,bool changed) noexcept {
 if(!StateCostumes::IsTransitionEnabled(true))return;
 if(!nativeSoundEnabled||!changed)return;
 NativeCostumeEffectScope effects;
 if(!effects)return;
 EntityInfo live{};uint32_t phase=0;
 const bool active=Read(At(0x1487a4c),phase)&&phase==4&&Entity(actor.entity,live,true)
  &&live.identity0==actor.identity0&&live.identity4==actor.identity4;
 const bool contextAllowed=OnlineStateAllowed();
 const BYTE submit[]={0x55,0x8b,0xec,0x83,0xec,0x0c,0x57,0x8b,0x7d,0x08,0x83,0xff,0xff};
 const bool recognized=HeaderControlled(actor.entity);
 const bool signature=SoundSubmissionSignature(submit,sizeof submit);
 if(!BurstPolicy(nativeSoundEnabled,changed,contextAllowed,active,recognized,signature)){
  char reason[192];std::snprintf(reason,sizeof reason,
   "Transition sound guard refused: enabled=%u changed=%u context=%u active=%u model=%u signature=%u phase=%u",
   unsigned(nativeSoundEnabled),unsigned(changed),unsigned(contextAllowed),unsigned(active),unsigned(recognized),unsigned(signature),unsigned(phase));
  Queue(reason);return;
 }
#ifdef PREFLIGHT_TOOL
 if(offlineEventSinks.enabled){
  if(offlineEventSinks.beforeSubmit)offlineEventSinks.beforeSubmit();
  ++offlineEventSinks.sounds[actor.side];offlineEventSinks.soundActor[actor.side]=actor.entity;return;
 }
#endif
 BYTE* aliases=nullptr;BYTE* cues=nullptr;BYTE* definitions=nullptr;BYTE* original=nullptr;
 BYTE* definition=nullptr;uint32_t mapped=0,group=0,count=0,alternatives=0;
 uint16_t sid=0,bank=0,sample=0,alternativeCount=0;
 if(!Read(At(0x247121c),aliases)||!Read(aliases+0x117*4,mapped)||mapped!=0x1df5
  ||!Read(At(0x2470824),cues)||!Accessible(cues+mapped*8,8,true)
  ||!Read(cues+mapped*8,original)||!Accessible(original,1)||original[0]!=0x1e
  ||!Read(cues+mapped*8+4,group)||group!=121
  ||!Read(At(0x2470828),definitions)||!Read(At(0x247082c),count)||count<=0x35c9
  ||!Read(definitions+0x35c9*8,definition)||!Read(definition+0x1a,sid)||sid!=0x35c9
  ||!Read(definition+0x1c,bank)||bank!=1||!Read(definition+0x1e,sample)||sample!=318
  ||!Read(definition+0x14,alternatives)||alternatives
  ||!Read(definition+0x18,alternativeCount)||alternativeCount){
  Queue("Transition sound refused: native alias, empty cue or sound definition differs.");return;
 }
 auto entry=reinterpret_cast<PVOID volatile*>(cues+mapped*8);

 if(InterlockedCompareExchangePointer(entry,transformationSoundScript,original)!=original)return;

 const uint32_t result=reinterpret_cast<uint32_t(__cdecl*)(uint32_t,void*)>(At(0x957c40))(0x117,nullptr);
 const bool restored=InterlockedCompareExchangePointer(entry,original,transformationSoundScript)==transformationSoundScript;
 char line[192];std::snprintf(line,sizeof line,
  "Transformation sound event=117 cue=1DF5 sound=35C9 bank=1 sample=318 handle=%08X cue_restored=%u",
  unsigned(result),unsigned(restored));Queue(line);
}
static uint8_t ps4LastC[2]={0,0};static int32_t ps4LastA[2]={0,0};
#include "RollbackCostumes.inc"
extern "C" void __cdecl DangerCheckEntry() noexcept;

static bool NoGuessTransform(void*) noexcept {return false;}
extern "C" void __cdecl D1Tick() noexcept;
struct Edge {BYTE* entity{};uint32_t identity0{},identity4{};bool pb{},taunt{},plReaction{},cliff{},cb{},cliffVictim{},cliffAuthor{};};
Edge edges[2]{};
SRWLOCK eventLock=SRWLOCK_INIT;
void Events(void* entity) noexcept {

 uint32_t phase=0;
 if(!Read(At(0x1487a4c),phase)||phase!=4)return;
 if(IsNetworkContext()&&!OnlineStateAllowed())return;

 DangerCheckEntry();
 D1Tick();
 if(!TryAcquireSRWLockExclusive(&eventLock))return;
 EntityInfo actor{};
 if(!Entity(entity,actor,true)){
  if(HeaderControlled(entity)&&rejectedEventLog.Permit(GetTickCount64()))
   Queue("Costume diagnostic: recognized body rejected by native active-actor identity/context guard.");
  ReleaseSRWLockExclusive(&eventLock);return;
 }
 Edge& edge=edges[actor.side];
 if(edge.entity!=actor.entity||edge.identity0!=actor.identity0||edge.identity4!=actor.identity4)
  edge={actor.entity,actor.identity0,actor.identity4,false,false,false,false,false,false,false};
 uint32_t cliffChoice=0,cliffRemaining=0,cliffAuthor=2,cliffVictim=2;
 bool cliff=false;
 static uint32_t lastIgnoredCliff[2]={0xffffffff,0xffffffff};
 if(Read(At(0x13ce9f8),cliffChoice)&&Read(At(0x13cea00),cliffRemaining)
  &&Read(At(0x13cea08),cliffAuthor)&&Read(At(0x13cea04),cliffVictim)
  &&CliffAuthor(actor.side,cliffAuthor,cliffVictim,cliffChoice,cliffRemaining,actor.action,actor.mode,true)){
  void* opponent=reinterpret_cast<void*(__cdecl*)(int)>(At(0x982670))(cliffVictim);
  EntityInfo victim{};
  const bool paired=Entity(opponent,victim,true)&&victim.side==cliffVictim;
  cliff=CliffAuthor(actor.side,cliffAuthor,cliffVictim,cliffChoice,cliffRemaining,actor.action,actor.mode,paired);
 }
 if(cliff&&!edge.cliff){
  const bool changed=NoGuessTransform(actor.entity);
  TransitionSound(actor,changed);
  TransitionBurst(actor,changed,true);
  char line[192];std::snprintf(line,sizeof line,"Native cliffhanger author edge: side=%u choice=%u action=%u transform_applied=%u",unsigned(actor.side),unsigned(cliffChoice),unsigned(actor.action),unsigned(changed));Queue(line);
 }
 edge.cliff=cliff;
 {

  static uint32_t lastAction[2]={0xffffffff,0xffffffff};static uint8_t lastMode[2]={0xff,0xff};
  static DWORD lastRead=0;static bool enabled=true;const DWORD now=GetTickCount();
  if(!lastRead||now-lastRead>2000){lastRead=now;char ini[MAX_PATH]{};GetModuleFileNameA(nullptr,ini,MAX_PATH);
   char* e=std::strrchr(ini,92);if(e){std::snprintf(ini,MAX_PATH,"%s",Extra_Ini());enabled=GetPrivateProfileIntA("StatefulCostumesDebug","ActionLog",0,ini)!=0;}}
  if(enabled&&(lastAction[actor.side]!=actor.action||lastMode[actor.side]!=actor.mode)){
   lastAction[actor.side]=actor.action;lastMode[actor.side]=actor.mode;
   char l[128];std::snprintf(l,sizeof l,"ACTION side=%u character=%u action=%u mode=%u",
    unsigned(actor.side),unsigned(actor.identity0),unsigned(actor.action),unsigned(actor.mode));Queue(l);
  }
 }
 const bool pb=reinterpret_cast<BYTE(__cdecl*)(int)>(At(0x6b3730))(actor.side)!=0;

 const bool allowed=AllowedTaunt(actor.identity0,actor.action);
 const bool taunt=allowed&&reinterpret_cast<BYTE(__cdecl*)(int)>(At(0x6b20c0))(actor.side)!=0;
 if(actor.identity0==10&&kokoroActionLog.Permit(GetTickCount64())){
  char line[256];std::snprintf(line,sizeof line,
   "Kokoro diagnostic actor=%p side=%u action=%u mode=%u selector=%u controlled=%u allowlist=%u native_taunt=%s",
   actor.entity,unsigned(actor.side),unsigned(actor.action),unsigned(actor.mode),unsigned(actor.selector),
   unsigned(HeaderControlled(entity)),unsigned(allowed),allowed?(taunt?"true":"false"):"not_called");
  Queue(line);
 }
 if(pb&&!edge.pb){
  const bool authorChanged=NoGuessTransform(actor.entity);
  TransitionSound(actor,authorChanged);
  TransitionBurst(actor,authorChanged,true);

  Queue("Native PB rising edge: author Schoolgirl considered; victim break left to PS4 script rule (0xCF/danger).");
 }

 if(taunt&&!edge.taunt){
  const bool applied=NoGuessTransform(actor.entity);
  TransitionSound(actor,applied);
  TransitionBurst(actor,applied,true);
  char line[192];std::snprintf(line,sizeof line,
   "Native taunt rising edge: actor=%p character=%u action=%u transform_applied=%u",
   actor.entity,unsigned(actor.identity0),unsigned(actor.action),unsigned(applied));Queue(line);
 }

 {static uint32_t vfLast[2]={0,0};
  const uint32_t id=actor.identity0;const bool vf=id==31||id==32||id==33||id==41;
  static ULONGLONG r1Read=0;static bool r1=false;const ULONGLONG t=GetTickCount64();
  if(!r1Read||t-r1Read>2000){r1Read=t;char ini[MAX_PATH]{};GetModuleFileNameA(nullptr,ini,MAX_PATH);
   char* e=std::strrchr(ini,92);if(e){std::snprintf(ini,MAX_PATH,"%s",Extra_Ini());r1=GetPrivateProfileIntA("StatefulCostumesVF","R1",0,ini)!=0;}}
  if(!IsNetworkContext()&&r1&&vf&&actor.action==176&&vfLast[actor.side]!=176&&StateCostumes::IsSchoolgirl(actor.entity)){
   TransitionSound(actor,true);TransitionBurst(actor,true,true);StateCostumes::OnToggleSchoolgirl(actor.entity);
   char l[96];std::snprintf(l,sizeof l,"VF H+P+K bascule side=%u character=%u",unsigned(actor.side),unsigned(id));Queue(l);}
  vfLast[actor.side]=actor.action;}
 const bool plReaction=PLReaction(actor.action,actor.mode);
 if(plReaction&&!edge.plReaction){
  void* other=reinterpret_cast<void*(__cdecl*)(const void*)>(At(0x982640))(actor.entity+0x58);
  EntityInfo attacker{};
  const bool paired=Entity(other,attacker,true)&&attacker.side!=actor.side;
  if(paired&&AllowedPL(attacker.identity0,attacker.action)){

   int32_t actionCopy=static_cast<int32_t>(attacker.action);
   const bool property=reinterpret_cast<BYTE(__cdecl*)(int32_t*,uint32_t)>(At(0x6b0330))(&actionCopy,attacker.selector)!=0;
   if(PLContact(edge.plReaction,actor.action,actor.mode,true,attacker.identity0,attacker.action,property)){
    ConfirmPLContact(attacker.entity);
    const bool victimChanged=StateCostumes::OnNewConfirmedHit(actor.entity);TransitionBurst(actor,victimChanged);rb::PinBreaks(actor.entity);
    Queue("Committed PL contact: author toggle and destructible victim considered.");
   }
  }
 }

 const bool cb=actor.mode==8&&(actor.action==24661||actor.action==24665);
 if(actor.mode==8&&actor.action/10==2466&&!cb&&!edge.cb){
  char unknown[128];std::snprintf(unknown,sizeof unknown,"CB diagnostic: unknown 2466x reaction %u/8 side=%u character=%u",unsigned(actor.action),unsigned(actor.side),unsigned(actor.identity0));Queue(unknown);
 }
 if(cb&&!edge.cb){
  const bool broken=StateCostumes::OnNewConfirmedHit(actor.entity);TransitionBurst(actor,broken);rb::PinBreaks(actor.entity);

  char line[160];std::snprintf(line,sizeof line,"Native Critical Burst edge (victim reaction %u/8): victim side=%u character=%u broken=%u",
   unsigned(actor.action),unsigned(actor.side),unsigned(actor.identity0),unsigned(broken));Queue(line);
 }
 edge.cb=cb;
 const bool cliffAuthorMove=false;
 edge.cliffAuthor=cliffAuthorMove;

 edge.pb=pb;edge.taunt=taunt;edge.plReaction=plReaction;
 ReleaseSRWLockExclusive(&eventLock);
}
void __cdecl RenderBridge(void* entity,void* list,uint32_t arg3) noexcept {

 if(InterlockedCompareExchange(&controlActive,0,0)==0){originalRender(entity,list,arg3);return;}
 const bool controlled=HeaderControlled(entity);

 if(!TryAcquireSRWLockExclusive(&renderLock)){
  if(!controlled)originalRender(entity,list,arg3);
  return;
 }
 ObserveCostumeEffectEngineThread();
 const LONG currentEpoch=InterlockedCompareExchange(&resetEpoch,0,0);
 if(currentEpoch!=observedResetEpoch){
  {const LONG u=InterlockedExchange(&lastUnknownReset,0);if(u){char l[96];std::snprintf(l,sizeof l,"Reset site inconnu 0x%08X : etat des costumes conserve.",unsigned(u));Queue(l);}}
  const bool fullReset=InterlockedExchange(&resetFull,0)!=0;
  if(!StateCostumes::ResetForRound(!fullReset)){
   if(fullReset)InterlockedExchange(&resetFull,1);

   ReleaseSRWLockExclusive(&renderLock);
   if(!controlled)originalRender(entity,list,arg3);
   return;
  }
  edges[0]={};edges[1]={};observedResetEpoch=currentEpoch;rb::Clear();
  Queue("Completed native round/training reset observed: controlled baselines restored; event edges cleared.");
 }
 ObserveTimerDomain();
 struct VisibilityScope {
  StateCostumes::RenderScope value{};
  bool active=false;
  void Restore() noexcept {
   if(!active)return;
   active=false;
   if(!StateCostumes::EndRenderScope(value))Queue("Online visibility restore refused: native entity/flags identity changed during render.");
  }
  ~VisibilityScope(){Restore();}
 } visibility;
 if(controlled&&IsNetworkContext()){
  if(!StateCostumes::BeginRenderScope(entity,visibility.value)){
   Queue("Suppressed online controlled draw: native visibility snapshot unavailable.");
   ReleaseSRWLockExclusive(&renderLock);return;
  }
  visibility.active=true;
 }
 if(controlled){
  if(!StateCostumes::OnBeforeRender(entity)){

   if(suppressedDrawLog.Permit(GetTickCount64()))
    Queue("Suppressed controlled body draw: state validation/ownership refused (maximum one diagnostic per second).");
   visibility.Restore();ReleaseSRWLockExclusive(&renderLock);return;
  }
 }
 Events(entity);
 originalRender(entity,list,arg3);
 visibility.Restore();
 ReleaseSRWLockExclusive(&renderLock);
}
__attribute__((noinline,optimize("no-omit-frame-pointer"))) void __cdecl ResetBridge() noexcept {
 bool resetOnline=false;

 {
  const uintptr_t ra=reinterpret_cast<uintptr_t>(__builtin_return_address(0));
  const uint32_t va=image?uint32_t(ra-reinterpret_cast<uintptr_t>(image)+0x400000):0;

  if(va==0x68b8e5||va==0x8088f0)InterlockedExchange(&resetFull,1);
  else if(va!=0x809bef&&va!=0x80bb8e)InterlockedExchange(&lastUnknownReset,LONG(va));
  if(va==0x68b8e5||va==0x8088f0||va==0x809bef||va==0x80bb8e)
   resetOnline=true;
 }

 originalReset();
 const LONG completedEpoch=InterlockedIncrement(&resetEpoch);
 if(resetOnline)InterlockedExchange(&onlineResetPending,completedEpoch);
}
}

void EnableNativeTransitionBurst(bool enabled) noexcept {nativeBurstEnabled=enabled;}
void EnableNativeTransitionSound(bool enabled) noexcept {nativeSoundEnabled=enabled;}
void EnableOriginalSchoolgirlParticles(bool enabled) noexcept {originalSchoolgirlEnabled=enabled;}
void EnableOnlineCostumeTrial(bool enabled,bool rollbackDisabled) noexcept {
 onlineTrialEnabled=enabled;rollbackConfiguredDisabled=rollbackDisabled;
 Queue(enabled?"ONLINE TRIAL full effects: render-scoped forms plus particles/audio with private RNG streams; native lobby and known inactive rollback only; peer sync unverified.":"ONLINE TRIAL disabled.");
}
void EnableRollbackCostumes(bool enabled,bool laboTest) noexcept {
 rollbackCostumesEnabled=enabled;rb::laboTest=laboTest;
 if(laboTest)Queue("ROLLBACK costumes : [Debug] RollbackLabo=1, suivi des recalculs aussi dans le labo hors ligne du rollback.");
 Queue(enabled?"ROLLBACK costumes: admis quand DOA5Rollback est actif en salon ; evenements recalcules dedoublonnes (script + image de debut), images predites annulees retablies.":"ROLLBACK costumes: coupe (EnableRollbackCostumes=0).");
}
bool Prepare(BYTE* base,Logger logger) noexcept {
 if(prepared)return image==base;
 image=base;destination=logger;
 if(!image){Notify("Stateful refused: game image is missing.");return false;}
 if(!NativeGuards()){Notify("Stateful refused: renderer or event signatures differ.");return false;}
 if(!AlternateGuard(false)){Notify("Stateful refused: ALT pointers, tables or asset names differ.");return false;}
 alternateStorage=static_cast<uint32_t*>(VirtualAlloc(nullptr,16*32,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
 if(!alternateStorage){Notify("Stateful refused: persistent ALT allocation failed.");return false;}
 for(unsigned i=0;i<16;i++)if(!kAlternates[i].expectedPointerRva){
  alternatePointers[i]=alternateStorage+i*8;
  for(unsigned j=0;j<4;j++){alternatePointers[i][j*2]=kAlternates[i].pairs[j].h;alternatePointers[i][j*2+1]=kAlternates[i].pairs[j].hl;}
 }
 DWORD old=0;
 if(!VirtualProtect(alternateStorage,16*32,PAGE_READONLY,&old)){VirtualFree(alternateStorage,0,MEM_RELEASE);alternateStorage=nullptr;return false;}
 StateCostumes::SetLogger(QueueCostumeState);
 StateCostumes::SetRenderOnlyPredicate(IsNetworkContext);prepared=true;
 Notify("Stateful prototype prepared: 14 new persistent ALT tables; 2 existing preserved; PB/taunt, bounded victim PL reaction, native round/training reset observer; runtime untested.");
 return true;
}

static BYTE* dangerGateway=nullptr;
static void PS4Break(unsigned side,const char* reason,unsigned c,int a) noexcept {
 if(InterlockedCompareExchange(&controlActive,0,0)==0)return;
 if(IsNetworkContext()&&!OnlineStateAllowed())return;
 uint32_t phase=0;if(!Read(At(0x1487a4c),phase)||phase!=4)return;
 void* e=reinterpret_cast<void*(__cdecl*)(int)>(At(0x982670))(int(side));
 EntityInfo info{};if(!Entity(e,info,true))return;

 const bool rbLogic=rb::Active()&&rb::LogicThread();const uint32_t rbFrame=rbLogic?rb::Frame():0;
 if(rbLogic&&rb::BreakSeen(side,rbFrame))return;
 const bool broken=StateCostumes::OnNewConfirmedHit(info.entity);
 if(rbLogic){if(broken)rb::RecordBreak(side,rbFrame,info.entity);}
 else rb::PinBreaks(info.entity);
 if(broken)TransitionBurst(info,true);
 if(rb::Active()&&!broken)return;
 char line[192];std::snprintf(line,sizeof line,"PS4 regle casse (%s C=%u A=%d) side=%u character=%u action=%u mode=%u broken=%u",
  reason,c,a,side,unsigned(info.identity0),unsigned(info.action),unsigned(info.mode),unsigned(broken));Queue(line);
}
static void PS4CheckSides() noexcept {
 if(InterlockedCompareExchange(&controlActive,0,0)==0)return;
 const bool network=IsNetworkContext();
 if(network&&!OnlineStateAllowed())return;
 for(unsigned side=0;side<2;++side){
  uint8_t c=0;int32_t a=0;
  if(!Read(At(0x13d0604+side*0x6c8),c)||!Read(At(0x13d05cc+side*0x6c8),a))continue;

  if(!network&&(c==1||c==4||c==7)){
   uint32_t vt=0,x=0,z=0;
   if(Read(At(0x24722ac),vt)&&vt&&Read(At(0x13cbaf0+side*0x180),x)&&Read(At(0x13cbaf8+side*0x180),z)
    &&reinterpret_cast<int(__cdecl*)(uint32_t,uint32_t)>(At(0x98e680))(x,z)==7)c=c==1?5:6;
  }
  const bool cHit=c==1||c==4||c==5||c==6, aHit=a==4;
  if((cHit&&c!=ps4LastC[side])||(aHit&&a!=ps4LastA[side]))PS4Break(side,aHit?"A":"C",c,a);
  ps4LastC[side]=c;ps4LastA[side]=a;
 }
}
extern "C" void __cdecl DangerCheckEntry() noexcept {if(rb::LogicThread())rb::LogicObserve();PS4CheckSides();}
static void __fastcall ScriptBreak(void* actor) noexcept {
 if(InterlockedCompareExchange(&controlActive,0,0)==0)return;
 rb::LogicObserve();
 if(IsNetworkContext()&&!OnlineStateAllowed())return;
 EntityInfo info{};if(!Entity(actor,info,true))return;
 PS4Break(info.side,"script 0xCF",0,0);
}
static bool ArmPS4Rule() noexcept {
 const BYTE danger[]={0x55,0x8b,0xec,0x83,0xec,0x3c,0xa1};
 const BYTE cf[]={0x8b,0xc8,0xe8};
 if(!Signature(0x740f60,danger,sizeof danger)||!Signature(0x6f5720,cf,sizeof cf)
  ||At(0x6f5727)+*reinterpret_cast<int32_t*>(At(0x6f5723))!=At(0xc1a530)){Notify("PS4 regle : signatures differentes, accroches non installees.");return false;}
 dangerGateway=static_cast<BYTE*>(VirtualAlloc(nullptr,64,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE));
 if(!dangerGateway)return false;
 BYTE* g=dangerGateway;*g++=0x9c;*g++=0x60;*g++=0xe8;Put32(g,uint32_t(reinterpret_cast<BYTE*>(&RuntimeGlue::DangerCheckEntry)-(g+4)));g+=4;
 *g++=0x61;*g++=0x9d;std::memcpy(g,At(0x740f60),6);g+=6;*g++=0xe9;Put32(g,uint32_t(At(0x740f66)-(g+4)));g+=4;
 FlushInstructionCache(GetCurrentProcess(),dangerGateway,64);
 DWORD old=0,ign=0;
 BYTE patch[6]={0xe9,0,0,0,0,0x90};Put32(patch+1,uint32_t(dangerGateway-(At(0x740f60)+5)));
 if(!VirtualProtect(At(0x740f60),6,PAGE_EXECUTE_READWRITE,&old))return false;
 std::memcpy(At(0x740f60),patch,6);VirtualProtect(At(0x740f60),6,old,&ign);FlushInstructionCache(GetCurrentProcess(),At(0x740f60),6);
 if(!VirtualProtect(At(0x6f5722),5,PAGE_EXECUTE_READWRITE,&old))return false;
 Put32(At(0x6f5723),uint32_t(reinterpret_cast<BYTE*>(&ScriptBreak)-(At(0x6f5722)+5)));
 VirtualProtect(At(0x6f5722),5,old,&ign);FlushInstructionCache(GetCurrentProcess(),At(0x6f5722),5);
 Notify("PS4 regle armee : danger_check 0x740F60 (A==4 / C 1,4,5,6) + commande de script 0xCF 0x6F5722.");return true;
}

#include "PS4D1Table.inc"
static BYTE* scriptGateway=nullptr;static BYTE* interpGateway=nullptr;
struct ScriptRun {volatile uint32_t number{};volatile uint32_t base{};volatile uint32_t maxOff{};volatile uint32_t serial{};
 volatile uint32_t startF{};
 uint32_t seenSerial{};uint32_t fired{};uint32_t flipped{};ULONGLONG at[32]{};rb::Handle h[32]{};};
static ScriptRun scriptRun[2]{};
struct ScriptEnd {volatile uint32_t number{},maxOff{},serial{},startF{};volatile bool based{};};
static ScriptEnd scriptEnd[2]{};
static volatile uint32_t scriptRing[64]{};static volatile LONG scriptRingHead=0;

static volatile uint32_t diagNumber[2]={0,0},diagMax[2]={0,0},diagSerial[2]={0,0};
extern "C" void __cdecl ScriptStart(int side,uint32_t number) noexcept {
 if(side<0||side>1)return;ScriptRun& r=scriptRun[side];
 uint32_t rbF=0;if(rb::Active()){rb::LogicObserve();rbF=rb::Frame();rb::ScriptStarted(unsigned(side),number,rbF);}
 if(r.number>=1500&&r.number<2600){diagNumber[side]=r.number;diagMax[side]=r.maxOff;diagSerial[side]=diagSerial[side]+1;}
 {const LONG h=InterlockedIncrement(&scriptRingHead)-1;scriptRing[h&63]=(uint32_t(side)<<16)|(number&0xffff);}
 scriptEnd[side].number=r.number;scriptEnd[side].maxOff=r.maxOff;scriptEnd[side].based=r.base!=0;scriptEnd[side].serial=r.serial;scriptEnd[side].startF=r.startF;
 r.number=number;r.base=0;r.maxOff=0;r.startF=rbF;r.serial=r.serial+1;
}

static volatile uint32_t interpSteps[2]={0,0};
extern "C" void __cdecl InterpStep(uint32_t side) noexcept {
 if(side>1)return;interpSteps[side]=interpSteps[side]+1;rb::LogicObserve();ScriptRun& r=scriptRun[side];if(r.number>=0x2000)return;
 const uint32_t ptr=*reinterpret_cast<volatile uint32_t*>(At(0x13cc6d8+side*4));if(!ptr)return;
 if(!r.base)r.base=ptr;
 const uint32_t off=ptr-r.base;if(off<0x8000&&off>r.maxOff)r.maxOff=off;
}

static ULONGLONG pbVictimUntil[2]={0,0};static ULONGLONG pbAuthorUntil[2]={0,0};static bool pbWas[2]={false,false};static int16_t hpWas[2]={1,1};

static ULONGLONG pbTempRevert[2]={0,0};

static bool UsesSchoolgirlTimingModelName(const char* n) noexcept {
 static const char* const names[]={
  "MARIE_DLCU_028",
  "MOMIJI_DLCU_032",
  "HELENA_DLCU_032",
  "KOKORO_DLCU_032",
  "CHRISTIE_DLCU_032",
  "LISA_DLCU_032",
  "KASUMI_DLCU_032",
  "RACHEL_DLCU_032",
  "NYOTENGU_DLCU_028",
  "MILA_DLCU_032",
  "AYANE_DLCU_032",
  "HITOMI_DLCU_032",
  "HONOKA_DLCU_017",
  "PHASE4_DLCU_028",
  "LEIFANG_DLCU_032",
  "TINA_DLCU_032",
  "MILA_DLCU_053",
  "ELIOT_DLCU_016",
  "BASS_DLCU_016",
  "HAYABUSA_DLCU_016",
  "KASUMI_DLCU_053",
  "RACHEL_DLCU_053",
  "NYOTENGU_DLCU_052",
  "RAIDOU_DLCU_010",
  "MOMIJI_DLCU_053",
  "AYANE_DLCU_053",
  "GENFU_DLCU_016",
  "HAYATE_DLCU_016",
  "TINA_DLCU_053",
  "BRAD_DLCU_016",
  "LISA_DLCU_053",
  "PHASE4_DLCU_052",
  "MARIE_DLCU_052",
  "ZACK_DLCU_016",
  "EIN_DLCU_016",
  "ALPHA152_DLCU_007",
  "LEIFANG_DLCU_053",
  "HITOMI_DLCU_053",
  "BAYMAN_DLCU_016",
  "JANNLEE_DLCU_016",
  "CHRISTIE_DLCU_053",
  "NAOTORA_DLCU_018",
  "KOKORO_DLCU_053",
  "HELENA_DLCU_053",
  "LEON_DLCU_016",
  "HONOKA_DLCU_041",
  "RIG_DLCU_016",
 };
 if(!n)return false;
 for(const char* s:names)if(std::strcmp(n,s)==0)return true;
 return false;
}

static bool IsVFSwimsuitModelName(const char* n) noexcept {
 static const char* const vf[]={"AKIRA_DLCU_012","JACKY_DLCU_012","PAI_DLCU_011","SARAH_DLCU_011"};
 if(!n)return false;
 for(const char* s:vf)if(std::strcmp(n,s)==0)return true;
 return false;
}
static bool UsesPowerTimingModelName(const char* n) noexcept {
 return UsesSchoolgirlTimingModelName(n)||(n&&std::strcmp(n,"PHASE4_DLCU_035")==0)||IsVFSwimsuitModelName(n);
}
static bool UsesSchoolgirlTiming(void* entity) noexcept {
 return UsesSchoolgirlTimingModelName(StateCostumes::ModelName(entity));
}
static void CancelCliffForPB(unsigned side) noexcept;
static void CancelPendingForPB(unsigned side) noexcept;
static void PBKOTick(ULONGLONG now) noexcept {
 for(unsigned side=0;side<2;++side){
  const bool pb=reinterpret_cast<BYTE(__cdecl*)(int)>(At(0x6b3730))(int(side))!=0;
  if(pb&&!pbWas[side]){pbVictimUntil[side^1u]=now+8000;pbAuthorUntil[side]=now+7000;
   void* pe=reinterpret_cast<void*(__cdecl*)(int)>(At(0x982670))(int(side));EntityInfo pi{};
   rb::Handle pbh;
   if(Entity(pe,pi,true)&&UsesPowerTimingModelName(StateCostumes::ModelName(pi.entity))
    &&rb::Claim(rb::KPB,side,scriptRun[side].number,0,scriptRun[side].startF,pi.entity,pbh)){
    CancelPendingForPB(side);
    TransitionSound(pi,true);TransitionBurst(pi,true,true);if(StateCostumes::OnToggleSchoolgirl(pi.entity))rb::MarkApplied(pbh);
    pbTempRevert[side]=now+12000;Queue("Power Blow School Girl / Zack Island / VF / Gust Phase4 : bascule au debut ; commandes du script de PB ignorees 12 s.");}
  }
  pbWas[side]=pb;

  if(pbTempRevert[side]&&pb&&pbTempRevert[side]<now+3000)pbTempRevert[side]=now+3000;

  if(pbTempRevert[side]&&now>=pbTempRevert[side]){pbTempRevert[side]=0;}
  if(false){
   void* pe=reinterpret_cast<void*(__cdecl*)(int)>(At(0x982670))(int(side));EntityInfo pi{};
   if(Entity(pe,pi,true)&&StateCostumes::IsTransformed(pi.entity)){
    TransitionSound(pi,true);TransitionBurst(pi,true,true);StateCostumes::OnToggleSchoolgirl(pi.entity);
    Queue("Power Blow School Girl : retour en uniforme.");}}
 }
 for(unsigned side=0;side<2;++side){
  int16_t hp=0;if(!Read(At(0x13d056c+side*0x6c8),hp))continue;
  if(hp<=0&&hpWas[side]>0&&pbVictimUntil[side]&&now<pbVictimUntil[side]){pbVictimUntil[side]=0;PS4Break(side,"KO par Power Blow",0,0);}

  else if(hp<=0&&hpWas[side]>0){
   static int sig=-1;
   if(sig<0){const BYTE s[]={0x55,0x8b,0xec,0x53,0x8b,0x5d,0x08,0x8b,0xc3,0x83,0xf0,0x01,0x69,0xc0,0xc8,0x06,0x00,0x00,0x33,0xc9};
    sig=Signature(0x691e10,s,sizeof s)?1:0;if(!sig)Queue("KO camera : signature 0x691E10 differente, regle inactive.");}
   const unsigned winner=side^1u;
   const bool special=sig==1&&reinterpret_cast<BYTE(__cdecl*)(int)>(At(0x691e10))(int(winner))!=0;
   char l[96];std::snprintf(l,sizeof l,"KO side=%u vainqueur=%u camera speciale=%u",side,winner,unsigned(special));Queue(l);
   if(special)PS4Break(side,"KO camera speciale",0,0);
  }
  if(hp>0&&now>=pbVictimUntil[side])pbVictimUntil[side]=0;
  hpWas[side]=hp;
 }
}

static unsigned pendingFlips[2]={0,0};static ULONGLONG pendingAt[2]={0,0};

struct PendingD1Source {
 void* entity{};uint32_t character{},generation{},number{},serial{};LONG epoch{};bool effect{};rb::Handle h{};
};
static PendingD1Source pendingD1Source[2][64]{};
struct ConfirmedPLScript {
 void* entity{};const char* model{};uint32_t character{},generation{},number{},serial{};
 LONG epoch{};bool armed{},bound{};
};
static ConfirmedPLScript confirmedPL[2]{};
static ConfirmedPLScript previousPLScript[2][3]{};
static void CancelPendingForPB(unsigned side) noexcept {
 if(side>=2)return;
 pendingFlips[side]=0;pendingAt[side]=0;
 for(auto& source:pendingD1Source[side])source={};
 confirmedPL[side]={};
 for(auto& token:previousPLScript[side])token={};
 CancelCliffForPB(side);
}
static uint32_t PLScriptNumber(uint32_t character,uint32_t action) noexcept {
 for(const auto& row:kPowerLauncherScripts)if(row.character==character&&row.action==action)return row.number;
 return 0xffffffffu;
}
static bool PLConsumesD1WithModel(const EntityInfo& actor,uint32_t number,uint32_t serial,const char* model) noexcept {
 auto matches=[&](ConfirmedPLScript& pl){
  if(!pl.armed||pl.entity!=actor.entity||pl.character!=actor.identity0||pl.generation!=actor.identity4
   ||pl.epoch!=InterlockedCompareExchange(&resetEpoch,0,0)||pl.model!=model
   ||pl.number!=number||pl.serial!=serial)return false;
  pl.bound=true;return true;
 };
 if(matches(confirmedPL[actor.side]))return true;
 for(auto& token:previousPLScript[actor.side])if(matches(token))return true;
 return false;
}
static bool PLConsumesD1(const EntityInfo& actor,uint32_t number,uint32_t serial) noexcept {
 return PLConsumesD1WithModel(actor,number,serial,StateCostumes::ModelName(actor.entity));
}
static bool QueueD1Flip(const EntityInfo& actor,uint32_t number,uint32_t serial,ULONGLONG at,bool effect,rb::Handle h={}) noexcept {
 unsigned& n=pendingFlips[actor.side];
 if(n>=64){Queue("D1 pending queue full: new command declined.");return false;}
 pendingD1Source[actor.side][n++]={actor.entity,actor.identity0,actor.identity4,number,serial,
  InterlockedCompareExchange(&resetEpoch,0,0),effect,h};
 pendingAt[actor.side]=at;return true;
}

static bool BindPLScript(const EntityInfo& actor,uint32_t number,const char* model) noexcept {
 const ScriptRun& current=scriptRun[actor.side];const ScriptEnd& ended=scriptEnd[actor.side];
 uint32_t serial=0;bool bound=true;
 if(current.serial&&current.number==number){serial=current.serial;}
 else if(ended.based&&ended.serial&&ended.number==number&&ended.serial+1==current.serial){serial=ended.serial;}
 else return false;
 if(!PLConsumesD1WithModel(actor,number,serial,model)){
  auto& old=previousPLScript[actor.side];old[2]=old[1];old[1]=old[0];old[0]=confirmedPL[actor.side];
  confirmedPL[actor.side]={actor.entity,model,actor.identity0,actor.identity4,
   number,serial,InterlockedCompareExchange(&resetEpoch,0,0),true,bound};
 }

 bool effectAlready=false;unsigned kept=0;
 for(unsigned i=0;i<pendingFlips[actor.side];++i){
  const PendingD1Source& source=pendingD1Source[actor.side][i];
  const bool same=source.entity==actor.entity&&source.character==actor.identity0&&source.generation==actor.identity4
   &&source.epoch==confirmedPL[actor.side].epoch&&PLConsumesD1WithModel(actor,source.number,source.serial,model);
  if(same)effectAlready|=source.effect;else pendingD1Source[actor.side][kept++]=source;
 }
 pendingFlips[actor.side]=kept;
 return effectAlready;
}
static void ApplyPendingD1(const EntityInfo& actor) noexcept {
 const LONG epoch=InterlockedCompareExchange(&resetEpoch,0,0);
 unsigned& count=pendingFlips[actor.side];
 while(count){const PendingD1Source source=pendingD1Source[actor.side][--count];
  if(source.entity!=actor.entity||source.character!=actor.identity0||source.generation!=actor.identity4||source.epoch!=epoch)continue;
  if(!rb::Live(source.h))continue;
  if(PLConsumesD1(actor,source.number,source.serial))continue;
  if(StateCostumes::OnToggleSchoolgirl(actor.entity))rb::MarkApplied(source.h);
 }
}

#include "CliffTransform.inc"
#include "PLStartTransform.inc"
static bool vfIdle[2]={false,false};static ULONGLONG vfIdleNext[2]={0,0};
static uint32_t d1LastNum[2]={0xffffffff,0xffffffff},d1LastOff[2]={0,0};
static ULONGLONG d1LastAt[2]={0,0};
namespace {
void ResetOnlineTracking() noexcept {
 ResetOnlineClock();
 for(unsigned side=0;side<2;++side){
  edges[side]={};scriptRun[side]={};scriptEnd[side]={};
  ps4LastC[side]=0;ps4LastA[side]=0;
  pbVictimUntil[side]=pbAuthorUntil[side]=pbTempRevert[side]=0;
  pbWas[side]=false;hpWas[side]=1;
  vfIdle[side]=false;vfIdleNext[side]=0;
  d1LastNum[side]=0xffffffff;d1LastOff[side]=0;d1LastAt[side]=0;
  CancelPendingForPB(side);
 }
 ClearCliffTracking();ClearPLStartTracking();rb::Clear();
 Queue("ONLINE TRIAL: native match/round reset; timers and transition edges cleared.");
}
}
static volatile uint32_t camCaller=0,camSerial=0;
extern "C" void __cdecl D1Tick() noexcept {
 if(InterlockedCompareExchange(&controlActive,0,0)==0){ClearCliffTracking();ClearPLStartTracking();return;}
 uint32_t phase=0;if(!Read(At(0x1487a4c),phase)||phase!=4){if(phase!=3){ClearCliffTracking();ClearPLStartTracking();}return;}
 if(IsNetworkContext()&&!OnlineStateAllowed())return;
 rb::RenderTick();
 const ULONGLONG now=CostumeTime();
 PBKOTick(now);
 PLStartTick();
 CliffTick();
 if(false){static uint32_t seenCam=0;if(seenCam!=camSerial){seenCam=camSerial;int16_t h0=0,h1=0;Read(At(0x13d056c),h0);Read(At(0x13d056c+0x6c8),h1);
  char l[96];std::snprintf(l,sizeof l,"CAMERA mode appelant=0x%08X vie J1=%d J2=%d",unsigned(camCaller-uint32_t(uintptr_t(At(0)))),int(h0),int(h1));Queue(l);}}

 {static ULONGLONG lastRead=0;static int key=0;static bool down=false;
  if(!lastRead||now-lastRead>2000){lastRead=now;char ini[MAX_PATH]{};GetModuleFileNameA(nullptr,ini,MAX_PATH);
   char* e=std::strrchr(ini,92);if(e){std::snprintf(ini,MAX_PATH,"%s",Extra_Ini());key=GetPrivateProfileIntA("StatefulCostumesVF","Key",0,ini);}}
  DWORD pid=0;GetWindowThreadProcessId(GetForegroundWindow(),&pid);
  const bool now_down=key>0&&key<256&&pid==GetCurrentProcessId()&&(GetAsyncKeyState(key)&0x8000)!=0;
  if(!IsNetworkContext()&&now_down&&!down){
   void* pe=reinterpret_cast<void*(__cdecl*)(int)>(At(0x982670))(0);EntityInfo pi{};
   if(Entity(pe,pi,true)){const uint32_t id=pi.identity0;
    if((id==31||id==32||id==33||id==41)&&StateCostumes::IsSchoolgirl(pi.entity)){
     TransitionSound(pi,true);TransitionBurst(pi,true,true);StateCostumes::OnToggleSchoolgirl(pi.entity);
     char l[80];std::snprintf(l,sizeof l,"VF touche bascule character=%u",unsigned(id));Queue(l);}}
  }
  down=now_down;}

 {auto& idle=vfIdle;auto& idleNext=vfIdleNext;static uint32_t lastSteps[2]={0,0};static ULONGLONG lastNow[2]={0,0},active[2]={0,0};

  uint32_t recNow=0;const bool recOk=ReadOnlineFrame(recNow);static uint32_t recPrev=0;uint32_t recDelta=(recOk&&recNow>recPrev&&recNow-recPrev<=10)?recNow-recPrev:0;if(recOk)recPrev=recNow;
  for(unsigned side=0;side<2;++side){const ScriptRun& r=scriptRun[side];
   const uint32_t steps=interpSteps[side];const bool running=steps!=lastSteps[side];lastSteps[side]=steps;
   if(r.serial==0||r.number!=0){idle[side]=false;continue;}
   if(!idle[side]){idle[side]=true;idleNext[side]=288;active[side]=0;lastNow[side]=now;continue;}

   uint32_t act=0xffffffff;Read(At(0x13d05ac+side*0x6c8),act);act&=0xffff;
   {static unsigned dl=0;static uint32_t lastAct[2]={0xffff,0xffff};{static uint32_t lastCam=0;const uint32_t cm=camCaller;if((act!=lastAct[side]||cm!=lastCam)&&dl<60){lastAct[side]=act;lastCam=cm;++dl;char l[96];std::snprintf(l,sizeof l,"VF garde diag side=%u action=%u horloge=%u ok=%u",side,unsigned(act),unsigned(recNow),unsigned(recOk));Queue(l);}}}
   ULONGLONG dt=now-lastNow[side];lastNow[side]=now;if(dt>100)dt=100;(void)dt;if(act==0)active[side]+=recDelta;
   if(active[side]<idleNext[side])continue;
   idleNext[side]+=525;
   void* pe=reinterpret_cast<void*(__cdecl*)(int)>(At(0x982670))(int(side));EntityInfo pi{};
   if(!Entity(pe,pi,true))continue;const uint32_t id=pi.identity0;
   if((id==31||id==32||id==33||id==41)&&StateCostumes::IsSchoolgirl(pi.entity)){
    TransitionSound(pi,true);TransitionBurst(pi,true,true);StateCostumes::OnToggleSchoolgirl(pi.entity);
    char l[80];std::snprintf(l,sizeof l,"VF garde bascule side=%u character=%u",side,unsigned(id));Queue(l);}}}
 {static uint32_t seen[2]={0,0};
  for(unsigned side=0;side<2;++side)if(seen[side]!=diagSerial[side]){seen[side]=diagSerial[side];
   char l[96];std::snprintf(l,sizeof l,"Fin de script %u side=%u position max=%u",unsigned(diagNumber[side]),side,unsigned(diagMax[side]));Queue(l);}}
 for(unsigned side=0;side<2;++side){

  if(pendingFlips[side]&&now-pendingAt[side]>=(IsNetworkContext()?166u:167u)){
   void* pe=reinterpret_cast<void*(__cdecl*)(int)>(At(0x982670))(int(side));EntityInfo pi{};
   if(Entity(pe,pi,true))ApplyPendingD1(pi);
  }
 }
 for(unsigned side=0;side<2;++side){
  ScriptRun& r=scriptRun[side];
  if(r.seenSerial!=r.serial){

   const ScriptEnd& x=scriptEnd[side];
   if(x.serial==r.seenSerial&&x.based&&x.number<0x2000){
    void* pe=reinterpret_cast<void*(__cdecl*)(int)>(At(0x982670))(int(side));EntityInfo pi{};
    if(Entity(pe,pi,true)){unsigned k=0;
     for(unsigned i=0;i<sizeof(kPS4D1)/sizeof(kPS4D1[0]);++i){
      if(kPS4D1[i][0]!=pi.identity0||kPS4D1[i][1]!=x.number)continue;
      const uint32_t bit=1u<<(k&31);++k;
      if(OwnsCliffScript(pi,x.number,x.serial)){r.fired|=bit;continue;}

      if(!(r.fired&bit)&&x.maxOff>=kPS4D1[i][3]){
       r.fired|=bit;if(pbTempRevert[side]&&now<pbTempRevert[side]){Queue("PS4 0xD1 (fin de script) pendant le Power Blow : absorbee.");continue;}
       if(PLConsumesD1(pi,x.number,x.serial)){Queue("PL D1 end command already satisfied by PL start.");continue;}
       rb::Handle eh;if(!rb::Claim(rb::KD1,side,x.number,i,x.startF,pi.entity,eh))continue;
       const bool sg=StateCostumes::IsSchoolgirl(pi.entity);
       if(QueueD1Flip(pi,x.number,x.serial,now-167,sg,eh)&&sg){TransitionSound(pi,true);TransitionBurst(pi,true,true);}
       char line[160];std::snprintf(line,sizeof line,"PS4 0xD1 (script %u octet %u, vu a la fin) side=%u character=%u schoolgirl=%u",
        unsigned(x.number),unsigned(kPS4D1[i][2]),side,unsigned(pi.identity0),unsigned(sg));Queue(line);}}}
   }
   if(pbAuthorUntil[side]>now){char l[80];std::snprintf(l,sizeof l,"PB script lance side=%u numero=%u",side,unsigned(r.number));Queue(l);}
   if(side==1){static ULONGLONG rd=0;static int all=0;if(!rd||now-rd>2000){rd=now;char ini[MAX_PATH]{};GetModuleFileNameA(nullptr,ini,MAX_PATH);
     char* e=std::strrchr(ini,92);if(e){std::snprintf(ini,MAX_PATH,"%s",Extra_Ini());all=GetPrivateProfileIntA("StatefulCostumesDebug","ScriptLog",0,ini);}}
    const LONG head=InterlockedCompareExchange(&scriptRingHead,0,0);static LONG tail=0;if(head-tail>64)tail=head-64;
    for(;tail<head;++tail){const uint32_t v=scriptRing[tail&63];
     if(all){char l[64];std::snprintf(l,sizeof l,"SCRIPT side=%u numero=%u",unsigned(v>>16),unsigned(v&0xffff));Queue(l);}}}
   r.seenSerial=r.serial;r.fired=0;r.flipped=0;for(auto& hh:r.h)hh={};}
  const uint32_t number=r.number,off=r.maxOff;
  if(number>=0x2000)continue;
  void* e=reinterpret_cast<void*(__cdecl*)(int)>(At(0x982670))(int(side));
  EntityInfo info{};if(!Entity(e,info,true))continue;
  unsigned k=0;
  for(unsigned i=0;i<sizeof(kPS4D1)/sizeof(kPS4D1[0]);++i){
   if(kPS4D1[i][0]!=info.identity0||kPS4D1[i][1]!=number)continue;
   const uint32_t bit=1u<<(k&31);const unsigned slot=k&31;++k;
   if(OwnsCliffScript(info,number,r.serial)){r.fired|=bit;r.flipped|=bit;continue;}

   auto& lastNum=d1LastNum;auto& lastOff=d1LastOff;auto& lastAt=d1LastAt;
   if(!(r.fired&bit)&&r.base&&off>=kPS4D1[i][2]){
    r.fired|=bit;r.at[slot]=now;
    if(!rb::Claim(rb::KD1,side,number,i,r.startF,info.entity,r.h[slot])){r.flipped|=bit;continue;}
    const bool dup=lastNum[side]==number&&lastOff[side]==kPS4D1[i][2]&&pbAuthorUntil[side]>now&&now-lastAt[side]<7000u;
    lastNum[side]=number;lastOff[side]=kPS4D1[i][2];lastAt[side]=now;
    if(dup){r.flipped|=bit;Queue("PS4 0xD1 ignoree : meme script relance (doublon PC), sans son ni effet.");continue;}
    if(PLConsumesD1(info,number,r.serial)){r.flipped|=bit;Queue("PL D1 command already satisfied by PL start.");continue;}
    const bool sg=StateCostumes::IsSchoolgirl(info.entity);
    if(sg&&!pbTempRevert[side]){TransitionSound(info,true);TransitionBurst(info,true,true);}
    char line[160];std::snprintf(line,sizeof line,"PS4 0xD1 (script %u octet %u) side=%u character=%u schoolgirl=%u",
     unsigned(number),unsigned(kPS4D1[i][2]),side,unsigned(info.identity0),unsigned(sg));Queue(line);
   }
   if((r.fired&bit)&&!(r.flipped&bit)){r.flipped|=bit;
    if(pbTempRevert[side]&&now<pbTempRevert[side]){Queue("PS4 0xD1 pendant le Power Blow : absorbee (transformation School Girl deja faite).");}
    else{QueueD1Flip(info,number,r.serial,now,StateCostumes::IsSchoolgirl(info.entity)&&!pbTempRevert[side],r.h[slot]);}}
  }
 }
}
static bool ArmInterpStep() noexcept {
 const BYTE sig[]={0x55,0x8b,0xec,0x81,0xec,0xc0,0x00,0x00,0x00,0xa1};
 if(!Signature(0x6f2060,sig,sizeof sig)){Notify("PS4 0xD1 : signature 0x6F2060 differente.");return false;}
 interpGateway=static_cast<BYTE*>(VirtualAlloc(nullptr,64,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE));if(!interpGateway)return false;
 BYTE* g=interpGateway;*g++=0x9c;*g++=0x60;*g++=0x51;
 *g++=0xe8;Put32(g,uint32_t(reinterpret_cast<BYTE*>(&InterpStep)-(g+4)));g+=4;
 *g++=0x83;*g++=0xc4;*g++=0x04;*g++=0x61;*g++=0x9d;
 std::memcpy(g,At(0x6f2060),9);g+=9;*g++=0xe9;Put32(g,uint32_t(At(0x6f2069)-(g+4)));g+=4;
 FlushInstructionCache(GetCurrentProcess(),interpGateway,64);
 DWORD old=0,ign=0;BYTE patch[9]={0xe9,0,0,0,0,0x90,0x90,0x90,0x90};Put32(patch+1,uint32_t(interpGateway-(At(0x6f2060)+5)));
 if(!VirtualProtect(At(0x6f2060),9,PAGE_EXECUTE_READWRITE,&old))return false;
 std::memcpy(At(0x6f2060),patch,9);VirtualProtect(At(0x6f2060),9,old,&ign);FlushInstructionCache(GetCurrentProcess(),At(0x6f2060),9);
 Notify("PS4 0xD1 position exacte : interpreteur 0x6F2060 accroche.");return true;
}

extern "C" void __cdecl CamHook(uint32_t ret,uint32_t) noexcept {if(camCaller!=ret){camCaller=ret;camSerial=camSerial+1;}}
static BYTE* camGateway=nullptr;
static bool ArmCamera() noexcept {
 const BYTE sig[]={0x55,0x8b,0xec,0x8b,0x4d,0x08,0x80,0x79,0x6c,0x00};
 if(!Signature(0x78b350,sig,sizeof sig)){Notify("Camera : signature 0x78B350 differente.");return false;}
 camGateway=static_cast<BYTE*>(VirtualAlloc(nullptr,64,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE));if(!camGateway)return false;
 BYTE* g=camGateway;*g++=0x9c;*g++=0x60;
 *g++=0xff;*g++=0x74;*g++=0x24;*g++=0x28;
 *g++=0xff;*g++=0x74;*g++=0x24;*g++=0x28;
 *g++=0xe8;Put32(g,uint32_t(reinterpret_cast<BYTE*>(&CamHook)-(g+4)));g+=4;
 *g++=0x83;*g++=0xc4;*g++=0x08;*g++=0x61;*g++=0x9d;
 std::memcpy(g,At(0x78b350),6);g+=6;*g++=0xe9;Put32(g,uint32_t(At(0x78b356)-(g+4)));g+=4;
 FlushInstructionCache(GetCurrentProcess(),camGateway,64);
 DWORD old=0,ign=0;BYTE patch[6]={0xe9,0,0,0,0,0x90};Put32(patch+1,uint32_t(camGateway-(At(0x78b350)+5)));
 if(!VirtualProtect(At(0x78b350),6,PAGE_EXECUTE_READWRITE,&old))return false;
 std::memcpy(At(0x78b350),patch,6);VirtualProtect(At(0x78b350),6,old,&ign);FlushInstructionCache(GetCurrentProcess(),At(0x78b350),6);
 Notify("Camera : modes 0x78B350 accroches (journal CAMERA).");return true;
}
static bool ArmScriptStart() noexcept {
 const BYTE sig[]={0x55,0x8b,0xec,0x83,0xec,0x50,0xa1};
 if(!Signature(0x6f7e30,sig,sizeof sig)){Notify("PS4 0xD1 : signature 0x6F7E30 differente.");return false;}
 scriptGateway=static_cast<BYTE*>(VirtualAlloc(nullptr,64,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE));if(!scriptGateway)return false;
 BYTE* g=scriptGateway;*g++=0x9c;*g++=0x60;
 *g++=0xff;*g++=0x74;*g++=0x24;*g++=0x2c;
 *g++=0xff;*g++=0x74;*g++=0x24;*g++=0x2c;
 *g++=0xe8;Put32(g,uint32_t(reinterpret_cast<BYTE*>(&ScriptStart)-(g+4)));g+=4;
 *g++=0x83;*g++=0xc4;*g++=0x08;*g++=0x61;*g++=0x9d;
 std::memcpy(g,At(0x6f7e30),6);g+=6;*g++=0xe9;Put32(g,uint32_t(At(0x6f7e36)-(g+4)));g+=4;
 FlushInstructionCache(GetCurrentProcess(),scriptGateway,64);
 DWORD old=0,ign=0;BYTE patch[6]={0xe9,0,0,0,0,0x90};Put32(patch+1,uint32_t(scriptGateway-(At(0x6f7e30)+5)));
 if(!VirtualProtect(At(0x6f7e30),6,PAGE_EXECUTE_READWRITE,&old))return false;
 std::memcpy(At(0x6f7e30),patch,6);VirtualProtect(At(0x6f7e30),6,old,&ign);FlushInstructionCache(GetCurrentProcess(),At(0x6f7e30),6);
 Notify("PS4 0xD1 armee : lancement de script 0x6F7E30, table de transformations PS4.");return true;
}

static BYTE* loopGateway=nullptr;
static bool ArmScriptLoop() noexcept {
 const BYTE sig[]={0x55,0x8b,0xec,0x56,0x8b,0x75,0x08,0x83,0xc8,0xff};
 if(!Signature(0x6f6140,sig,sizeof sig)){Notify("PS4 0xD1 : signature 0x6F6140 differente.");return false;}
 loopGateway=static_cast<BYTE*>(VirtualAlloc(nullptr,64,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE));if(!loopGateway)return false;
 BYTE* g=loopGateway;*g++=0x9c;*g++=0x60;*g++=0xff;*g++=0x74;*g++=0x24;*g++=0x28;
 *g++=0xe8;Put32(g,uint32_t(reinterpret_cast<BYTE*>(&InterpStep)-(g+4)));g+=4;
 *g++=0x83;*g++=0xc4;*g++=0x04;*g++=0x61;*g++=0x9d;
 std::memcpy(g,At(0x6f6140),7);g+=7;*g++=0xe9;Put32(g,uint32_t(At(0x6f6147)-(g+4)));g+=4;
 FlushInstructionCache(GetCurrentProcess(),loopGateway,64);
 DWORD old=0,ign=0;BYTE patch[7]={0xe9,0,0,0,0,0x90,0x90};Put32(patch+1,uint32_t(loopGateway-(At(0x6f6140)+5)));
 if(!VirtualProtect(At(0x6f6140),7,PAGE_EXECUTE_READWRITE,&old))return false;
 std::memcpy(At(0x6f6140),patch,7);VirtualProtect(At(0x6f6140),7,old,&ign);FlushInstructionCache(GetCurrentProcess(),At(0x6f6140),7);
 Notify("PS4 0xD1 fin de script : boucle 0x6F6140 accrochee.");return true;
}
bool ArmRenderer() noexcept {
 if(rendererArmed)return true;
 if(!prepared||!NativeGuards()||!AlternateGuard(false))return false;

 if(onlineTrialEnabled&&!InstallCostumeEffectRngHooks()){
  Notify("Stateful refused: private FX RNG routing could not be installed safely.");
  return false;
 }
 gateway=static_cast<BYTE*>(VirtualAlloc(nullptr,64,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
 if(!gateway)return false;
 resetGateway=gateway+32;
 std::memcpy(savedRender,At(0x76e960),6);std::memcpy(gateway,savedRender,6);
 gateway[6]=0xe9;Put32(gateway+7,static_cast<uint32_t>(At(0x76e966)-(gateway+11)));

 std::memcpy(savedReset,At(0x68b380),8);std::memcpy(resetGateway,savedReset,8);
 resetGateway[8]=0xe9;Put32(resetGateway+9,static_cast<uint32_t>(At(0x68b388)-(resetGateway+13)));
 auto abandon=[](){VirtualFree(gateway,0,MEM_RELEASE);gateway=nullptr;resetGateway=nullptr;};
 DWORD old=0;
 if(!VirtualProtect(gateway,64,PAGE_EXECUTE_READ,&old)){abandon();return false;}
 FlushInstructionCache(GetCurrentProcess(),gateway,64);
 if(!NativeGuards()){abandon();return false;}
 BYTE patch[6]={0xe9,0,0,0,0,0x90};Put32(patch+1,static_cast<uint32_t>(reinterpret_cast<BYTE*>(&RenderBridge)-(At(0x76e960)+5)));
 BYTE resetPatch[8]={0xe9,0,0,0,0,0x90,0x90,0x90};Put32(resetPatch+1,static_cast<uint32_t>(reinterpret_cast<BYTE*>(&ResetBridge)-(At(0x68b380)+5)));
 DWORD renderProtection=0,resetProtection=0,ignored=0;
 if(!VirtualProtect(At(0x76e960),6,PAGE_EXECUTE_READWRITE,&renderProtection)){abandon();return false;}
 if(!VirtualProtect(At(0x68b380),8,PAGE_EXECUTE_READWRITE,&resetProtection)){
  VirtualProtect(At(0x76e960),6,renderProtection,&ignored);abandon();return false;
 }
 if(!NativeGuards()){
  VirtualProtect(At(0x68b380),8,resetProtection,&ignored);
  VirtualProtect(At(0x76e960),6,renderProtection,&ignored);abandon();return false;
 }
 originalRender=reinterpret_cast<RenderFunction>(gateway);
 originalReset=reinterpret_cast<ResetFunction>(resetGateway);

 std::memcpy(At(0x68b380),resetPatch,8);
 std::memcpy(At(0x76e960),patch,6);
 VirtualProtect(At(0x68b380),8,resetProtection,&ignored);
 VirtualProtect(At(0x76e960),6,renderProtection,&ignored);
 FlushInstructionCache(GetCurrentProcess(),At(0x68b380),8);
 FlushInstructionCache(GetCurrentProcess(),At(0x76e960),6);
 ArmPS4Rule();
 ArmScriptStart();

 ArmInterpStep();
 ArmScriptLoop();
 rendererArmed=true;
 Notify("Full Effects r3: online deadlines use the native recorded-input clock; local FX use per-thread RNG routing.");
 Notify("Stateful renderer RVA36E960 and reset observer RVA28B380 armed before any 990016 mount.");return true;
}
bool RendererArmed() noexcept {
 if(!rendererArmed||!originalRender||!originalReset||!Accessible(At(0x76e960),6)||!Accessible(At(0x68b380),8))return false;
 BYTE* site=At(0x76e960);int32_t relative=0;
 if(site[0]!=0xe9||site[5]!=0x90)return false;
 std::memcpy(&relative,site+1,4);
 if(site+5+relative!=reinterpret_cast<BYTE*>(&RenderBridge))return false;
 site=At(0x68b380);
 if(site[0]!=0xe9||site[5]!=0x90||site[6]!=0x90||site[7]!=0x90)return false;
 std::memcpy(&relative,site+1,4);
 return site+5+relative==reinterpret_cast<BYTE*>(&ResetBridge);
}
bool ValidateAlternateTables() noexcept {return prepared&&AlternateGuard(alternatesInstalled);}
bool ActivateControlBeforeMount() noexcept {
 if(!alternatesInstalled||!RendererArmed()||!AlternateGuard(true))return false;
 InterlockedExchange(&controlActive,1);
 Notify("Stateful control activated immediately before990016 native mount; retained after any partial mount failure.");
 return true;
}
bool InstallAlternateTables() noexcept {
 if(alternatesInstalled)return AlternateGuard(true);
 if(!rendererArmed||!AlternateGuard(false))return false;
 unsigned touched[16]{},count=0;
 for(unsigned i=0;i<16;i++)if(!kAlternates[i].expectedPointerRva){
  auto field=reinterpret_cast<PVOID volatile*>(image+kAlternates[i].fieldRva);
  const auto result=ProtectedPointer::Exchange(field,alternatePointers[i],nullptr);
  if(result.exchanged)touched[count++]=i;
  if(!result.exchanged||!result.protectionRestored){
   bool restored=true;
   while(count){unsigned j=touched[--count];const auto undo=ProtectedPointer::Exchange(reinterpret_cast<PVOID volatile*>(image+kAlternates[j].fieldRva),nullptr,alternatePointers[j]);restored=undo.exchanged&&undo.protectionRestored&&restored;}
   Notify(restored?"Stateful refused before mount: ALT exchange/protection failed; pointers rolled back.":"Stateful refused before mount: incomplete ALT rollback; persistent storage retained; restart required.");return false;
  }
 }
 alternatesInstalled=true;
 if(!AlternateGuard(true)){RollbackAlternateTables();return false;}
 Notify("Stateful ALT commit complete: 14 null pointers populated; 2 existing pointers unchanged.");return true;
}
void RollbackAlternateTables() noexcept {
 if(!alternatesInstalled)return;
 bool restored=true;
 for(unsigned i=0;i<16;i++)if(!kAlternates[i].expectedPointerRva){
  const auto result=ProtectedPointer::Exchange(reinterpret_cast<PVOID volatile*>(image+kAlternates[i].fieldRva),nullptr,alternatePointers[i]);
  restored=result.exchanged&&result.protectionRestored&&restored;
 }
 if(restored){alternatesInstalled=false;Notify("Stateful ALT pointers rolled back before archive mount; page protections restored.");}
 else Notify("Stateful ALT rollback incomplete; persistent storage retained; restart required.");
}
void FlushLogs() noexcept {
 PollEffectTuningFromWorker();
 if(!destination)return;
 for(unsigned i=0;i<128;i++){
  char copy[256]{};bool found=false;
  AcquireSRWLockExclusive(&queueLock);
  if(queueRead!=queueWrite){std::memcpy(copy,messages[queueRead],256);queueRead=(queueRead+1)%128;found=true;}
  ReleaseSRWLockExclusive(&queueLock);
  if(!found)break;destination(copy);
 }
}
bool EventSelfTest() noexcept {
 unsigned checks=0,failed=0;
 auto expect=[&](bool value,bool wanted){++checks;if(value!=wanted)++failed;};
 for(uint32_t side=0;side<2;++side)for(uint32_t choice=2;choice<=3;++choice){
  const uint32_t action=choice==2?0x142:0x143;
  expect(CliffAuthor(side,side,1-side,choice,30,action,13,true),true);
  expect(CliffAuthor(side,side,1-side,choice,1,action,13,true),true);
  expect(CliffAuthor(side,1-side,side,choice,30,action,13,true),false);
  expect(CliffAuthor(side,side,side,choice,30,action,13,true),false);
  expect(CliffAuthor(side,side,2,choice,30,action,13,true),false);
  expect(CliffAuthor(side,side,1-side,choice,0,action,13,true),false);
  expect(CliffAuthor(side,side,1-side,choice,31,action,13,true),false);
  expect(CliffAuthor(side,side,1-side,choice,30,action,9,true),false);
  expect(CliffAuthor(side,side,1-side,choice,30,action,13,false),false);
  expect(CliffAuthor(side,side,1-side,choice,30,choice==2?0x143:0x142,13,true),false);
  expect(CliffAuthor(side,side,1-side,1,30,action,13,true),false);
  expect(CliffAuthor(side,side,1-side,choice,30,8009,13,true),false);
 }
 expect(CliffAuthor(2,2,0,2,30,0x142,13,true),false);
 expect(CliffAuthor(0,0,1,2,30,0x6358,13,true),false);
 expect(CliffAuthor(0,0,1,3,30,0x6359,13,true),false);

 for(unsigned mask=0;mask<64;++mask)
  expect(BurstPolicy(mask&1,mask&2,mask&4,mask&8,mask&16,mask&32),mask==63);
 DiagnosticRateLimit rate;
 expect(rate.Permit(0),true);
 for(uint64_t tick=1;tick<1000;++tick)expect(rate.Permit(tick),false);
 expect(rate.Permit(1000),true);expect(rate.Permit(1000),false);
 expect(rate.Permit(1999),false);expect(rate.Permit(2000),true);
 DiagnosticRateLimit wrap;
 expect(wrap.Permit(UINT64_MAX-500),true);
 expect(wrap.Permit(498),false);expect(wrap.Permit(499),true);
 for(const auto& attack:kPowerLaunchers){
  expect(PLContact(false,25633,9,true,attack.character,attack.action,true),true);
  expect(PLContact(false,0,0,true,attack.character,attack.action,true),false);
  const uint32_t blocks[]={16001u,16009u,16011u};
  for(uint32_t block:blocks)
   expect(PLContact(false,block,9,true,attack.character,attack.action,true),false);
  expect(PLContact(false,25633,8,true,attack.character,attack.action,true),false);
  expect(PLContact(false,25633,9,false,attack.character,attack.action,true),false);
  expect(PLContact(false,25633,9,true,attack.character,attack.action,false),false);
  expect(PLContact(true,25633,9,true,attack.character,attack.action,true),false);

  bool previous=false;
  expect(PLContact(previous,25633,9,true,attack.character,attack.action,false),false);
  previous=PLReaction(25633,9);
  expect(PLContact(previous,25633,9,true,attack.character,attack.action,true),false);
  previous=PLReaction(0,0);
  expect(PLContact(previous,25633,9,true,attack.character,attack.action,true),true);
 }
 const TauntId aliases[]={{4,8610},{9,8575},{9,8576}};
 for(const auto& alias:aliases){
  expect(PLContact(false,25633,9,true,alias.character,alias.action,false),false);
  expect(PLContact(false,25633,9,true,alias.character,alias.action,true),false);
 }
 expect(PLContact(false,25633,9,true,0xffffffff,8503,true),false);
 std::printf("{\"ok\":%s,\"checks\":%u,\"failures\":%u,\"allowlisted_pl_actions\":37,\"excluded_reaction_aliases\":3,\"game_process_accessed\":false}\n",failed?"false":"true",checks,failed);
 return failed==0;
}
#ifdef PREFLIGHT_TOOL
#include "IntegratedEventSelfTest.inc"
#include "OnlineTrialSelfTest.inc"
bool ReloadAliasSelfTest() noexcept {
 if(image||prepared||alternatesInstalled||controlActive)return false;
 image=static_cast<BYTE*>(VirtualAlloc(nullptr,0x1100000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
 if(!image)return false;
 uint32_t tables[16][8]{};unsigned checks=0,failed=0;
 auto check=[&](bool ok,const char* label){++checks;if(!ok){++failed;std::printf("FAIL reload ALT: %s\n",label);}};
 for(unsigned i=0;i<16;i++){
  const auto& plan=kAlternates[i];alternatePointers[i]=tables[i];
  const uint32_t existing=plan.expectedPointerRva?uint32_t(uintptr_t(image+plan.expectedPointerRva)):0;
  Put32(image+plan.fieldRva,existing);
  for(unsigned j=0;j<4;j++){
   tables[i][2*j]=plan.pairs[j].h;tables[i][2*j+1]=plan.pairs[j].hl;
   if(existing){Put32(reinterpret_cast<BYTE*>(uintptr_t(existing))+8*j,plan.pairs[j].h);Put32(reinterpret_cast<BYTE*>(uintptr_t(existing))+8*j+4,plan.pairs[j].hl);}
   Put32(At(0x11dacf0)+16*plan.pairs[j].h,uint32_t(uintptr_t(plan.pairs[j].hName)));
   Put32(At(0x11dacf0)+16*plan.pairs[j].hl,uint32_t(uintptr_t(plan.pairs[j].hlName)));
  }
 }
 prepared=true;check(ValidateAlternateTables(),"initial original names accepted");
 const auto& first=kAlternates[0].pairs[0];const auto* alias=SharedAlias(first.h,990016);
 check(alias!=nullptr,"fixture alias exists");
 if(!alias){VirtualFree(image,0,MEM_RELEASE);image=nullptr;return false;}
 Put32(At(0x11dacf0)+16*first.h,uint32_t(uintptr_t(alias->unique)));
 check(!ValidateAlternateTables(),"redirected names refused before initial mount");
 Put32(At(0x11dacf0)+16*first.h,uint32_t(uintptr_t(first.hName)));
 for(unsigned i=0;i<16;i++)if(!kAlternates[i].expectedPointerRva)Put32(image+kAlternates[i].fieldRva,uint32_t(uintptr_t(tables[i])));
 alternatesInstalled=true;check(ValidateAlternateTables(),"initial ALT commit retains original names");
 for(const auto& plan:kAlternates)for(const auto& pair:plan.pairs)for(unsigned id:{pair.h,pair.hl}){
  const auto* item=SharedAlias(id,990016);check(item!=nullptr,"every alternate has its own pack alias");
  if(item)Put32(At(0x11dacf0)+16*id,uint32_t(uintptr_t(item->unique)));
 }
 check(!ValidateAlternateTables(),"aliases require active controller");
 controlActive=1;check(ValidateAlternateTables(),"all installed aliases accepted after activation");
 Put32(At(0x11dacf0)+16*first.h,uint32_t(uintptr_t("unexpected alias")));
 check(!ValidateAlternateTables(),"foreign alias refused after activation");
 Put32(At(0x11dacf0)+16*first.h,uint32_t(uintptr_t(alias->unique)));
 const uint32_t saved=tables[0][0];tables[0][0]^=1;
 check(!ValidateAlternateTables(),"altered persistent ALT pair refused");tables[0][0]=saved;
 const uint32_t field=U32(image+kAlternates[0].fieldRva);Put32(image+kAlternates[0].fieldRva,0);
 check(!ValidateAlternateTables(),"missing installed ALT pointer refused");Put32(image+kAlternates[0].fieldRva,field);
 check(ValidateAlternateTables(),"restored committed layout passes again");
 controlActive=0;alternatesInstalled=false;prepared=false;
 for(auto& pointer:alternatePointers)pointer=nullptr;
 VirtualFree(image,0,MEM_RELEASE);image=nullptr;
 std::printf("{\"ok\":%s,\"checks\":%u,\"failures\":%u,\"actual_runtime_ALT_guard\":true,\"game_process_accessed\":false}\n",failed?"false":"true",checks,failed);
 return failed==0;
}
#endif
}
