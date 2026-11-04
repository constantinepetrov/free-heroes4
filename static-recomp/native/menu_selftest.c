/* GPL-3.0-or-later. Own RT_MENU fixture; no original captions or command tables. */
#include "h4_host.h"
#include <assert.h>
void h4_test_menu_setup(uint32_t wc) {
 assert(void_vm_claim(H4_IMAGE_BASE,H4_IMAGE_SIZE,4));MEM16(H4_IMAGE_BASE)=0x5a4d;MEM32(H4_IMAGE_BASE+0x3c)=0x80;MEM32(H4_IMAGE_BASE+0x80)=0x4550;MEM32(H4_IMAGE_BASE+0x80+136)=0x1000;MEM32(H4_IMAGE_BASE+0x80+140)=0x1000;
 uint32_t rs=H4_IMAGE_BASE+0x1000;MEM16(rs+14)=1;MEM32(rs+16)=4;MEM32(rs+20)=0x80000018;MEM16(rs+38)=1;MEM32(rs+40)=133;MEM32(rs+44)=0x80000030;MEM16(rs+62)=1;MEM32(rs+64)=0x409;MEM32(rs+68)=72;uint16_t words[]={0,0,0x80,1001,'T','e','s','t',0};MEM32(rs+72)=0x1800;MEM32(rs+76)=sizeof words;memcpy(GPTR(H4_IMAGE_BASE+0x1800),words,sizeof words);MEM32(wc+36)=133;
}
void h4_test_menu(uint32_t window) {
 uint32_t menu=h4_test_api("USER32.dll!GetMenu",1,window);assert(menu);assert(!h4_test_api("USER32.dll!CheckMenuItem",3,menu,1001u,8u));assert(h4_test_api("USER32.dll!CheckMenuItem",3,menu,1001u,0u)==8);assert(!h4_test_api("USER32.dll!EnableMenuItem",3,menu,1001u,1u));assert(h4_test_api("USER32.dll!EnableMenuItem",3,menu,1001u,0u)==1);assert(h4_test_api("USER32.dll!CheckMenuItem",3,menu,999u,0u)==0xffffffff);puts("PASS real RT_MENU parsing, class association and command state");
}
