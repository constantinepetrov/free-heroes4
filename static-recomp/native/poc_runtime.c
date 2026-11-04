/* GPL-3.0. Test host; no original game instructions or generated game C. */
#include "recomp_types.h"
#include <sys/mman.h>
#include <errno.h>
#include <unistd.h>
uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_flag_k,g_flag_a,g_flag_b,g_flag_cf;
uint32_t g_cur_func,g_icall_trace[ICALL_TRACE_SIZE],g_icall_from[ICALL_TRACE_SIZE],g_icall_trace_idx,g_icall_count,g_backedges;
uint32_t g_enter_trace[RECOMP_ENTER_SIZE],g_enter_idx;
uint32_t g_fs_base,g_gs_base,g_es_base;
uint64_t g_mm[8];
uint64_t g_fp_round_mask,g_fp_store_mask;
double g_st[8];int64_t g_st_i64[8];int g_fp_top;uint16_t g_fpu_cw=0x37f;int void_fpu_cmp;
unsigned char *h4_guest_base;
ptrdiff_t g_mem_base;
void (*recomp_yield_hook)(void);
static uint32_t last_error=0x12345678;
static void get_error(void){g_eax=last_error;g_esp+=4;}
static void get_tls(void){if(MEM32(g_esp+4)!=0){fputs("unexpected TLS index\n",stderr);exit(3);}g_eax=0x01001000;g_esp+=8;}
static void set_error(void){last_error=MEM32(g_esp+4);g_esp+=8;}
#ifdef H4_HAS_STARTUP
static void get_version(void){fputs("original startup reached GetVersion\n",stderr);g_eax=0x0a280105;g_esp+=4;}
#endif
recomp_func_t void_resolve(uint32_t va){
#ifdef H4_HAS_STARTUP
 if(va==0xf000000c)return get_version;
#endif
switch(va){case 0xf0000000:return get_error;case 0xf0000004:return get_tls;case 0xf0000008:return set_error;default:fprintf(stderr,"unsupported dispatch %08x from %08x\n",va,g_cur_func);exit(77);}}
void void_lift_trap(uint32_t va,const char* reason){fprintf(stderr,"lift trap %08x: %s\n",va,reason);exit(78);}

int main(int argc,char** argv){
 if(argc<2||argc>3){fputs("mode required: rng, crt_rand, shuffle\n",stderr);return 2;}
 void* p=mmap(NULL,VOID_GUEST_SPAN,PROT_NONE,MAP_PRIVATE|MAP_ANON,-1,0);
 h4_guest_base=p;g_mem_base=(ptrdiff_t)p;
 if(p==MAP_FAILED){perror("reserve guest");return 2;}
 if(mprotect((void*)(VOID_GUEST_BASE+0x00400000),0x00e00000,PROT_READ|PROT_WRITE)){perror("commit guest");return 2;}
#ifdef H4_HAS_STARTUP
 if(!strcmp(argv[1],"startup")){
  if(argc!=3)return 3;
  FILE* image=fopen(argv[2],"rb");if(!image){perror("image");return 2;}
  size_t loaded=fread((void*)(VOID_GUEST_BASE+0x400000),1,0xe00000,image);
  if(ferror(image)||!feof(image)||loaded<0x200){fclose(image);return 2;}fclose(image);
  MEM32(H4_GET_VERSION)=0xf000000c;g_fs_base=0x01003000;MEM32(g_fs_base)=0xffffffff;
  g_esp=0x01010000;MEM32(g_esp)=0;h4_startup();fputs("startup returned\n",stderr);return 0;
 }
#endif
 MEM32(H4_GET_ERROR)=0xf0000000;MEM32(H4_GET_TLS)=0xf0000004;MEM32(H4_SET_ERROR)=0xf0000008;MEM32(H4_TLS_INDEX)=0;
 uint32_t seed,count;
 while(scanf("%u %u",&seed,&count)==2){
  if(count<1||count>64)return 3;
  g_eax=g_ecx=g_edx=0;g_ebx=0x11223344;g_esi=0x22334455;g_edi=0x33445566;g_ebp=0x44556677;
  g_flag_k=g_flag_a=g_flag_b=g_flag_cf=0;g_esp=0x01010000;MEM32(g_esp)=0;
  MEM32(0x01001000)=seed;MEM32(0x01001014)=seed;last_error=0x12345678;
  if(!strcmp(argv[1],"rng")){g_ecx=0x01001000;h4_rng();}
  else if(!strcmp(argv[1],"crt_rand"))h4_crt_rand();
  else if(!strcmp(argv[1],"shuffle")){
   MEM32(g_esp+4)=0x01002000;MEM32(g_esp+8)=0x01002000+4*count;MEM32(g_esp+12)=0x01001000;
   for(uint32_t i=0;i<count;i++)MEM32(0x01002000+4*i)=0x101+i*17;
   h4_shuffle();
  }
#ifdef H4_HAS_X87
  else if(!strcmp(argv[1],"x87")){
   MEM32(g_esp+4)=seed;MEM32(g_esp+8)=count;MEM32(g_esp+12)=0;h4_x87();
  }
#endif
  else return 3;
  if(g_esp!=0x01010004||g_ebx!=0x11223344||g_esi!=0x22334455||g_edi!=0x33445566||g_ebp!=0x44556677||last_error!=0x12345678){fputs("ABI preservation failed\n",stderr);return 4;}
  printf("%u %u",g_eax,MEM32(!strcmp(argv[1],"crt_rand")?0x01001014:0x01001000));
  if(!strcmp(argv[1],"shuffle"))for(uint32_t i=0;i<count;i++)printf(" %u",MEM32(0x01002000+4*i));
  putchar('\n');
 }
 munmap(p,VOID_GUEST_SPAN);return 0;
}
