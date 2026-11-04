/* GPL-3.0-or-later. Original instructions enter only through private generated C. */
#include "h4_host.h"
#include <assert.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <locale.h>
#include <wctype.h>

static uint32_t last_error, tls_value[1088];
static unsigned char tls_used[1088];
static const char *active_import;
static uint32_t active_iat;
static uint64_t api_calls;

void void_log(const char *fmt, ...) {va_list ap;va_start(ap,fmt);vfprintf(stderr,fmt,ap);va_end(ap);fputc('\n',stderr);}
_Noreturn void void_fatal(const char *fmt, ...) {va_list ap;va_start(ap,fmt);vfprintf(stderr,fmt,ap);va_end(ap);fputc('\n',stderr);exit(78);}
void void_set_last_error(uint32_t error) {last_error=error;if(g_fs_base)MEM32(g_fs_base+0x34)=error;}
void void_lift_trap(uint32_t va,const char *reason) {void_fatal("lift trap %08x: %s",va,reason);}
static void need(uint32_t va,uint32_t n,int write) {
 if (!(write?void_vm_writable(va,n):void_vm_readable(va,n)))void_fatal("bad API %s pointer %08x+%x from %08x",active_import,va,n,g_cur_func);
}
static uint32_t arg(unsigned n) {need(g_esp+4*n,4,0);return MEM32(g_esp+4*n);}
static const char *string(uint32_t va) {uint32_t n=0;while(n<32768){need(va+n,1,0);if(!MEM8(va+n))return GPTR(va);++n;}void_fatal("unterminated string %08x",va);}
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
 uint32_t cp=arg(1),options=arg(2),src=arg(3),dst=arg(5),cap=arg(6);int32_t len=(int32_t)arg(4);
 if(cp==0)cp=1252;
 if((cp!=1252&&cp!=1251)||!src||len==0||len< -1||(int32_t)cap<0||options&~(wide?0x400u:9u)){void_set_last_error(87);return 0;}
 uint32_t n=0;
 if(len==-1){do{need(src+n*(wide?2:1),wide?2:1,0);++n;if(n>1048576)void_fatal("conversion input too long");}while(wide?MEM16(src+2*(n-1)):MEM8(src+n-1));}else n=(uint32_t)len;
 if(n>0x10000000){void_set_last_error(87);return 0;}need(src,n*(wide?2:1),0);
 if(cap&&cap<n){void_set_last_error(122);return 0;}if(cap)need(dst,n*(wide?1:2),1);
 int used=0;uint8_t def='?';if(wide&&arg(7)){need(arg(7),1,0);def=MEM8(arg(7));}
 for(uint32_t i=0;i<n;i++){
  if(wide){uint8_t b;uint16_t u=MEM16(src+2*i);int ok=cp==1251?void_u16_to_ansi(u,&b):void_u16_to_cp1252(u,&b);if(!ok){b=def;used=1;}if(cap)MEM8(dst+i)=b;}
  else if(cap)MEM16(dst+2*i)=cp==1251?void_ansi_to_u16(MEM8(src+i)):void_cp1252_to_u16(MEM8(src+i));
 }
 if(wide&&arg(8)){need(arg(8),4,1);MEM32(arg(8))=used;}return n;
}
/* shortcut: one guest thread; unsatisfied blocking waits stop until a scheduler exists. */
typedef struct {unsigned kind,refs,state,manual,owner,recursion;char *name;} object;
static object objects[256];
static object *object_get(uint32_t handle) {unsigned i=handle-0x30000000u;if(i>=256||!objects[i].refs)return NULL;return &objects[i];}
static uint32_t object_create(unsigned kind,uint32_t name,uint32_t manual,uint32_t state) {
 const char *n=name?string(name):NULL;
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
static void invoke(void) {
 const char *full=active_import,*s=strchr(full,'!');s=s?s+1:full;
 uint32_t a,b,c,d,value=0;unsigned count=0;
 ++api_calls;
 if(getenv("H4_TRACE_API"))void_log("API %s from %08x esp=%08x",full,g_cur_func,g_esp);
#define API(name,n) if(!strcmp(s,name)) {count=n;
#define NEXT } else
 API("GetVersion",0) value=0x0a280105;
 NEXT API("GetSystemInfo",1) a=arg(1);need(a,36,1);memset(GPTR(a),0,36);MEM32(a+4)=4096;MEM32(a+8)=0x10000;MEM32(a+12)=GUEST_USER_TOP-1;MEM32(a+16)=1;MEM32(a+20)=1;MEM32(a+24)=586;MEM32(a+28)=65536;MEM16(a+32)=6;
 NEXT API("GetVersionExA",1) a=arg(1);need(a,4,0);b=MEM32(a);if(b!=148&&b!=156){void_set_last_error(87);}else{need(a,b,1);memset(GPTR(a+4),0,b-4);MEM32(a+4)=5;MEM32(a+8)=1;MEM32(a+12)=2600;MEM32(a+16)=2;if(b==156){MEM16(a+148)=3;MEM8(a+154)=1;}value=1;}
 NEXT API("GetLastError",0) value=last_error;
 NEXT API("SetLastError",1) void_set_last_error(arg(1));
 NEXT API("HeapCreate",3) flags(arg(1),1);value=void_heap_create(arg(2),arg(3));
 NEXT API("HeapDestroy",1) value=void_heap_destroy(arg(1));
 NEXT API("GetProcessHeap",0) value=void_process_heap();
 NEXT API("HeapAlloc",3) flags(arg(2),9);value=void_heap_alloc(arg(1),arg(3),arg(2)&8);
 NEXT API("HeapFree",3) flags(arg(2),1);value=void_heap_free(arg(1),arg(3));
 NEXT API("HeapSize",3) flags(arg(2),1);value=void_heap_size(arg(1),arg(3));
 NEXT API("HeapReAlloc",4) flags(arg(2),25);value=void_heap_realloc(arg(1),arg(3),arg(4),arg(2)&8,arg(2)&16);
 NEXT API("VirtualAlloc",4) value=void_vm_alloc(arg(1),arg(2),arg(3),arg(4));
 NEXT API("VirtualFree",3) value=void_vm_free(arg(1),arg(2),arg(3));
 NEXT API("VirtualProtect",4) need(arg(4),4,1);value=void_vm_protect(arg(1),arg(2),arg(3),(uint32_t*)GPTR(arg(4)));
 NEXT API("VirtualQuery",3) if(arg(3)<28){void_set_last_error(24);}else{need(arg(2),28,1);value=void_vm_query(arg(1),arg(2));}
 NEXT API("CreateEventA",4) if(arg(1)){void_set_last_error(50);}else value=object_create(1,arg(4),!!arg(2),!!arg(3));
 NEXT API("CreateMutexA",3) if(arg(1)){void_set_last_error(50);}else value=object_create(2,arg(3),0,!!arg(2));
 NEXT API("WaitForSingleObject",2) value=object_wait(arg(1),arg(2));
 NEXT API("SetEvent",1) object *o=object_get(arg(1));if(o&&o->kind==1){o->state=1;value=1;}else void_set_last_error(6);
 NEXT API("ResetEvent",1) object *o=object_get(arg(1));if(o&&o->kind==1){o->state=0;value=1;}else void_set_last_error(6);
 NEXT API("ReleaseMutex",1) object *o=object_get(arg(1));if(o&&o->kind==2&&o->owner==1&&o->recursion){if(!--o->recursion)o->owner=0;value=1;}else void_set_last_error(288);
 NEXT API("CloseHandle",1) object *o=object_get(arg(1));if(o){if(!--o->refs){free(o->name);memset(o,0,sizeof *o);}value=1;}else void_set_last_error(6);
 NEXT API("InitializeCriticalSection",1) a=arg(1);need(a,24,1);memset(GPTR(a),0,24);MEM32(a)=MEM32(a+4)=0xffffffff;
 NEXT API("EnterCriticalSection",1) a=arg(1);need(a,24,1);if(MEM32(a+12)&&MEM32(a+12)!=1)void_fatal("critical section owner unsupported");MEM32(a+12)=1;MEM32(a+8)++;MEM32(a+4)++;
 NEXT API("LeaveCriticalSection",1) a=arg(1);need(a,24,1);if(MEM32(a+12)!=1||!MEM32(a+8))void_fatal("critical section not owned");if(!--MEM32(a+8))MEM32(a+12)=0;MEM32(a+4)--;
 NEXT API("DeleteCriticalSection",1) a=arg(1);need(a,24,1);if(MEM32(a+8))void_fatal("delete owned critical section");memset(GPTR(a),0,24);
 NEXT API("TlsAlloc",0) value=tls_alloc();
 NEXT API("TlsGetValue",1) value=tls_get(arg(1));
 NEXT API("TlsSetValue",2) value=tls_set(arg(1),arg(2));
 NEXT API("TlsFree",1) a=arg(1);if(a<1088&&tls_used[a]){tls_used[a]=0;tls_value[a]=0;value=1;}else void_set_last_error(87);
 NEXT API("GetCurrentThreadId",0) value=1;
 NEXT API("GetCurrentProcess",0) value=0xffffffff;
 NEXT API("LCMapStringW",6) a=arg(2);if(a!=0x100&&a!=0x200)void_fatal("unsupported LCMapStringW flags %08x",a);b=arg(3);int32_t len=(int32_t)arg(4);c=0;if(len==-1){do{need(b+2*c,2,0);++c;}while(MEM16(b+2*(c-1))&&c<1048576);}else if(len>0)c=(uint32_t)len;if(!c||c>1048576){void_set_last_error(87);}else{need(b,2*c,0);if(arg(6)==0)value=c;else if(arg(6)<c)void_set_last_error(122);else{need(arg(5),2*c,1);for(d=0;d<c;d++){uint16_t u=MEM16(b+2*d);MEM16(arg(5)+2*d)=(uint16_t)(a==0x100?towlower(u):towupper(u));}value=c;}}
 NEXT API("GetStringTypeW",4) if(arg(1)!=1){void_set_last_error(50);}else{a=arg(2);int32_t len=(int32_t)arg(3);b=0;if(len==-1){do{need(a+2*b,2,0);++b;}while(MEM16(a+2*(b-1))&&b<1048576);}else if(len>0)b=(uint32_t)len;if(!b||b>1048576){void_set_last_error(87);}else{need(a,b*2,0);need(arg(4),b*2,1);for(c=0;c<b;c++)MEM16(arg(4)+2*c)=ctype1(MEM16(a+2*c));value=1;}}
 NEXT API("WideCharToMultiByte",8) value=convert(1);
 NEXT API("MultiByteToWideChar",6) value=convert(0);
 NEXT API("IsValidCodePage",1) a=arg(1);value=a==0||a==1251||a==1252;
 NEXT API("GetCPInfo",2) a=arg(1);if(a==0||a==1251||a==1252){b=arg(2);need(b,20,1);memset(GPTR(b),0,20);MEM32(b)=1;MEM8(b+4)='?';value=1;}else void_set_last_error(87);
 NEXT API("GetACP",0) value=1252;
 NEXT API("GetOEMCP",0) value=437;
 NEXT API("SetHandleCount",1) value=arg(1);
 NEXT API("GetCommandLineA",0) static uint32_t cmd;if(!cmd)cmd=void_gstrdup("\"C:\\HeroesIV\\heroes4.exe\"");value=cmd;
 NEXT API("GetModuleHandleA",1) if(!arg(1)||!strcasecmp(string(arg(1)),"heroes4.exe"))value=H4_IMAGE_BASE;else void_set_last_error(126);
 NEXT API("GetModuleFileNameA",3) const char *p="C:\\HeroesIV\\heroes4.exe";a=arg(3);if(arg(1)&&arg(1)!=H4_IMAGE_BASE)void_set_last_error(126);else if(a){need(arg(2),a,1);b=(uint32_t)strlen(p);if(b>=a){b=a-1;void_set_last_error(122);}memcpy(GPTR(arg(2)),p,b);MEM8(arg(2)+b)=0;value=b;}
 NEXT API("GetStartupInfoA",1) a=arg(1);need(a,68,1);memset(GPTR(a),0,68);MEM32(a)=68;MEM32(a+48)=1;
 NEXT API("GetEnvironmentStringsW",0) static uint32_t env;if(!env)env=void_galloc(4);value=env;
 NEXT API("GetEnvironmentStrings",0) static uint32_t env;if(!env)env=void_galloc(2);value=env;
 NEXT API("FreeEnvironmentStringsW",1) value=1; /* Environment block retained for this process lifetime. */
 NEXT API("FreeEnvironmentStringsA",1) value=1;
 NEXT API("GetStdHandle",1) a=arg(1);value=(a>=0xfffffff4u&&a<=0xfffffff6u)?0x100+(0xfffffff6u-a):0xffffffff;
 NEXT API("GetFileType",1) a=arg(1);if(a>=0x100&&a<=0x102)value=2;else void_set_last_error(6);
 NEXT API("IsBadReadPtr",2) value=!void_vm_readable(arg(1),arg(2));
 NEXT API("IsBadWritePtr",2) value=!void_vm_writable(arg(1),arg(2));
 NEXT API("IsBadCodePtr",1) value=h4_find_lifted(arg(1))==NULL;
 NEXT API("OutputDebugStringA",1) void_log("guest: %s",string(arg(1)));
 NEXT API("ExitProcess",1) void_log("original ExitProcess(%u), caller %08x, API calls %llu",arg(1),g_cur_func,(unsigned long long)api_calls);exit(arg(1)?79:0);
 NEXT {void_log("unimplemented Win32 import %s from %08x (IAT %08x, calls %llu)",full,g_cur_func,active_iat,(unsigned long long)api_calls);exit(77);}
#undef API
#undef NEXT
 (void)b;(void)c;(void)d;finish(value,count);
}
recomp_func_t void_resolve(uint32_t va) {
 recomp_func_t fn=h4_find_lifted(va);if(fn)return fn;
 if(va>=0xe0000000u && !(va&3) && (va-0xe0000000u)/4<H4_IMPORT_COUNT){unsigned i=(va-0xe0000000u)/4;active_import=h4_import_names[i].name;active_iat=h4_import_names[i].iat;return invoke;}
 void_log("unsupported dispatch %08x from %08x",va,g_cur_func);exit(77);
}
static void fault(int sig,siginfo_t *info,void *context) {
 (void)context; /* Diagnostic only: never resume a guest memory fault. */
 char buf[180];int n=snprintf(buf,sizeof buf,"guest fault signal=%d host=%p guest=%08x function=%08x esp=%08x eax=%08x ecx=%08x edx=%08x\n",sig,info->si_addr,(uint32_t)((uintptr_t)info->si_addr-(uintptr_t)h4_guest_base),g_cur_func,g_esp,g_eax,g_ecx,g_edx);write(2,buf,(size_t)n);_exit(80);
}
static void init(void) {
 if(!setlocale(LC_CTYPE,"C.UTF-8")&&!setlocale(LC_CTYPE,"en_US.UTF-8"))void_fatal("UTF-8 host locale unavailable");
 if(!void_guest_reserve())exit(2);
 struct sigaction sa={0};sa.sa_sigaction=fault;sa.sa_flags=SA_SIGINFO;sigemptyset(&sa.sa_mask);sigaction(SIGSEGV,&sa,0);sigaction(SIGBUS,&sa,0);
 if(!void_vm_claim(0x08000000,0x800000,4)||!void_vm_claim(0x7ffd0000,0x10000,4))void_fatal("cannot allocate startup stack/TEB");
 g_fs_base=0x7ffde000;MEM32(g_fs_base)=0xffffffff;MEM32(g_fs_base+4)=0x08800000;MEM32(g_fs_base+8)=0x08000000;MEM32(g_fs_base+0x18)=g_fs_base;MEM32(g_fs_base+0x24)=1;
 g_esp=0x087fffc0;MEM32(g_esp)=0;
}
static uint32_t test_api(const char *name,unsigned count,...) {
 uint32_t sp=g_esp;va_list ap;va_start(ap,count);for(unsigned i=1;i<=count;i++)MEM32(sp+4*i)=va_arg(ap,uint32_t);va_end(ap);
 active_import=name;invoke();assert(g_esp==sp+4*(count+1));g_esp=sp;return g_eax;
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
 uint32_t ev=object_create(1,0,0,0);assert(ev&&object_wait(ev,0)==258);assert(test_api("KERNEL32.dll!SetEvent",1,ev)==1);assert(!object_wait(ev,0)&&object_wait(ev,0)==258);assert(test_api("KERNEL32.dll!CloseHandle",1,ev));assert(object_wait(ev,0)==0xffffffff&&last_error==6);
 ev=object_create(1,0,1,1);assert(!object_wait(ev,0)&&!object_wait(ev,0));assert(test_api("KERNEL32.dll!ResetEvent",1,ev));assert(object_wait(ev,0)==258);assert(test_api("KERNEL32.dll!CloseHandle",1,ev));
 uint32_t mu=object_create(2,0,0,0);assert(!object_wait(mu,0)&&!object_wait(mu,0));assert(test_api("KERNEL32.dll!ReleaseMutex",1,mu));assert(test_api("KERNEL32.dll!ReleaseMutex",1,mu));assert(!test_api("KERNEL32.dll!ReleaseMutex",1,mu)&&last_error==288);assert(test_api("KERNEL32.dll!CloseHandle",1,mu));
 uint32_t cs=void_galloc(24);test_api("KERNEL32.dll!InitializeCriticalSection",1,cs);test_api("KERNEL32.dll!EnterCriticalSection",1,cs);test_api("KERNEL32.dll!EnterCriticalSection",1,cs);assert(MEM32(cs+8)==2&&MEM32(cs+12)==1);test_api("KERNEL32.dll!LeaveCriticalSection",1,cs);test_api("KERNEL32.dll!LeaveCriticalSection",1,cs);assert(!MEM32(cs+12)&&MEM32(cs+4)==0xffffffff);test_api("KERNEL32.dll!DeleteCriticalSection",1,cs);void_gfree(cs);
 assert(!void_vm_alloc(0xfffff000,0x10000,0x3000,4));assert(!void_vm_free(0xfffff000,0x10000,0x4000));assert(!void_vm_protect(0xfffff000,0x10000,4,&old));
 uint32_t text=void_galloc(16),out=void_galloc(32);MEM8(text)=0xdf;MEM8(text+1)=0;assert(test_api("KERNEL32.dll!MultiByteToWideChar",6,1251u,0u,text,0xffffffffu,out,16u)==2&&MEM16(out)==0x42f&&!MEM16(out+2));assert(test_api("KERNEL32.dll!WideCharToMultiByte",8,1251u,0u,out,0xffffffffu,text,16u,0u,0u)==2&&MEM8(text)==0xdf);assert(!test_api("KERNEL32.dll!MultiByteToWideChar",6,1251u,0u,text,0xffffffffu,out,1u)&&last_error==122);MEM16(text)='A';MEM16(text+2)=' ';MEM16(text+4)=0;assert(test_api("KERNEL32.dll!GetStringTypeW",4,1u,text,0xffffffffu,out));assert(MEM16(out)==0x381&&MEM16(out+2)==0x248&&MEM16(out+4)==0x20);assert(test_api("KERNEL32.dll!LCMapStringW",6,0x409u,0x100u,text,0xffffffffu,out,16u)==3&&MEM16(out)=='a');void_gfree(text);void_gfree(out);
 puts("PASS guest heap, VM, TLS, event, mutex, critical section, CP1251/1252, CTYPE1 and stdcall tests");
}
int main(int argc,char **argv) {
 if(argc!=2&&argc!=3){fputs("usage: bootstrap --selftest | image.bin\n",stderr);return 2;}init();if(!strcmp(argv[1],"--selftest")){selftest();return 0;}
 if(!strcmp(argv[1],"--unknown-api")){active_import="KERNEL32.dll!UnsupportedSyntheticAPI";invoke();return 3;}
 if(!strcmp(argv[1],"--blocking-wait")){object_wait(object_create(1,0,0,0),0xffffffff);return 3;}
 if(!void_vm_claim(H4_IMAGE_BASE,H4_IMAGE_SIZE,4))void_fatal("image allocation failed");
 FILE *f=fopen(argv[1],"rb");if(!f){perror("image");return 2;}size_t n=fread(GPTR(H4_IMAGE_BASE),1,H4_IMAGE_SIZE,f);int extra=fgetc(f);if(ferror(f)||extra!=EOF||n<512){fclose(f);void_fatal("invalid mapped PE image length");}fclose(f);
 for(unsigned i=0;i<H4_IMPORT_COUNT;i++)MEM32(h4_import_names[i].iat)=0xe0000000u+4*i;
 void_log("original startup ARM64, image=%08x size=%x, host page=%ld",H4_IMAGE_BASE,H4_IMAGE_SIZE,sysconf(_SC_PAGESIZE));h4_startup();void_log("original startup returned eax=%08x",g_eax);return 0;
}
