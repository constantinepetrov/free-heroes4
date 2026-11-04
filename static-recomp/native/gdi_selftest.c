/* GPL-3.0-or-later. Synthetic DIB colors, orientations and object lifetime only. */
#include "h4_host.h"
#include <assert.h>
void h4_test_gdi(void) {
 uint32_t m=void_galloc(128);MEM32(m)=40;MEM32(m+4)=32;MEM32(m+8)=(uint32_t)-16;MEM16(m+12)=1;MEM16(m+14)=16;MEM32(m+16)=3;MEM32(m+40)=0xf800;MEM32(m+44)=0x7e0;MEM32(m+48)=0x1f;
 uint32_t dc=h4_test_api("GDI32.dll!CreateCompatibleDC",1,0u),bmp=h4_test_api("GDI32.dll!CreateDIBSection",6,0u,m,0u,m+64,0u,0u),pixels=MEM32(m+64);assert(dc&&bmp&&pixels);
 for(unsigned y=0;y<16;y++)for(unsigned x=0;x<32;x++)MEM16(pixels+64*y+2*x)=y<8?0xf800:0x7e0;
 uint32_t old=h4_test_api("GDI32.dll!SelectObject",2,dc,bmp);assert(old==0x52000000);assert(!h4_test_api("GDI32.dll!DeleteObject",1,bmp));
 assert(h4_test_api("GDI32.dll!BitBlt",9,0x51000000u,0u,0u,32u,16u,dc,0u,0u,0xcc0020u));uint16_t readback[32*16]={0};assert(h4_ddraw_primary_io(0,0,0,32,16,readback,32));for(unsigned y=0;y<16;y++)assert(readback[32*y]==(y<8?0xf800:0x7e0));
 uint32_t dc2=h4_test_api("GDI32.dll!CreateCompatibleDC",1,0u);MEM32(m+8)=16;MEM16(m+14)=32;MEM32(m+16)=0;uint32_t bmp2=h4_test_api("GDI32.dll!CreateDIBSection",6,0u,m,0u,m+64,0u,0u),pixels2=MEM32(m+64);assert(bmp2);assert(h4_test_api("GDI32.dll!SelectObject",2,dc2,bmp2)==old);
 assert(h4_test_api("GDI32.dll!BitBlt",9,dc2,0u,0u,32u,16u,0x51000000u,0u,0u,0xcc0020u));assert(MEM32(pixels2)==0xff00&&MEM32(pixels2+15*128)==0xff0000);
 assert(h4_test_api("GDI32.dll!SelectObject",2,dc,old)==bmp);assert(h4_test_api("GDI32.dll!SelectObject",2,dc2,old)==bmp2);assert(h4_test_api("GDI32.dll!DeleteObject",1,bmp));assert(h4_test_api("GDI32.dll!DeleteObject",1,bmp2));assert(h4_test_api("GDI32.dll!DeleteDC",1,dc)&&h4_test_api("GDI32.dll!DeleteDC",1,dc2));void_gfree(m);
 puts("PASS GDI DIB565/DIB32, top-down/bottom-up, screen BitBlt and object lifetime");
}
