/* GPL-3.0-or-later. Original instructions enter only through private generated C. */
#include "h4_host.h"
#include <assert.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <locale.h>
#include <wctype.h>

static uint32_t last_error, tls_value[1088], exception_filter;
static unsigned char tls_used[1088];
static unsigned timer_period_refs[1001];
static unsigned com_refs,com_mode;
static const char *active_import;
static uint32_t active_iat;
static uint64_t api_calls,original_entries;
static unsigned original_unique,winmain_reached;
static int selftesting;
int h4_eh_test_mode(void){return selftesting;}
static unsigned callback_tests,cleanup_count;
static uint32_t eh_test_effect,eh_test_record,eh_test_binding,eh_test_object,cleanup_rec,cleanup_result;
static void test_catch_first(void){MEM32(eh_test_effect)=1;g_esi=0xdeadbeef;h4_test_api("KERNEL32.dll!RaiseException",4,0xe06d7363u,1u,3u,eh_test_record);void_fatal("synthetic rethrow returned");}
static void test_catch_second(void){assert(MEM32(eh_test_effect)==1);if(eh_test_binding==1)assert(MEM8(cleanup_rec-8)==0x7b);else if(eh_test_binding==2)assert(MEM32(cleanup_rec-8)==eh_test_object);MEM32(eh_test_effect)=12;g_eax=0x12345678;g_esp+=4;}
static void test_cleanup_a(void){assert(MEM32(cleanup_rec+8)==0xffffffffu&&g_ebp==cleanup_rec+12);MEM32(cleanup_result)=MEM32(cleanup_result)*10+1;cleanup_count++;g_esp+=4;}
static void test_cleanup_b(void){assert(MEM32(cleanup_rec+8)==0&&g_ebp==cleanup_rec+12);MEM32(cleanup_result)=MEM32(cleanup_result)*10+2;cleanup_count++;g_esp+=4;}
static void test_wndproc(void) {h4_need(g_esp,20,0);assert(MEM32(g_esp+4)==0x50000001u);g_eax=MEM32(g_esp+8)==0x81;g_esp+=20;++callback_tests;}
static unsigned char visited[(H4_IMAGE_SIZE+7)/8];
void h4_record_entry(uint32_t va) {
 uint32_t offset=va-H4_IMAGE_BASE;if(offset>=H4_IMAGE_SIZE)void_fatal("entry outside reviewed PE %08x",va);
 ++original_entries;if(!(visited[offset>>3]&(1u<<(offset&7)))){visited[offset>>3]|=1u<<(offset&7);++original_unique;}
#ifdef H4_MILESTONE_WINMAIN
 if(va==H4_MILESTONE_WINMAIN){winmain_reached=1;void_log("ENTERED original WinMain %08x esp=%08x",va,g_esp);}
#endif
}
static void execution_summary(void) {void_log("execution summary: entries=%llu unique=%u WinMain=%u last=%08x",(unsigned long long)original_entries,original_unique,winmain_reached,g_cur_func);}


void void_log(const char *fmt, ...) {va_list ap;va_start(ap,fmt);vfprintf(stderr,fmt,ap);va_end(ap);fputc('\n',stderr);}
_Noreturn void void_fatal(const char *fmt, ...) {va_list ap;va_start(ap,fmt);vfprintf(stderr,fmt,ap);va_end(ap);fputc('\n',stderr);exit(78);}
void void_set_last_error(uint32_t error) {last_error=error;if(g_fs_base)MEM32(g_fs_base+0x34)=error;}
void void_lift_trap(uint32_t va,const char *reason) {void_fatal("lift trap %08x: %s",va,reason);}
void h4_need(uint32_t va,uint32_t n,int write) {
 if (!(write?void_vm_writable(va,n):void_vm_readable(va,n)))void_fatal("bad API %s pointer %08x+%x from %08x",active_import,va,n,g_cur_func);
}
uint32_t h4_arg(unsigned n) {h4_need(g_esp+4*n,4,0);return MEM32(g_esp+4*n);}
const char *h4_string(uint32_t va) {uint32_t n=0;while(n<32768){h4_need(va+n,1,0);if(!MEM8(va+n))return GPTR(va);++n;}void_fatal("unterminated string %08x",va);}
/* The PoC guest timezone is UTC, independent of host daylight-saving rules. */
static void system_time(uint32_t out,time_t seconds,unsigned milliseconds) {
 struct tm t;if(!gmtime_r(&seconds,&t))void_fatal("time conversion failed");h4_need(out,16,1);
 uint16_t fields[]={t.tm_year+1900,t.tm_mon+1,t.tm_wday,t.tm_mday,t.tm_hour,t.tm_min,t.tm_sec,milliseconds};
 memcpy(GPTR(out),fields,sizeof fields);
}
static uint32_t tls_alloc(void) {for(unsigned i=0;i<1088;i++)if(!tls_used[i]){tls_used[i]=1;tls_value[i]=0;return i;}void_set_last_error(8);return 0xffffffff;}
static uint32_t tls_get(uint32_t i) {if(i>=1088||!tls_used[i]){void_set_last_error(87);return 0;}void_set_last_error(0);return tls_value[i];}
static uint32_t tls_set(uint32_t i,uint32_t v) {if(i>=1088||!tls_used[i]){void_set_last_error(87);return 0;}tls_value[i]=v;return 1;}
static void flags(uint32_t value,uint32_t allowed) {if(value&~allowed)void_fatal("unsupported %s flags %08x",active_import,value);}
static void finish(uint32_t value,unsigned argc) {g_eax=value;g_esp+=4+4*argc;}
/* ctype1 adapted from the pinned H5 kernel32.c, GPL-3.0-or-later. */
static uint16_t ctype1(uint32_t c)
{
    uint16_t t = 0;
    if (c == 0) return 0x20;            /* C1_CNTRL */
    if (iswupper((wint_t)c)) t |= 0x001;
    if (iswlower((wint_t)c)) t |= 0x002;
    if (iswdigit((wint_t)c)) t |= 0x004;
    if (iswspace((wint_t)c)) t |= 0x008;
    if (iswpunct((wint_t)c)) t |= 0x010;
    if (iswcntrl((wint_t)c)) t |= 0x020;
    if (c == ' ' || c == '\t') t |= 0x040;
    if (iswxdigit((wint_t)c)) t |= 0x080;
    if (iswalpha((wint_t)c)) t |= 0x100;
    t |= 0x200;                          /* C1_DEFINED */
    if (c >= 0x80 && c < 0xA0) t = 0x220;
    return t;
}

static uint32_t convert(int wide) {
 uint32_t cp=h4_arg(1),options=h4_arg(2),src=h4_arg(3),dst=h4_arg(5),cap=h4_arg(6);int32_t len=(int32_t)h4_arg(4);
 if(cp==0)cp=1252;
 if((cp!=1252&&cp!=1251)||!src||len==0||len< -1||(int32_t)cap<0||options&~(wide?0x400u:9u)){void_set_last_error(87);return 0;}
 uint32_t n=0;
 if(len==-1){do{h4_need(src+n*(wide?2:1),wide?2:1,0);++n;if(n>1048576)void_fatal("conversion input too long");}while(wide?MEM16(src+2*(n-1)):MEM8(src+n-1));}else n=(uint32_t)len;
 if(n>0x10000000){void_set_last_error(87);return 0;}h4_need(src,n*(wide?2:1),0);
 if(cap&&cap<n){void_set_last_error(122);return 0;}if(cap)h4_need(dst,n*(wide?1:2),1);
 int used=0;uint8_t def='?';if(wide&&h4_arg(7)){h4_need(h4_arg(7),1,0);def=MEM8(h4_arg(7));}
 for(uint32_t i=0;i<n;i++){
  if(wide){uint8_t b;uint16_t u=MEM16(src+2*i);int ok=cp==1251?void_u16_to_ansi(u,&b):void_u16_to_cp1252(u,&b);if(!ok){b=def;used=1;}if(cap)MEM8(dst+i)=b;}
  else if(cap)MEM16(dst+2*i)=cp==1251?void_ansi_to_u16(MEM8(src+i)):void_cp1252_to_u16(MEM8(src+i));
 }
 if(wide&&h4_arg(8)){h4_need(h4_arg(8),4,1);MEM32(h4_arg(8))=used;}return n;
}
/* shortcut: one guest thread; unsatisfied blocking waits stop until a scheduler exists. */
typedef struct {unsigned kind,refs,state,manual,owner,recursion;char *name;} object;
static object objects[256];
static object *object_get(uint32_t handle) {unsigned i=handle-0x30000000u;if(i>=256||!objects[i].refs)return NULL;return &objects[i];}
static uint32_t object_create(unsigned kind,uint32_t name,uint32_t manual,uint32_t state) {
 const char *n=name?h4_string(name):NULL;
 for(unsigned i=0;n&&i<256;i++)if(objects[i].refs&&objects[i].name&&!strcmp(n,objects[i].name)){
  if(objects[i].kind!=kind){void_set_last_error(6);return 0;}objects[i].refs++;void_set_last_error(183);return 0x30000000u+i;}
 for(unsigned i=0;i<256;i++)if(!objects[i].refs){objects[i]=(object){.kind=kind,.refs=1,.state=state,.manual=manual,.name=n?strdup(n):NULL};if(kind==2&&state){objects[i].owner=1;objects[i].recursion=1;}void_set_last_error(0);return 0x30000000u+i;}
 void_set_last_error(8);return 0;
}
static uint32_t object_wait(uint32_t h,uint32_t ms) {
 object *o=object_get(h);if(!o){void_set_last_error(6);return 0xffffffff;}
 if(o->kind==1&&o->state){if(!o->manual)o->state=0;return 0;}
 if(o->kind==2&&(!o->owner||o->owner==1)){o->owner=1;o->recursion++;return 0;}
 if(ms==0)return 258;
 void_log("unsupported blocking wait handle=%08x timeout=%u from %08x",h,ms,g_cur_func);exit(77);
}
/* Guest environment is isolated from host control variables and credentials. */
static struct {char *name,*value;} environment[128];
static uint32_t environment_set(uint32_t name,uint32_t value) {
 const char *n=h4_string(name);if(!*n||(strchr(n,'=')&&!(n[0]=='='&&n[1]&&n[2]==':'&&!n[3]))){void_set_last_error(87);return 0;}
 unsigned i=0;while(i<128&&(!environment[i].name||strcasecmp(n,environment[i].name)))i++;
 if(!value){if(i<128){free(environment[i].name);free(environment[i].value);environment[i].name=environment[i].value=NULL;}return 1;}
 if(i==128){i=0;while(i<128&&environment[i].name)i++;if(i==128){void_set_last_error(8);return 0;}}
 char *copy=strdup(h4_string(value)),*key=strdup(n);if(!copy||!key){free(copy);free(key);void_set_last_error(8);return 0;}free(environment[i].name);free(environment[i].value);environment[i].name=key;environment[i].value=copy;return 1;
}
static uint32_t environment_get(uint32_t name,uint32_t out,uint32_t cap) {
 const char *n=h4_string(name);for(unsigned i=0;i<128;i++)if(environment[i].name&&!strcasecmp(n,environment[i].name)){unsigned size=(unsigned)strlen(environment[i].value);if(cap<=size)return size+1;h4_need(out,size+1,1);memcpy(GPTR(out),environment[i].value,size+1);void_set_last_error(0);return size;}
 void_set_last_error(203);return 0;
}
static uint32_t environment_block(int wide) {
 size_t n=1;for(unsigned i=0;i<128;i++)if(environment[i].name)n+=strlen(environment[i].name)+strlen(environment[i].value)+2;if(n==1)n=2;
 uint32_t out=void_galloc((uint32_t)n*(wide?2u:1u));if(!out)return 0;unsigned offset=0;
 for(unsigned i=0;i<128;i++)if(environment[i].name){const char *parts[]={environment[i].name,"=",environment[i].value};for(unsigned k=0;k<3;k++)for(const unsigned char *p=(const unsigned char*)parts[k];*p;p++){if(wide)MEM16(out+2*offset)=void_cp1252_to_u16(*p);else MEM8(out+offset)=*p;offset++;}offset++;}
 return out;
}
/* Diagnostic only: reject malformed or cyclic guest chains before dereferencing. */
static void trace_seh(void) {
 uint32_t rec=MEM32(g_fs_base),seen[64];unsigned n=0;
 while(rec&&rec!=0xffffffffu&&n<64){
  for(unsigned i=0;i<n;i++)if(seen[i]==rec){void_log("SEH chain cycle at %08x",rec);return;}
  if(!void_vm_readable(rec,12)){void_log("SEH invalid frame %08x",rec);return;}
  seen[n++]=rec;uint32_t handler=MEM32(rec+4);
  void_log("SEH frame=%08x next=%08x handler=%08x state=%08x",rec,MEM32(rec),handler,MEM32(rec+8));
  if(void_vm_readable(handler,11)&&MEM8(handler)==0xb8){uint32_t fi=MEM32(handler+1);if(void_vm_readable(fi,20))void_log("SEH candidate FuncInfo=%08x magic=%08x max=%u unwind=%08x tries=%u trymap=%08x",fi,MEM32(fi),MEM32(fi+4),MEM32(fi+8),MEM32(fi+12),MEM32(fi+16));}
  rec=MEM32(rec);
 }
 if(n==64)void_log("SEH chain exceeds diagnostic limit");
}
static int eh_dispatch_enabled(void){
#ifdef H4_TESTED_EH
 return eh_frame_count!=0;
#else
 return selftesting&&eh_frame_count;
#endif
}
static void invoke(void) {
 const char *full=active_import,*s=strchr(full,'!');s=s?s+1:full;
 uint32_t a,b,c,d,value=0;unsigned count=0;
 ++api_calls;
 if(!strcasecmp(full,"DSOUND.dll!ordinal_1")||!strcasecmp(full,"DSOUND.dll!DirectSoundCreate")){h4_need(h4_arg(2),4,1);MEM32(h4_arg(2))=0;void_log("DirectSound backend unavailable: returning DSERR_NODRIVER, no fake device");finish(0x88780078u,3);return;}

 if(getenv("H4_TRACE_API"))void_log("API %s from %08x esp=%08x",full,g_cur_func,g_esp);
#define API(name,n) if(!strcmp(s,name)) {count=n;
#define NEXT } else
 API("GetVersion",0) value=0x0a280105;
 NEXT API("_BinkSetSoundSystem@8",2) void_log("Bink audio unavailable: returning initialization failure");value=0;
 NEXT API("_BinkSetSoundTrack@4",1) void_log("Bink optional soundtrack selection %u (no decoder active)",h4_arg(1));
 NEXT API("_BinkOpen@8",2) void_log("Bink decoder unavailable: returning NULL for optional intro (flags=%08x)",h4_arg(2));value=0;
 NEXT API("CoInitializeEx",2) a=h4_arg(2)&2u;if(h4_arg(1)||(h4_arg(2)&~14u))value=0x80070057;else if(com_refs&&a!=com_mode)value=0x80010106;else{value=com_refs?1:0;com_refs++;com_mode=a;}
 NEXT API("CoUninitialize",0) if(com_refs)--com_refs;
 NEXT API("CoCreateInstance",5) h4_need(h4_arg(1),16,0);h4_need(h4_arg(4),16,0);h4_need(h4_arg(5),4,1);MEM32(h4_arg(5))=0;void_log("COM class not registered in this host: %08x-%08x-%08x-%08x",MEM32(h4_arg(1)),MEM32(h4_arg(1)+4),MEM32(h4_arg(1)+8),MEM32(h4_arg(1)+12));value=0x80040154;
 NEXT API("MessageBoxA",4) void_log("original MessageBoxA flags=%08x title=%s text=%s",h4_arg(4),h4_arg(3)?h4_string(h4_arg(3)):"",h4_arg(2)?h4_string(h4_arg(2)):"");void_log("stopping before dialog response; no success is simulated");exit(77);
 NEXT API("RaiseException",4) if(eh_dispatch_enabled()){uint32_t record=h4_arg(4);if(h4_arg(1)!=0xe06d7363u||h4_arg(2)!=1||h4_arg(3)!=3)void_fatal("unsupported EH exception record");h4_need(record,12,0);if(MEM32(record)!=0x19930520u)void_fatal("unsupported EH exception magic");g_esp+=4;eh_throw(MEM32(record+4),MEM32(record+8));void_fatal("EH dispatch unexpectedly returned");}void_log("original RaiseException code=%08x flags=%08x nargs=%u args=%08x caller=%08x",h4_arg(1),h4_arg(2),h4_arg(3),h4_arg(4),g_cur_func);if(h4_arg(3)<=15&&h4_arg(4)){h4_need(h4_arg(4),h4_arg(3)*4,0);for(unsigned i=0;i<h4_arg(3);i++)void_log("exception arg[%u]=%08x",i,MEM32(h4_arg(4)+4*i));}trace_seh();if(h4_arg(3)==3&&h4_arg(4)&&MEM32(h4_arg(4))==0x19930520u){uint32_t rec=MEM32(g_fs_base);if(void_vm_readable(rec,12)){uint32_t handler=MEM32(rec+4);if(void_vm_readable(handler,11)&&MEM8(handler)==0xb8){uint32_t fi=MEM32(handler+1);uint32_t chosen=eh_select(fi,(int32_t)MEM32(rec+8),MEM32(h4_arg(4)+8));void_log("EH selected original catch=%08x (not executed)",chosen);}}}void_log("SEH dispatch is not implemented; stopping instead of reporting success");exit(77);
 NEXT API("DirectDrawCreateEx",4) value=h4_directdraw_create(h4_arg(1),h4_arg(2),h4_arg(3),h4_arg(4));
 NEXT API("LoadImageA",6) if(h4_arg(3)==1||h4_arg(3)==2){void_log("optional icon/cursor LoadImageA unsupported, returning NULL/ERROR_NOT_SUPPORTED");void_set_last_error(50);}else void_fatal("unsupported LoadImageA type %u",h4_arg(3));
 NEXT API("GetFileVersionInfoSizeA",2) if(h4_arg(2)){h4_need(h4_arg(2),4,1);MEM32(h4_arg(2))=0;}value=h4_version_size(h4_string(h4_arg(1)));
 NEXT API("GetFileVersionInfoA",4) value=h4_version_copy(h4_string(h4_arg(1)),h4_arg(3),h4_arg(4));
 NEXT API("VerQueryValueA",4) value=h4_version_query(h4_arg(1),h4_string(h4_arg(2)),h4_arg(3),h4_arg(4));
 NEXT API("GetLocalTime",1) struct timespec t;if(clock_gettime(CLOCK_REALTIME,&t))void_fatal("realtime clock failed");system_time(h4_arg(1),t.tv_sec,(unsigned)(t.tv_nsec/1000000));
 NEXT API("GetSystemTime",1) struct timespec t;if(clock_gettime(CLOCK_REALTIME,&t))void_fatal("realtime clock failed");system_time(h4_arg(1),t.tv_sec,(unsigned)(t.tv_nsec/1000000));
 NEXT API("GetTimeZoneInformation",1) a=h4_arg(1);h4_need(a,172,1);memset(GPTR(a),0,172);MEM16(a+4)='U';MEM16(a+6)='T';MEM16(a+8)='C';
 NEXT API("FileTimeToLocalFileTime",2) h4_need(h4_arg(1),8,0);h4_need(h4_arg(2),8,1);memmove(GPTR(h4_arg(2)),GPTR(h4_arg(1)),8);value=1;
 NEXT API("timeBeginPeriod",1) a=h4_arg(1);struct timespec resolution;if(a<1||a>1000||clock_getres(CLOCK_MONOTONIC,&resolution)||resolution.tv_sec||resolution.tv_nsec>(long)a*1000000){value=97;}else timer_period_refs[a]++;
 NEXT API("timeEndPeriod",1) a=h4_arg(1);if(a<1||a>1000||!timer_period_refs[a])value=97;else timer_period_refs[a]--;
 NEXT API("timeGetTime",0) struct timespec t;if(clock_gettime(CLOCK_MONOTONIC,&t))void_fatal("monotonic clock failed");value=(uint32_t)((uint64_t)t.tv_sec*1000+t.tv_nsec/1000000);
 NEXT API("GetSystemTimeAsFileTime",1) struct timespec t;if(clock_gettime(CLOCK_REALTIME,&t))void_fatal("clock_gettime failed");uint64_t ft=((uint64_t)t.tv_sec+11644473600ull)*10000000ull+t.tv_nsec/100;h4_need(h4_arg(1),8,1);MEM32(h4_arg(1))=(uint32_t)ft;MEM32(h4_arg(1)+4)=(uint32_t)(ft>>32);
 NEXT API("GlobalMemoryStatus",1) h4_need(h4_arg(1),32,1);h4_memory_status(h4_arg(1));
 NEXT API("GetSystemInfo",1) a=h4_arg(1);h4_need(a,36,1);memset(GPTR(a),0,36);MEM32(a+4)=4096;MEM32(a+8)=0x10000;MEM32(a+12)=GUEST_USER_TOP-1;MEM32(a+16)=1;MEM32(a+20)=1;MEM32(a+24)=586;MEM32(a+28)=65536;MEM16(a+32)=6;
 NEXT API("GetVersionExA",1) a=h4_arg(1);h4_need(a,4,0);b=MEM32(a);if(b!=148&&b!=156){void_set_last_error(87);}else{h4_need(a,b,1);memset(GPTR(a+4),0,b-4);MEM32(a+4)=5;MEM32(a+8)=1;MEM32(a+12)=2600;MEM32(a+16)=2;if(b==156){MEM16(a+148)=3;MEM8(a+154)=1;}value=1;}
 NEXT API("SetUnhandledExceptionFilter",1) value=exception_filter;exception_filter=h4_arg(1);
 NEXT API("InterlockedIncrement",1) a=h4_arg(1);h4_need(a,4,1);value=++MEM32(a);
 NEXT API("InterlockedDecrement",1) a=h4_arg(1);h4_need(a,4,1);value=--MEM32(a);
 NEXT API("InterlockedExchange",2) a=h4_arg(1);h4_need(a,4,1);value=MEM32(a);MEM32(a)=h4_arg(2);
 NEXT API("GetLastError",0) value=last_error;
 NEXT API("SetLastError",1) void_set_last_error(h4_arg(1));
 NEXT API("HeapCreate",3) flags(h4_arg(1),1);value=void_heap_create(h4_arg(2),h4_arg(3));
 NEXT API("HeapDestroy",1) value=void_heap_destroy(h4_arg(1));
 NEXT API("GetProcessHeap",0) value=void_process_heap();
 NEXT API("HeapAlloc",3) flags(h4_arg(2),9);value=void_heap_alloc(h4_arg(1),h4_arg(3),h4_arg(2)&8);
 NEXT API("HeapFree",3) flags(h4_arg(2),1);value=void_heap_free(h4_arg(1),h4_arg(3));
 NEXT API("HeapSize",3) flags(h4_arg(2),1);value=void_heap_size(h4_arg(1),h4_arg(3));
 NEXT API("HeapReAlloc",4) flags(h4_arg(2),25);value=void_heap_realloc(h4_arg(1),h4_arg(3),h4_arg(4),h4_arg(2)&8,h4_arg(2)&16);
 NEXT API("VirtualAlloc",4) value=void_vm_alloc(h4_arg(1),h4_arg(2),h4_arg(3),h4_arg(4));
 NEXT API("VirtualFree",3) value=void_vm_free(h4_arg(1),h4_arg(2),h4_arg(3));
 NEXT API("VirtualProtect",4) h4_need(h4_arg(4),4,1);value=void_vm_protect(h4_arg(1),h4_arg(2),h4_arg(3),(uint32_t*)GPTR(h4_arg(4)));
 NEXT API("VirtualQuery",3) if(h4_arg(3)<28){void_set_last_error(24);}else{h4_need(h4_arg(2),28,1);value=void_vm_query(h4_arg(1),h4_arg(2));}
 NEXT API("CreateEventA",4) if(h4_arg(1)){void_set_last_error(50);}else value=object_create(1,h4_arg(4),!!h4_arg(2),!!h4_arg(3));
 NEXT API("CreateMutexA",3) if(h4_arg(1)){void_set_last_error(50);}else value=object_create(2,h4_arg(3),0,!!h4_arg(2));
 NEXT API("WaitForSingleObject",2) value=object_wait(h4_arg(1),h4_arg(2));
 NEXT API("SetEvent",1) object *o=object_get(h4_arg(1));if(o&&o->kind==1){o->state=1;value=1;}else void_set_last_error(6);
 NEXT API("ResetEvent",1) object *o=object_get(h4_arg(1));if(o&&o->kind==1){o->state=0;value=1;}else void_set_last_error(6);
 NEXT API("ReleaseMutex",1) object *o=object_get(h4_arg(1));if(o&&o->kind==2&&o->owner==1&&o->recursion){if(!--o->recursion)o->owner=0;value=1;}else void_set_last_error(288);
 NEXT API("CloseHandle",1) object *o=object_get(h4_arg(1));if(o){if(!--o->refs){free(o->name);memset(o,0,sizeof *o);}value=1;}else if(h4_file_close(h4_arg(1)))value=1;else void_set_last_error(6);
 NEXT API("InitializeCriticalSection",1) a=h4_arg(1);h4_need(a,24,1);memset(GPTR(a),0,24);MEM32(a)=MEM32(a+4)=0xffffffff;
 NEXT API("EnterCriticalSection",1) a=h4_arg(1);h4_need(a,24,1);if(MEM32(a+12)&&MEM32(a+12)!=1)void_fatal("critical section owner unsupported");MEM32(a+12)=1;MEM32(a+8)++;MEM32(a+4)++;
 NEXT API("LeaveCriticalSection",1) a=h4_arg(1);h4_need(a,24,1);if(MEM32(a+12)!=1||!MEM32(a+8))void_fatal("critical section not owned");if(!--MEM32(a+8))MEM32(a+12)=0;MEM32(a+4)--;
 NEXT API("DeleteCriticalSection",1) a=h4_arg(1);h4_need(a,24,1);if(MEM32(a+8))void_fatal("delete owned critical section");memset(GPTR(a),0,24);
 NEXT API("TlsAlloc",0) value=tls_alloc();
 NEXT API("TlsGetValue",1) value=tls_get(h4_arg(1));
 NEXT API("TlsSetValue",2) value=tls_set(h4_arg(1),h4_arg(2));
 NEXT API("TlsFree",1) a=h4_arg(1);if(a<1088&&tls_used[a]){tls_used[a]=0;tls_value[a]=0;value=1;}else void_set_last_error(87);
 NEXT API("GetCurrentThreadId",0) value=1;
 NEXT API("GetCurrentProcess",0) value=0xffffffff;
 NEXT API("LCMapStringW",6) a=h4_arg(2);if(a!=0x100&&a!=0x200)void_fatal("unsupported LCMapStringW flags %08x",a);b=h4_arg(3);int32_t len=(int32_t)h4_arg(4);c=0;if(len==-1){do{h4_need(b+2*c,2,0);++c;}while(MEM16(b+2*(c-1))&&c<1048576);}else if(len>0)c=(uint32_t)len;if(!c||c>1048576){void_set_last_error(87);}else{h4_need(b,2*c,0);if(h4_arg(6)==0)value=c;else if(h4_arg(6)<c)void_set_last_error(122);else{h4_need(h4_arg(5),2*c,1);for(d=0;d<c;d++){uint16_t u=MEM16(b+2*d);MEM16(h4_arg(5)+2*d)=(uint16_t)(a==0x100?towlower(u):towupper(u));}value=c;}}
 NEXT API("GetStringTypeW",4) if(h4_arg(1)!=1){void_set_last_error(50);}else{a=h4_arg(2);int32_t len=(int32_t)h4_arg(3);b=0;if(len==-1){do{h4_need(a+2*b,2,0);++b;}while(MEM16(a+2*(b-1))&&b<1048576);}else if(len>0)b=(uint32_t)len;if(!b||b>1048576){void_set_last_error(87);}else{h4_need(a,b*2,0);h4_need(h4_arg(4),b*2,1);for(c=0;c<b;c++)MEM16(h4_arg(4)+2*c)=ctype1(MEM16(a+2*c));value=1;}}
 NEXT API("WideCharToMultiByte",8) value=convert(1);
 NEXT API("MultiByteToWideChar",6) value=convert(0);
 NEXT API("IsValidCodePage",1) a=h4_arg(1);value=a==0||a==1251||a==1252;
 NEXT API("GetCPInfo",2) a=h4_arg(1);if(a==0||a==1251||a==1252){b=h4_arg(2);h4_need(b,20,1);memset(GPTR(b),0,20);MEM32(b)=1;MEM8(b+4)='?';value=1;}else void_set_last_error(87);
 NEXT API("GetACP",0) value=1252;
 NEXT API("GetOEMCP",0) value=437;
 NEXT API("SetHandleCount",1) value=h4_arg(1);
 NEXT API("GetCommandLineA",0) static uint32_t cmd;if(!cmd)cmd=void_gstrdup("\"C:\\HeroesIV\\heroes4.exe\"");value=cmd;
 NEXT API("GetModuleHandleA",1) if(!h4_arg(1)||!strcasecmp(h4_string(h4_arg(1)),"heroes4.exe"))value=H4_IMAGE_BASE;else void_set_last_error(126);
 NEXT API("GetModuleFileNameA",3) const char *p="C:\\HeroesIV\\heroes4.exe";a=h4_arg(3);if(h4_arg(1)&&h4_arg(1)!=H4_IMAGE_BASE)void_set_last_error(126);else if(a){h4_need(h4_arg(2),a,1);b=(uint32_t)strlen(p);if(b>=a){b=a-1;void_set_last_error(122);}memcpy(GPTR(h4_arg(2)),p,b);MEM8(h4_arg(2)+b)=0;value=b;}
 NEXT API("GetStartupInfoA",1) a=h4_arg(1);h4_need(a,68,1);memset(GPTR(a),0,68);MEM32(a)=68;MEM32(a+48)=1;
 NEXT API("SetEnvironmentVariableA",2) value=environment_set(h4_arg(1),h4_arg(2));
 NEXT API("GetEnvironmentVariableA",3) value=environment_get(h4_arg(1),h4_arg(2),h4_arg(3));
 NEXT API("GetEnvironmentStringsW",0) value=environment_block(1);
 NEXT API("GetEnvironmentStrings",0) value=environment_block(0);
 NEXT API("FreeEnvironmentStringsW",1) value=void_heap_free(void_process_heap(),h4_arg(1));
 NEXT API("FreeEnvironmentStringsA",1) value=void_heap_free(void_process_heap(),h4_arg(1));
 NEXT API("GetStdHandle",1) a=h4_arg(1);value=(a>=0xfffffff4u&&a<=0xfffffff6u)?0x100+(0xfffffff6u-a):0xffffffff;
 NEXT API("GetFileType",1) a=h4_arg(1);if(a>=0x100&&a<=0x102)value=2;else if(!(value=h4_file_type(a)))void_set_last_error(6);
 NEXT API("IsBadReadPtr",2) value=!void_vm_readable(h4_arg(1),h4_arg(2));
 NEXT API("IsBadWritePtr",2) value=!void_vm_writable(h4_arg(1),h4_arg(2));
 NEXT API("IsBadCodePtr",1) value=h4_find_lifted(h4_arg(1))==NULL;
 NEXT API("OutputDebugStringA",1) void_log("guest: %s",h4_string(h4_arg(1)));
 NEXT API("ExitProcess",1) void_log("original ExitProcess(%u), caller %08x, API calls %llu",h4_arg(1),g_cur_func,(unsigned long long)api_calls);exit(h4_arg(1)?79:0);
 NEXT {if(h4_window_api(s,&value,&count)||h4_gdi_api(s,&value,&count)||h4_registry_api(s,&value,&count)||h4_file_api(s,&value,&count)){finish(value,count);return;}void_log("unimplemented Win32 import %s from %08x (IAT %08x, calls %llu)",full,g_cur_func,active_iat,(unsigned long long)api_calls);exit(77);}
#undef API
#undef NEXT
 (void)b;(void)c;(void)d;finish(value,count);
}
recomp_func_t void_resolve(uint32_t va) {
 if(selftesting&&va==0xdfff0000u)return test_wndproc;
 if(selftesting&&va==0xdfff0004u)return test_cleanup_a;
 if(selftesting&&va==0xdfff0008u)return test_cleanup_b;
 if(selftesting&&va==0xdfff0010u)return test_catch_first;
 if(selftesting&&va==0xdfff0014u)return test_catch_second;
 recomp_func_t fn=h4_ddraw_resolve(va);if(fn)return fn;fn=h4_find_lifted(va);if(fn)return fn;
 if(va>=0xe0000000u && !(va&3) && (va-0xe0000000u)/4<H4_IMPORT_COUNT){unsigned i=(va-0xe0000000u)/4;active_import=h4_import_names[i].name;active_iat=h4_import_names[i].iat;return invoke;}
 void_log("unsupported dispatch %08x from %08x",va,g_cur_func);exit(77);
}
static void fault(int sig,siginfo_t *info,void *context) {
 (void)context; /* Diagnostic only: never resume a guest memory fault. */
 char buf[180];int n=snprintf(buf,sizeof buf,"guest fault signal=%d host=%p guest=%08x function=%08x esp=%08x eax=%08x ecx=%08x edx=%08x\n",sig,info->si_addr,(uint32_t)((uintptr_t)info->si_addr-(uintptr_t)h4_guest_base),g_cur_func,g_esp,g_eax,g_ecx,g_edx);write(2,buf,(size_t)n);_exit(80);
}
static void init(void) {
 if(!setlocale(LC_CTYPE,"C.UTF-8")&&!setlocale(LC_CTYPE,"en_US.UTF-8"))void_fatal("UTF-8 host locale unavailable");
 if(!void_guest_reserve())exit(2);atexit(execution_summary);
 struct sigaction sa={0};sa.sa_sigaction=fault;sa.sa_flags=SA_SIGINFO;sigemptyset(&sa.sa_mask);sigaction(SIGSEGV,&sa,0);sigaction(SIGBUS,&sa,0);
 if(!void_vm_claim(0x08000000,0x800000,4)||!void_vm_claim(0x7ffd0000,0x10000,4))void_fatal("cannot allocate startup stack/TEB");
 g_fs_base=0x7ffde000;MEM32(g_fs_base)=0xffffffff;MEM32(g_fs_base+4)=0x08800000;MEM32(g_fs_base+8)=0x08000000;MEM32(g_fs_base+0x18)=g_fs_base;MEM32(g_fs_base+0x24)=1;
 g_esp=0x087fffc0;MEM32(g_esp)=0;
}
uint32_t h4_test_api(const char *name,unsigned count,...) {
 uint32_t sp=g_esp;va_list ap;va_start(ap,count);for(unsigned i=1;i<=count;i++)MEM32(sp+4*i)=va_arg(ap,uint32_t);va_end(ap);
 active_import=name;invoke();assert(g_esp==sp+4*(count+1));g_esp=sp;return g_eax;
}
void h4_test_version(void);
void h4_test_registry(int read_only);
void h4_test_files(void);
void h4_test_menu_setup(uint32_t wc);
void h4_test_menu(uint32_t window);
void h4_test_push_key_pair(uint32_t key);
void h4_test_ddraw(void);
static void window_selftest(void) {
 uint32_t wc=void_galloc(48),name=void_gstrdup("SyntheticH4Window"),rect=void_galloc(96);
 h4_test_menu_setup(wc);MEM32(wc)=48;MEM32(wc+8)=0xdfff0000;MEM32(wc+40)=name;
 uint32_t atom=h4_test_api("USER32.dll!RegisterClassExA",1,wc);assert(atom);assert(!h4_test_api("USER32.dll!RegisterClassExA",1,wc));
 uint32_t win=h4_test_api("USER32.dll!CreateWindowExA",12,0u,atom,name,0u,0u,0u,320u,200u,0u,0u,H4_IMAGE_BASE,0u);assert(win==0x50000001&&callback_tests==2);h4_test_menu(win);
 MEM32(rect)=12;MEM32(rect+4)=34;assert(h4_test_api("USER32.dll!ClientToScreen",2,win,rect)&&MEM32(rect)==12&&MEM32(rect+4)==34);assert(h4_test_api("USER32.dll!BeginPaint",2,win,rect)==0x51000000&&MEM32(rect+16)==320&&MEM32(rect+20)==200);assert(h4_test_api("USER32.dll!EndPaint",2,win,rect));assert(h4_test_api("USER32.dll!GetCursorPos",1,rect));assert(!h4_test_api("USER32.dll!GetKeyState",1,0x1bu));
 assert(h4_test_api("USER32.dll!GetClientRect",2,win,rect)&&MEM32(rect+8)==320&&MEM32(rect+12)==200);
 assert(!h4_test_api("USER32.dll!SetWindowLongA",3,win,0xffffffebu,42u));assert(h4_test_api("USER32.dll!SetWindowLongA",3,win,0xffffffebu,0u)==42);
 h4_test_api("USER32.dll!ShowWindow",2,win,1u);assert(h4_test_api("USER32.dll!GetActiveWindow",0)==win);
 assert(!h4_test_api("USER32.dll!MsgWaitForMultipleObjects",5,0u,0u,0u,0u,0xffu));
 unsigned messages=0;while(h4_test_api("USER32.dll!PeekMessageA",5,rect,0u,0u,0u,1u)){assert(++messages<128);h4_test_api("USER32.dll!DispatchMessageA",1,rect);}
 unsigned key_messages=0;h4_test_push_key_pair(27);while(h4_test_api("USER32.dll!PeekMessageA",5,rect,0u,0u,0u,1u)){uint32_t msg=MEM32(rect+4);if(msg==0x100||msg==0x101){assert(MEM32(rect+8)==0x1b);assert(h4_test_api("USER32.dll!GetKeyState",1,0x1bu)==(msg==0x100?0x8000u:0));key_messages++;}h4_test_api("USER32.dll!DispatchMessageA",1,rect);}assert(key_messages==2);
 unsigned char_messages=0;h4_test_push_key_pair('n');while(h4_test_api("USER32.dll!PeekMessageA",5,rect,0u,0u,0u,1u)){if(MEM32(rect+4)==0x102){assert(MEM32(rect+8)=='n');char_messages++;}h4_test_api("USER32.dll!TranslateMessage",1,rect);h4_test_api("USER32.dll!DispatchMessageA",1,rect);}assert(char_messages==1);
 assert(h4_test_api("USER32.dll!MsgWaitForMultipleObjects",5,0u,0u,0u,0u,0xffu)==258);
 assert(messages>=3&&callback_tests>=5);h4_test_ddraw();assert(h4_test_api("USER32.dll!DestroyWindow",1,win));assert(!h4_test_api("USER32.dll!GetClientRect",2,win,rect));
 assert(void_vm_free(H4_IMAGE_BASE,0,0x8000));void_gfree(wc);void_gfree(name);void_gfree(rect);puts("PASS SDL window, messages and synthetic x86 stdcall callbacks");
}
static void selftest(void) {
 uint32_t h=void_heap_create(4096,0);assert(h);uint32_t p=void_heap_alloc(h,33,1);assert(p&&!(p&7)&&void_heap_size(h,p)==33);
 for(unsigned i=0;i<33;i++)assert(!MEM8(p+i));memset(GPTR(p),0xa5,33);
 p=void_heap_realloc(h,p,8192,1,0);assert(p&&void_heap_size(h,p)==8192);for(unsigned i=0;i<8192;i++)assert(MEM8(p+i)==(i<33?0xa5:0));
 uint32_t h2=void_heap_create(0,0);assert(h2);assert(!void_heap_free(h2,p));assert(void_heap_size(h2,p)==0xffffffff);assert(!void_heap_realloc(h,p,0xffffffff,0,0));assert(MEM8(p)==0xa5);
 assert(void_heap_free(h,p));assert(!void_heap_free(h,p));assert(void_heap_destroy(h));assert(!void_heap_alloc(h,1,0));assert(void_heap_destroy(h2));
 for(unsigned i=0;i<300;i++){h=void_heap_create(0,0);assert(h&&void_heap_destroy(h));}
 assert(!void_heap_create(4096,1));assert(!void_heap_create(0,4096)&&last_error==50);assert(!void_heap_destroy(void_process_heap()));
 uint32_t v=void_vm_alloc(0,0x20000,0x2000,4);assert(v&&!(v&0xffff));assert(!void_vm_readable(v,1));assert(void_vm_alloc(v+0x1000,3,0x1000,4)==v+0x1000);assert(!MEM32(v+0x1000));MEM32(v+0x1000)=0x12345678;
 uint32_t old;assert(void_vm_protect(v+0x1000,4096,2,&old)&&old==4);assert(void_vm_readable(v+0x1000,4)&&!void_vm_writable(v+0x1000,4));assert(void_vm_protect(v+0x1000,4096,4,&old));
 assert(void_vm_free(v+0x1000,4096,0x4000));assert(void_vm_alloc(v+0x1000,4096,0x1000,4)==v+0x1000&&!MEM32(v+0x1000));assert(void_vm_free(v,0,0x8000));assert(!void_vm_free(v,0,0x8000));
 uint32_t t=tls_alloc();assert(t!=0xffffffff&&!tls_get(t));assert(tls_set(t,0x1234)&&tls_get(t)==0x1234);assert(!tls_set(1088,1)&&last_error==87);
 /* Exercise the same stdcall dispatch used by generated C. */
 active_import="KERNEL32.dll!HeapCreate";MEM32(g_esp+4)=0;MEM32(g_esp+8)=0;MEM32(g_esp+12)=0;uint32_t sp=g_esp;invoke();assert(g_esp==sp+16&&g_eax);assert(void_heap_destroy(g_eax));g_esp=sp;
 uint32_t ev=object_create(1,0,0,0);assert(ev&&object_wait(ev,0)==258);assert(h4_test_api("KERNEL32.dll!SetEvent",1,ev)==1);assert(!object_wait(ev,0)&&object_wait(ev,0)==258);assert(h4_test_api("KERNEL32.dll!CloseHandle",1,ev));assert(object_wait(ev,0)==0xffffffff&&last_error==6);
 ev=object_create(1,0,1,1);assert(!object_wait(ev,0)&&!object_wait(ev,0));assert(h4_test_api("KERNEL32.dll!ResetEvent",1,ev));assert(object_wait(ev,0)==258);assert(h4_test_api("KERNEL32.dll!CloseHandle",1,ev));
 uint32_t mu=object_create(2,0,0,0);assert(!object_wait(mu,0)&&!object_wait(mu,0));assert(h4_test_api("KERNEL32.dll!ReleaseMutex",1,mu));assert(h4_test_api("KERNEL32.dll!ReleaseMutex",1,mu));assert(!h4_test_api("KERNEL32.dll!ReleaseMutex",1,mu)&&last_error==288);assert(h4_test_api("KERNEL32.dll!CloseHandle",1,mu));
 uint32_t cs=void_galloc(24);h4_test_api("KERNEL32.dll!InitializeCriticalSection",1,cs);h4_test_api("KERNEL32.dll!EnterCriticalSection",1,cs);h4_test_api("KERNEL32.dll!EnterCriticalSection",1,cs);assert(MEM32(cs+8)==2&&MEM32(cs+12)==1);h4_test_api("KERNEL32.dll!LeaveCriticalSection",1,cs);h4_test_api("KERNEL32.dll!LeaveCriticalSection",1,cs);assert(!MEM32(cs+12)&&MEM32(cs+4)==0xffffffff);h4_test_api("KERNEL32.dll!DeleteCriticalSection",1,cs);void_gfree(cs);
 assert(!void_vm_alloc(0xfffff000,0x10000,0x3000,4));assert(!void_vm_free(0xfffff000,0x10000,0x4000));assert(!void_vm_protect(0xfffff000,0x10000,4,&old));
 uint32_t text=void_galloc(16),out=void_galloc(32);MEM8(text)=0xdf;MEM8(text+1)=0;assert(h4_test_api("KERNEL32.dll!MultiByteToWideChar",6,1251u,0u,text,0xffffffffu,out,16u)==2&&MEM16(out)==0x42f&&!MEM16(out+2));assert(h4_test_api("KERNEL32.dll!WideCharToMultiByte",8,1251u,0u,out,0xffffffffu,text,16u,0u,0u)==2&&MEM8(text)==0xdf);assert(!h4_test_api("KERNEL32.dll!MultiByteToWideChar",6,1251u,0u,text,0xffffffffu,out,1u)&&last_error==122);MEM16(text)='A';MEM16(text+2)=' ';MEM16(text+4)=0;assert(h4_test_api("KERNEL32.dll!GetStringTypeW",4,1u,text,0xffffffffu,out));assert(MEM16(out)==0x381&&MEM16(out+2)==0x248&&MEM16(out+4)==0x20);assert(h4_test_api("KERNEL32.dll!LCMapStringW",6,0x409u,0x100u,text,0xffffffffu,out,16u)==3&&MEM16(out)=='a');void_gfree(text);void_gfree(out);
 assert(!h4_test_api("ole32.dll!CoInitializeEx",2,0u,2u));assert(h4_test_api("ole32.dll!CoInitializeEx",2,0u,2u)==1);assert(h4_test_api("ole32.dll!CoInitializeEx",2,0u,0u)==0x80010106);h4_test_api("ole32.dll!CoUninitialize",0);h4_test_api("ole32.dll!CoUninitialize",0);
 uint32_t audio_out=void_galloc(4);MEM32(audio_out)=0xffffffffu;assert(h4_test_api("DSOUND.dll!ordinal_1",3,0u,audio_out,0u)==0x88780078u&&!MEM32(audio_out));void_gfree(audio_out);assert(!h4_test_api("binkw32.dll!_BinkSetSoundSystem@8",2,0u,0u));assert(!h4_test_api("binkw32.dll!_BinkOpen@8",2,0u,0u));
 uint32_t em=void_galloc(256);memset(GPTR(em),0,256);uint32_t fi=em,tb=em+24,hs=em+44,info=em+64,arr=em+80,ct=em+96,td=em+112,other=em+144;MEM32(fi+12)=1;MEM32(fi+16)=tb;MEM32(tb)=2;MEM32(tb+4)=4;MEM32(tb+12)=1;MEM32(tb+16)=hs;MEM32(hs+4)=td;MEM32(hs+12)=0x1234;MEM32(info+12)=arr;MEM32(arr)=1;MEM32(arr+4)=ct;MEM32(ct+4)=other;memcpy(GPTR(td+8),"._N",4);memcpy(GPTR(other+8),"._N",4);int32_t found_low=-9,found_high=-9;uint32_t found_handler=0;assert(eh_select_detail(fi,4,info,&found_low,&found_high,&found_handler)==0x1234&&found_low==2&&found_high==4&&found_handler==hs);assert(!eh_select(fi,5,info));MEM8(other+9)='X';assert(!eh_select(fi,4,info));MEM32(hs+4)=0;assert(eh_select(fi,4,info)==0x1234);void_gfree(em);puts("PASS EH typed matching, non-match and catch-all selection");
 uint32_t uf=void_galloc(80);memset(GPTR(uf),0,80);cleanup_rec=uf+16;cleanup_result=uf+32;MEM32(uf+4)=3;MEM32(uf+8)=uf+40;MEM32(uf+40)=0xffffffffu;MEM32(uf+44)=0xdfff0004u;MEM32(uf+48)=0;MEM32(uf+52)=0xdfff0008u;MEM32(uf+56)=1;MEM32(cleanup_rec+8)=2;uint32_t saved_sp=g_esp,saved_bp=g_ebp,saved_bx=g_ebx;g_esp=0x08001000;eh_unwind(cleanup_rec,uf,0);assert(cleanup_count==1&&MEM32(cleanup_result)==2&&MEM32(cleanup_rec+8)==0);eh_unwind(cleanup_rec,uf,-1);assert(cleanup_count==2&&MEM32(cleanup_result)==21&&MEM32(cleanup_rec+8)==0xffffffffu&&g_esp==0x08001000&&g_ebp==saved_bp&&g_ebx==saved_bx);g_esp=saved_sp;eh_unwind(cleanup_rec,uf,-1);assert(cleanup_count==2);void_gfree(uf);puts("PASS EH unwind cleanup order, effects, stack and no extra calls");
 uint32_t env_name=void_gstrdup("H4_STATE_DIR"),env_value=void_gstrdup("C:\\Different"),env_out=void_galloc(64);const char *host_state=getenv("H4_STATE_DIR");assert(h4_test_api("KERNEL32.dll!SetEnvironmentVariableA",2,env_name,env_value));assert(getenv("H4_STATE_DIR")==host_state);assert(h4_test_api("KERNEL32.dll!GetEnvironmentVariableA",3,env_name,env_out,64u)==12&&!strcmp(GPTR(env_out),"C:\\Different"));uint32_t block=h4_test_api("KERNEL32.dll!GetEnvironmentStringsW",0);assert(block&&MEM16(block)=='H');assert(h4_test_api("KERNEL32.dll!FreeEnvironmentStringsW",1,block));assert(h4_test_api("KERNEL32.dll!SetEnvironmentVariableA",2,env_name,0u));assert(!h4_test_api("KERNEL32.dll!GetEnvironmentVariableA",3,env_name,env_out,64u));void_gfree(env_name);void_gfree(env_value);void_gfree(env_out);
 h4_test_version();window_selftest();h4_test_registry(0);h4_test_files();
 puts("PASS guest heap, VM, TLS, event, mutex, critical section, CP1251/1252, CTYPE1 and stdcall tests");
}
int main(int argc,char **argv) {
 if(argc!=2&&argc!=3){fputs("usage: bootstrap --selftest | image.bin\n",stderr);return 2;}init();if(!strcmp(argv[1],"--selftest")){selftesting=1;selftest();return 0;}
 if(!strcmp(argv[1],"--registry-read-test")){h4_test_registry(1);return 0;}
 if(!strcmp(argv[1],"--seh-trace-test")){uint32_t old=MEM32(g_fs_base),rec=void_galloc(12);MEM32(rec)=rec;MEM32(rec+4)=0;MEM32(rec+8)=4;MEM32(g_fs_base)=rec;trace_seh();MEM32(g_fs_base)=old;void_gfree(rec);return 0;}
 if(!strcmp(argv[1],"--eh-state-at-max")||!strcmp(argv[1],"--eh-target-at-max")){uint32_t p=void_galloc(48);MEM32(p+4)=1;MEM32(p+8)=p+32;MEM32(p+32)=0xffffffffu;MEM32(p+36)=0;MEM32(p+24)=!strcmp(argv[1],"--eh-state-at-max")?1:0;eh_unwind(p+16,p,!strcmp(argv[1],"--eh-state-at-max")?-1:1);return 3;}
 if(!strcmp(argv[1],"--eh-invalid-unwind")){uint32_t p=void_galloc(48);MEM32(p+4)=1;MEM32(p+8)=p+32;MEM32(p+16+8)=0;MEM32(p+32)=0;MEM32(p+36)=0xdfff0004u;eh_unwind(p+16,p,-1);return 3;}
 if(!strcmp(argv[1],"--eh-dispatch-test")||!strcmp(argv[1],"--eh-binding-test")||!strcmp(argv[1],"--eh-binding-reference-test")||!strcmp(argv[1],"--eh-binding-unsupported-test")){
  selftesting=1;uint32_t p=void_galloc(512);memset(GPTR(p),0,512);uint32_t map=p+24,tries=p+48,h1=p+88,h2=p+104,info=p+120,arr=p+136,ct=p+152,td=p+300,stub=p+200;cleanup_result=p+224;eh_test_effect=p+228;eh_test_record=p+232;MEM32(eh_test_record)=0x19930520u;cleanup_rec=0x08002000;cleanup_count=0;
  MEM32(p)=0x19930520;MEM32(p+4)=3;MEM32(p+8)=map;MEM32(p+12)=2;MEM32(p+16)=tries;
  MEM32(map)=0xffffffffu;MEM32(map+8)=0;MEM32(map+12)=0xdfff0008u;MEM32(map+16)=1;
  MEM32(tries)=1;MEM32(tries+4)=1;MEM32(tries+12)=1;MEM32(tries+16)=h1;MEM32(tries+20)=0;MEM32(tries+24)=2;MEM32(tries+32)=1;MEM32(tries+36)=h2;
  MEM32(h1+4)=td;MEM32(h1+12)=0xdfff0010u;MEM32(h2+4)=td;MEM32(h2+12)=0xdfff0014u;MEM32(info+12)=arr;MEM32(arr)=1;MEM32(arr+4)=ct;MEM32(ct+4)=td;MEM32(ct+12)=0xffffffffu;MEM32(ct+20)=1;memcpy(GPTR(td+8),"._N",4);eh_test_binding=!strcmp(argv[1],"--eh-binding-test")?1:!strcmp(argv[1],"--eh-binding-reference-test")?2:!strcmp(argv[1],"--eh-binding-unsupported-test")?3:0;if(eh_test_binding){MEM32(h2+8)=(uint32_t)-20;MEM8(p+280)=0x7b;eh_test_object=p+280;if(eh_test_binding==2)MEM32(h2)=8;else if(eh_test_binding==3)MEM32(ct+24)=0x1234;}
  MEM8(stub)=0xb8;MEM32(stub+1)=p;MEM8(stub+5)=0xe9;MEM32(cleanup_rec)=0xffffffffu;MEM32(cleanup_rec+4)=stub;MEM32(cleanup_rec+8)=1;MEM32(cleanup_rec-4)=0x08003000;MEM32(g_fs_base)=cleanup_rec;
  g_esp=0x08003000;g_esi=0x12344321;g_ebx=0x11223344;g_edi=0x33445566;recomp_ehframe_t outer={0},frame={0};outer.entry_esp=0x08004000;recomp_eh_push(&outer);frame.entry_esp=cleanup_rec+16;recomp_eh_push(&frame);
  if(!_setjmp(frame.jb)){MEM32(eh_test_record+4)=eh_test_binding?p+280:0x1234;MEM32(eh_test_record+8)=info;uint32_t nested=p+244;MEM32(nested)=0x19930520u;eh_test_record=nested;h4_test_api("KERNEL32.dll!RaiseException",4,0xe06d7363u,1u,3u,p+232);void_fatal("synthetic throw returned");}
  assert(frame.resume==0x12345678&&MEM32(eh_test_effect)==12&&cleanup_count==1&&MEM32(cleanup_result)==2);
  assert(g_esi==0x12344321&&g_ebx==0x11223344&&g_edi==0x33445566&&g_ebp==cleanup_rec+12&&g_esp==0x08003000&&MEM32(g_fs_base)==cleanup_rec&&eh_context_count==0);
  assert(eh_frame_count==2&&frame.prev==&outer);recomp_eh_pop(&frame);recomp_eh_pop(&outer);MEM32(g_fs_base)=0xffffffffu;void_gfree(p);puts("PASS EH dispatcher nested rethrow, cleanup, landing state and registers");return 0;
 }
 if(!strcmp(argv[1],"--eh-cross-frame-test")||!strcmp(argv[1],"--eh-dead-frame-test")){
  selftesting=1;uint32_t p=void_galloc(256);memset(GPTR(p),0,256);uint32_t rec=0x08002000,outer=0x08002100,stub=p+64,info=p+80,arr=p+96,outerfi=p+112,tries=p+136,handler=p+156,outerstub=p+180,map=p+200,outermap=p+232;
  MEM8(stub)=0xb8;MEM32(stub+1)=p;MEM8(stub+5)=0xe9;MEM32(p)=0x19930520;MEM32(p+4)=2;MEM32(p+8)=map;MEM32(map)=0xffffffffu;MEM32(map+4)=0xdfff0004u;MEM32(map+8)=0;MEM32(map+12)=0xdfff0008u;
  MEM32(info+12)=arr;MEM32(rec)=outer;MEM32(rec+4)=stub;MEM32(rec+8)=1;MEM32(outer)=0xffffffffu;MEM32(outer+4)=outerstub;MEM32(outer+8)=0;MEM32(outer-4)=0x08003000;
  MEM8(outerstub)=0xb8;MEM32(outerstub+1)=outerfi;MEM8(outerstub+5)=0xe9;MEM32(outerfi)=0x19930520;MEM32(outerfi+4)=1;MEM32(outerfi+8)=outermap;MEM32(outermap)=0xffffffffu;MEM32(outerfi+12)=1;MEM32(outerfi+16)=tries;MEM32(tries+12)=1;MEM32(tries+16)=handler;MEM32(handler+12)=0xdfff0014u;
  cleanup_rec=rec;cleanup_result=p+224;cleanup_count=0;eh_test_effect=p+240;MEM32(eh_test_effect)=1;MEM32(g_fs_base)=rec;g_esp=0x08003000;g_ebx=0x11223344;g_esi=0x22334455;g_edi=0x33445566;
  recomp_ehframe_t out={0};out.entry_esp=outer+16;recomp_eh_push(&out);if(!_setjmp(out.jb)){recomp_ehframe_t inner={0};inner.entry_esp=rec+16;recomp_eh_push(&inner);if(!strcmp(argv[1],"--eh-dead-frame-test"))recomp_eh_pop(&inner);g_ebx=0xabc;g_esi=0xdef;g_edi=0x789;eh_throw(0x1234,info);}
  assert(out.resume==0x12345678&&MEM32(eh_test_effect)==12&&cleanup_count==2&&MEM32(cleanup_result)==21);assert(g_ebx==0x11223344&&g_esi==0x22334455&&g_edi==0x33445566&&g_ebp==outer+12&&g_esp==0x08003000&&MEM32(g_fs_base)==outer&&eh_frame_count==1);recomp_eh_pop(&out);MEM32(g_fs_base)=0xffffffffu;void_gfree(p);puts("PASS EH cross-frame search, cleanup order, frame lifetime and registers");return 0;
 }
 if(!strcmp(argv[1],"--eh-chain-cycle-test")||!strcmp(argv[1],"--eh-no-handler-test")||!strcmp(argv[1],"--eh-malformed-test")||!strcmp(argv[1],"--eh-metadata-limit-test")){
  uint32_t p=void_galloc(128);memset(GPTR(p),0,128);uint32_t rec=0x08002000,stub=p+64,info=p+80,arr=p+96;
  MEM8(stub)=!strcmp(argv[1],"--eh-malformed-test")?0:0xb8;MEM32(stub+1)=p;MEM8(stub+5)=0xe9;MEM32(p)=0x19930520;if(!strcmp(argv[1],"--eh-metadata-limit-test"))MEM32(p+12)=257;MEM32(info+12)=arr;
  MEM32(rec)=!strcmp(argv[1],"--eh-chain-cycle-test")?rec:0xffffffffu;MEM32(rec+4)=stub;MEM32(rec+8)=0;MEM32(g_fs_base)=rec;eh_throw(0x1234,info);return 3;
 }
 if(!strcmp(argv[1],"--eh-rethrow-outside")){eh_throw(0,0);return 3;}
 if(!strcmp(argv[1],"--unknown-api")){active_import="KERNEL32.dll!UnsupportedSyntheticAPI";invoke();return 3;}
 if(!strcmp(argv[1],"--blocking-wait")){object_wait(object_create(1,0,0,0),0xffffffff);return 3;}
 if(!void_vm_claim(H4_IMAGE_BASE,H4_IMAGE_SIZE,4))void_fatal("image allocation failed");
 FILE *f=fopen(argv[1],"rb");if(!f){perror("image");return 2;}size_t n=fread(GPTR(H4_IMAGE_BASE),1,H4_IMAGE_SIZE,f);int extra=fgetc(f);if(ferror(f)||extra!=EOF||n<512){fclose(f);void_fatal("invalid mapped PE image length");}fclose(f);
 for(unsigned i=0;i<H4_IMPORT_COUNT;i++)MEM32(h4_import_names[i].iat)=0xe0000000u+4*i;
 void_log("original startup ARM64, image=%08x size=%x, host page=%ld",H4_IMAGE_BASE,H4_IMAGE_SIZE,sysconf(_SC_PAGESIZE));h4_startup();void_log("original startup returned eax=%08x",g_eax);return 0;
}
