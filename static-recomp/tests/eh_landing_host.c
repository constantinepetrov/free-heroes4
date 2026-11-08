/* GPL-3.0-or-later. Own PE32 fixture using the same EH dispatcher as bootstrap. */
#include "h4_host.h"
#include <assert.h>
#include <stdlib.h>
void h4_record_entry(uint32_t va){(void)va;}
int h4_eh_test_mode(void){return 0;}
void void_log(const char *fmt,...){va_list ap;va_start(ap,fmt);vfprintf(stderr,fmt,ap);va_end(ap);fputc('\n',stderr);}
_Noreturn void void_fatal(const char *fmt,...){va_list ap;va_start(ap,fmt);vfprintf(stderr,fmt,ap);va_end(ap);fputc('\n',stderr);exit(78);}
void void_lift_trap(uint32_t va,const char *reason){void_fatal("trap %08x %s",va,reason);}
void h4_need(uint32_t va,uint32_t n,int write){(void)write;if((uint64_t)va+n>0x01100000u)void_fatal("bad synthetic pointer");}
const char *h4_string(uint32_t va){h4_need(va,1,0);for(uint32_t n=0;n<1024;n++){h4_need(va+n,1,0);if(!MEM8(va+n))return GPTR(va);}void_fatal("unterminated synthetic string");}
static void synthetic_throw(void){assert(MEM32(g_esp+4)==0xe06d7363u&&MEM32(g_esp+8)==1&&MEM32(g_esp+12)==3);uint32_t args=MEM32(g_esp+16);assert(MEM32(args)==0x19930520u);uint32_t obj=MEM32(args+4),info=MEM32(args+8);g_esp+=4;h4_eh_throw(obj,info);}
static void corrupt_continuation(void){recomp_func_t f=h4_find_lifted(0x401350);assert(f);f();g_eax=0x401401;}
recomp_func_t void_resolve(uint32_t va){if(va==0x401350&&getenv("H4_EH_BAD_RESUME"))return corrupt_continuation;if(va==0xf0000000)return synthetic_throw;if(va==0xf0000004)return synthetic_throw;recomp_func_t f=h4_find_lifted(va);if(!f)void_fatal("unknown synthetic funclet %08x",va);return f;}
int main(int argc,char **argv){
 assert(argc==2);h4_guest_base=calloc(1,0x01100000);assert(h4_guest_base);g_mem_base=(ptrdiff_t)h4_guest_base;
 FILE *f=fopen(argv[1],"rb");assert(f);assert(fread(GPTR(0x400000),1,0x3000,f)==0x3000);assert(!fclose(f));
 g_fs_base=0x1003000;MEM32(g_fs_base)=0xffffffff;MEM32(0x40200c)=0xf0000000;MEM32(0x402010)=0xf0000004;MEM8(0x0100ffe4)=0x7f;MEM8(0x4020f0)=0;
 g_esp=0x1010000;MEM32(g_esp)=0;g_ebp=0x44556677;g_ebx=0x11223344;g_esi=0x22334455;g_edi=0x33445566;
 assert(h4_eh_resume_allowed(0x401000,0x401400));assert(!h4_eh_resume_allowed(0x401000,0x401401));h4_startup();assert(!eh_frame_count&&!eh_context_count&&g_eax==42&&g_esp==0x1010004&&g_ebp==0x44556677);
 assert(g_ebx==0x11223344&&g_esi==0x22334455&&g_edi==0x33445566&&MEM32(g_fs_base)==0xffffffff);assert(MEM8(0x0100ffe4)==0&&MEM8(0x4020f0)==12);
 free(h4_guest_base);puts("PASS real synthetic EH dispatcher, rethrow, cleanup and generated landing pad");return 0;
}
