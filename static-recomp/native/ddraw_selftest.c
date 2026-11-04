/* GPL-3.0-or-later. COM tests use only our synthetic surfaces and pixels. */
#include "h4_host.h"
#include <assert.h>
static uint32_t call(uint32_t obj,unsigned slot,unsigned argc,...) {
 uint32_t sp=g_esp;MEM32(sp)=RECOMP_RETADDR;MEM32(sp+4)=obj;va_list ap;va_start(ap,argc);for(unsigned i=0;i<argc;i++)MEM32(sp+8+4*i)=va_arg(ap,uint32_t);va_end(ap);
 void_resolve(MEM32(MEM32(obj)+4*slot))();assert(g_esp==sp+8+4*argc);g_esp=sp;return g_eax;
}
void h4_test_gdi(void);
void h4_test_ddraw(void) {
 uint32_t mem=void_galloc(1024),iid=mem,out=mem+16,desc=mem+32,fx=mem+256,key=mem+400;
 uint32_t id[]={0x15e65ec0,0x11d23b9c,0x60002fb9,0x5bea9797};memcpy(GPTR(iid),id,16);
 assert(!h4_directdraw_create(0,out,iid,0));uint32_t dd=MEM32(out);assert(dd&&MEM32(dd)&&MEM32(MEM32(dd)+21*4)==0xd1010054);
 assert(!call(dd,20,2,0x50000001u,8u));assert(!call(dd,21,5,320u,200u,16u,0u,0u));
 MEM32(desc)=124;MEM32(desc+4)=0x21;MEM32(desc+20)=1;MEM32(desc+104)=0x200|8|0x10;assert(!call(dd,6,3,desc,out,0u));uint32_t primary=MEM32(out);MEM32(key)=4;assert(!call(primary,12,2,key,out));uint32_t back=MEM32(out);
 MEM32(desc)=124;assert(!call(back,25,4,0u,desc,1u,0u));uint32_t pixels=MEM32(desc+36),pitch=MEM32(desc+16);assert(MEM32(desc+12)==320&&MEM32(desc+8)==200&&pitch==640&&pixels);
 for(unsigned y=0;y<200;y++)for(unsigned x=0;x<320;x++)MEM16(pixels+y*pitch+2*x)=(uint16_t)((x&31)|((y&63)<<5));
 assert(call(back,25,4,0u,desc,1u,0u)==0x887601aeu);assert(!call(back,32,1,0u));assert(call(back,32,1,0u)==0x88760248u);
 assert(!call(primary,11,2,0u,1u));MEM32(desc)=124;assert(!call(primary,25,4,0u,desc,0x11u,0u));pixels=MEM32(desc+36);assert(MEM16(pixels+10*640+7*2)==(7|(10<<5)));assert(!call(primary,32,1,0u));
 MEM32(fx)=100;MEM32(fx+80)=0x1234;assert(!call(back,5,5,0u,0u,0u,0x01000400u,fx));MEM32(key)=MEM32(key+4)=0x1234;assert(!call(back,29,2,8u,key));MEM32(key)=0;MEM32(key+4)=1;assert(call(back,29,2,8u,key)==0x80004001u);
 h4_test_gdi();assert(call(back,2,0)==1);assert(!call(primary,2,0));assert(!call(dd,2,0));void_gfree(mem);puts("PASS 32-bit DirectDraw COM, stdcall, attached surface, Lock, color fill, color key, Flip and SDL presentation");
}
