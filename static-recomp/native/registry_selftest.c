/* GPL-3.0-or-later. Synthetic values only; restart test reads the saved private registry. */
#include "h4_host.h"
#include <assert.h>
void h4_test_registry(int read_only) {
 uint32_t key=void_gstrdup("Software\\OpenH4Synthetic"),name=void_gstrdup("Number"),mem=void_galloc(32);MEM32(mem+12)=0x12345678;
 uint32_t h;if(!read_only){assert(!h4_test_api("ADVAPI32.dll!RegCreateKeyExA",9,0x80000001u,key,0u,0u,0u,0x2001fu,0u,mem,mem+4));h=MEM32(mem);assert(!h4_test_api("ADVAPI32.dll!RegSetValueExA",6,h,name,0u,4u,mem+12,4u));}
 else {assert(!h4_test_api("ADVAPI32.dll!RegOpenKeyExA",5,0x80000001u,key,0u,0x20019u,mem));h=MEM32(mem);assert(h4_test_api("ADVAPI32.dll!RegSetValueExA",6,h,name,0u,4u,mem+12,4u)==5);}
 MEM32(mem+8)=0;assert(!h4_test_api("ADVAPI32.dll!RegQueryValueExA",6,h,name,0u,mem+4,0u,mem+8));assert(MEM32(mem+8)==4&&MEM32(mem+4)==4);
 MEM32(mem+8)=2;MEM32(mem+16)=0xdeadbeef;assert(h4_test_api("ADVAPI32.dll!RegQueryValueExA",6,h,name,0u,0u,mem+16,mem+8)==234&&MEM32(mem+8)==4&&MEM32(mem+16)==0xdeadbeef);
 assert(!h4_test_api("ADVAPI32.dll!RegQueryValueExA",6,h,name,0u,mem+4,mem+16,mem+8));assert(MEM32(mem+16)==0x12345678);
 assert(!h4_test_api("ADVAPI32.dll!RegCloseKey",1,h));assert(h4_test_api("ADVAPI32.dll!RegCloseKey",1,h)==6);void_gfree(key);void_gfree(name);void_gfree(mem);puts(read_only?"PASS registry persistence after process restart and read-only rights":"PASS registry create, values, sizes, insufficient buffer and handles");
}
