/* GPL-3.0-or-later. Only the 32-byte fixture made by bootstrap_checks.py is read. */
#include "h4_host.h"
#include <assert.h>
void h4_test_files(void) {
 uint32_t path=void_gstrdup("c:\\heroesiv\\DATA\\fixture.DAT"),mem=void_galloc(512),root=void_gstrdup("C:\\HeroesIV"),data=void_gstrdup("Data"),newfile=void_gstrdup("new.dat"),wild=void_gstrdup("Data\\*.DAT"),escape=void_gstrdup("C:\\HeroesIV\\..\\outside.dat");
 assert(h4_test_api("KERNEL32.dll!SetCurrentDirectoryA",1,data));assert(h4_test_api("KERNEL32.dll!GetCurrentDirectoryA",2,256u,mem)==16&&!strcmp(GPTR(mem),"C:\\HeroesIV\\Data"));assert(h4_test_api("KERNEL32.dll!SetCurrentDirectoryA",1,root));assert(h4_test_api("KERNEL32.dll!GetCurrentDirectoryA",2,256u,mem)==11&&!strcmp(GPTR(mem),"C:\\HeroesIV"));
 MEM32(mem+100)=12;MEM32(mem+104)=0;MEM32(mem+108)=1;
 uint32_t f=h4_test_api("KERNEL32.dll!CreateFileA",7,path,0x80000000u,1u,mem+100,3u,0u,0u);assert(f!=0xffffffffu&&h4_test_api("KERNEL32.dll!GetFileType",1,f)==1);assert(h4_test_api("KERNEL32.dll!GetFileSize",2,f,0u)==32);assert(h4_test_api("KERNEL32.dll!ReadFile",5,f,mem,32u,mem+64,0u)&&MEM32(mem+64)==32);for(unsigned i=0;i<32;i++)assert(MEM8(mem+i)==i);
 assert(h4_test_api("KERNEL32.dll!SetFilePointer",4,f,5u,0u,0u)==5);assert(h4_test_api("KERNEL32.dll!ReadFile",5,f,mem,3u,mem+64,0u)&&MEM8(mem)==5&&MEM8(mem+2)==7);
 uint32_t mapping=h4_test_api("KERNEL32.dll!CreateFileMappingA",6,f,0u,2u,0u,0u,0u);assert(mapping);assert(h4_test_api("KERNEL32.dll!CloseHandle",1,f));uint32_t view=h4_test_api("KERNEL32.dll!MapViewOfFile",5,mapping,4u,0u,0u,0u);assert(view&&void_vm_readable(view,32)&&!void_vm_writable(view,32));for(unsigned i=0;i<32;i++)assert(MEM8(view+i)==i);assert(h4_test_api("KERNEL32.dll!CloseHandle",1,mapping));assert(h4_test_api("KERNEL32.dll!UnmapViewOfFile",1,view));assert(!h4_test_api("KERNEL32.dll!UnmapViewOfFile",1,view));
 assert(h4_test_api("KERNEL32.dll!CreateFileA",7,path,0x40000000u,1u,0u,3u,0u,0u)==0xffffffffu);assert(h4_test_api("KERNEL32.dll!CreateFileA",7,escape,0x80000000u,1u,0u,3u,0u,0u)==0xffffffffu);
 f=h4_test_api("KERNEL32.dll!CreateFileA",7,newfile,0xc0000000u,0u,0u,2u,0u,0u);assert(f!=0xffffffffu);MEM32(mem)=0x12345678;assert(h4_test_api("KERNEL32.dll!WriteFile",5,f,mem,4u,mem+64,0u));assert(h4_test_api("KERNEL32.dll!SetFilePointer",4,f,0u,0u,0u)==0);assert(h4_test_api("KERNEL32.dll!ReadFile",5,f,mem+4,4u,mem+64,0u)&&MEM32(mem+4)==0x12345678);assert(h4_test_api("KERNEL32.dll!CloseHandle",1,f));
 uint32_t search=h4_test_api("KERNEL32.dll!FindFirstFileA",2,wild,mem);assert(search!=0xffffffffu&&!strcmp(GPTR(mem+44),"Fixture.dat"));assert(!h4_test_api("KERNEL32.dll!FindNextFileA",2,search,mem));assert(h4_test_api("KERNEL32.dll!FindClose",1,search));
 void_gfree(path);void_gfree(mem);void_gfree(root);void_gfree(data);void_gfree(newfile);void_gfree(wild);void_gfree(escape);puts("PASS read-only resources, case-insensitive paths, private writes, search, file mapping and traversal denial");
}
