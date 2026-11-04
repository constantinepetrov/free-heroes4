/* GPL-3.0-or-later. Entire PE/resource fixture below is authored for this test. */
#include "h4_host.h"
#include <assert.h>
static uint32_t node_start(uint32_t p,const char *key,unsigned type) {
 MEM16(p)=MEM16(p+2)=0;MEM16(p+4)=type;uint32_t q=p+6;
 do {MEM16(q)=(unsigned char)*key;q+=2;}while(*key++);
 return (q+3)&~3u;
}
void h4_test_version(void) {
 assert(void_vm_claim(H4_IMAGE_BASE,H4_IMAGE_SIZE,4));
 MEM16(H4_IMAGE_BASE)=0x5a4d;MEM32(H4_IMAGE_BASE+0x3c)=0x80;MEM32(H4_IMAGE_BASE+0x80)=0x4550;
 MEM32(H4_IMAGE_BASE+0x80+136)=0x1000;MEM32(H4_IMAGE_BASE+0x80+140)=0x1000;
 uint32_t rs=H4_IMAGE_BASE+0x1000;MEM16(rs+14)=1;MEM32(rs+16)=16;MEM32(rs+20)=0x80000018;
 MEM16(rs+24+14)=1;MEM32(rs+40)=1;MEM32(rs+44)=0x80000030;
 MEM16(rs+48+14)=1;MEM32(rs+64)=0x409;MEM32(rs+68)=72;
 uint32_t root=H4_IMAGE_BASE+0x1800,v=node_start(root,"VS_VERSION_INFO",0);MEM16(root+2)=52;MEM32(v)=0xfeef04bd;MEM32(v+8)=0x00010002;MEM32(v+12)=0x00030004;v+=52;
 uint32_t sf=v;v=node_start(sf,"StringFileInfo",1);uint32_t lang=v;v=node_start(lang,"040904e4",1);uint32_t text=v;v=node_start(text,"FileVersion",1);
 const char *value="1.2-test";for(unsigned i=0;i<9;i++)MEM16(v+2*i)=(unsigned char)value[i];MEM16(text+2)=9;v=(v+18+3)&~3u;MEM16(text)=v-text;MEM16(lang)=v-lang;MEM16(sf)=v-sf;MEM16(root)=v-root;
 MEM32(rs+72)=0x1800;MEM32(rs+76)=v-root;
 uint32_t size=h4_version_size("C:\\HeroesIV\\heroes4.exe");assert(size>v-root);uint32_t data=void_galloc(size),out=void_galloc(8);
 assert(!h4_version_copy("heroes4.exe",size-1,data));assert(h4_version_copy("heroes4.exe",size,data));
 assert(h4_version_query(data,"\\",out,out+4));assert(MEM32(out+4)==52&&MEM32(MEM32(out))==0xfeef04bd&&MEM32(MEM32(out)+8)==0x00010002);
 assert(h4_version_query(data,"\\StringFileInfo\\040904E4\\FileVersion",out,out+4));assert(MEM32(out+4)==9&&!strcmp(GPTR(MEM32(out)),"1.2-test"));
 assert(!h4_version_query(data,"\\Missing",out,out+4));assert(!h4_version_size("unknown.dll"));MEM16(data)=0;assert(!h4_version_query(data,"\\",out,out+4));
 void_gfree(data);void_gfree(out);assert(void_vm_free(H4_IMAGE_BASE,0,0x8000));puts("PASS actual PE version tree, fixed info, ANSI strings and malformed data");
}
