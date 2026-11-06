/* Own synthetic landing-pad test. This is not an exception dispatcher. */
#include "recomp_types.h"
#include <assert.h>
#include <stdlib.h>
static recomp_ehframe_t *frame;
void recomp_eh_push(recomp_ehframe_t *f){f->prev=frame;frame=f;}
void recomp_eh_pop(recomp_ehframe_t *f){assert(frame==f);frame=f->prev;}
void h4_record_entry(uint32_t va){(void)va;}
void void_lift_trap(uint32_t va,const char *reason){fprintf(stderr,"trap %08x %s\n",va,reason);abort();}
static void synthetic_throw(void){
 assert(frame&&MEM32(g_fs_base)==g_ebp-12);
 uint32_t saved_sp=g_esp;g_esp-=4;MEM32(g_esp)=0;
 recomp_func_t f=h4_find_lifted(0x401300);assert(f);f();assert(g_esp==saved_sp);
 assert(g_eax==0x401400);frame->resume=g_eax;g_esp=g_ebp;
 _longjmp(frame->jb,1);
}
recomp_func_t void_resolve(uint32_t va){assert(va==0xf0000000);return synthetic_throw;}
int main(void){
 h4_guest_base=calloc(1,0x01100000);assert(h4_guest_base);g_mem_base=(ptrdiff_t)h4_guest_base;
 g_fs_base=0x1003000;MEM32(g_fs_base)=0xffffffff;MEM32(0x40200c)=0xf0000000;
 g_esp=0x1010000;MEM32(g_esp)=0;g_ebp=0x44556677;g_ebx=0x11223344;g_esi=0x22334455;g_edi=0x33445566;
 h4_startup();assert(!frame&&g_eax==42&&g_esp==0x1010004&&g_ebp==0x44556677);
 assert(g_ebx==0x11223344&&g_esi==0x22334455&&g_edi==0x33445566&&MEM32(g_fs_base)==0xffffffff);
 free(h4_guest_base);puts("PASS synthetic catch call, landing pad, registers, guest stack and SEH restoration");return 0;
}
