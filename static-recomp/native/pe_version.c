/* GPL-3.0-or-later. Read actual RT_VERSION bytes from the reviewed, mapped PE. */
#include "h4_host.h"
#include <strings.h>
static void check(uint32_t p,uint32_t n,int write) {
 if(!(write?void_vm_writable(p,n):void_vm_readable(p,n)))void_fatal("invalid version buffer %08x+%x",p,n);
}
static int main_exe(const char *path) {
 const char *name=path;for(const char *p=path;*p;p++)if(*p=='/'||*p=='\\')name=p+1;
 return !strcasecmp(name,"heroes4.exe");
}
static uint32_t entry(uint32_t root,uint32_t span,uint32_t dir,int id) {
 if(dir>span||span-dir<16)return 0;
 unsigned count=MEM16(root+dir+12)+MEM16(root+dir+14);
 if(count>(span-dir-16)/8)return 0;
 for(unsigned i=0;i<count;i++){uint32_t p=root+dir+16+8*i;if(id<0||MEM32(p)==(unsigned)id)return MEM32(p+4);}
 return 0;
}
uint32_t h4_pe_resource(unsigned type,int id,uint32_t *size) {
 if(!void_vm_readable(H4_IMAGE_BASE,256))return 0;
 uint32_t pe=MEM32(H4_IMAGE_BASE+0x3c);if(pe>H4_IMAGE_SIZE||H4_IMAGE_SIZE-pe<144)return 0;
 if(MEM16(H4_IMAGE_BASE)!=0x5a4d||MEM32(H4_IMAGE_BASE+pe)!=0x4550)return 0;
 uint32_t root=MEM32(H4_IMAGE_BASE+pe+136),span=MEM32(H4_IMAGE_BASE+pe+140);
 if(!root||root>H4_IMAGE_SIZE||span>H4_IMAGE_SIZE-root||!void_vm_readable(H4_IMAGE_BASE+root,span))return 0;
 root+=H4_IMAGE_BASE;uint32_t d=entry(root,span,0,(int)type);if(!(d&0x80000000))return 0;
 d=entry(root,span,d&0x7fffffff,id);if(!(d&0x80000000))return 0;
 d=entry(root,span,d&0x7fffffff,-1);if(d&0x80000000||d>span||span-d<16)return 0;
 uint32_t va=MEM32(root+d),n=MEM32(root+d+4);if(!n||n>1048576||va>H4_IMAGE_SIZE||n>H4_IMAGE_SIZE-va)return 0;
 *size=n;return H4_IMAGE_BASE+va;
}
uint32_t h4_version_size(const char *path) {
 uint32_t n;if(!main_exe(path)||!h4_pe_resource(16,-1,&n)){void_set_last_error(1812);return 0;}return ((n+3)&~3u)*2;
}
int h4_version_copy(const char *path,uint32_t cap,uint32_t out) {
 uint32_t n;if(!main_exe(path)){void_set_last_error(2);return 0;}uint32_t p=h4_pe_resource(16,-1,&n);if(!p){void_set_last_error(1812);return 0;}
 if(cap<((n+3)&~3u)*2){void_set_last_error(122);return 0;}check(out,cap,1);memset(GPTR(out),0,cap);memcpy(GPTR(out),GPTR(p),n);return 1;
}
typedef struct {uint32_t key,value,children,end,count,type;} node;
static int parse(uint32_t p,uint32_t end,node *out) {
 if(p>end||end-p<6)return 0;check(p,6,0);uint32_t length=MEM16(p);if(length<6||length>end-p)return 0;
 check(p,length,0);uint32_t key=p+6,v=key;while(v+2<=p+length&&MEM16(v))v+=2;if(v+2>p+length)return 0;
 v=(v+2+3)&~3u;uint32_t n=MEM16(p+2),type=MEM16(p+4);if(type>1||v>p+length||n>(p+length-v)/(type?2:1))return 0;
 *out=(node){key,v,(v+n*(type?2:1)+3)&~3u,p+length,n,type};return 1;
}
static int key_equal(uint32_t p,uint32_t end,const char *s,size_t len) {
 if(end<=p||len>=(end-p)/2)return 0;
 for(size_t i=0;i<len;i++){unsigned c=MEM16(p+2*i),b=(unsigned char)s[i];if(c>127||((c>='A'&&c<='Z')?c+32:c)!=((b>='A'&&b<='Z')?b+32:b))return 0;}return MEM16(p+2*len)==0;
}
int h4_version_query(uint32_t block,const char *query,uint32_t out,uint32_t length) {
 check(out,4,1);check(length,4,1);MEM32(out)=MEM32(length)=0;check(block,6,0);uint32_t root_len=MEM16(block);node n;
 if(!parse(block,block+root_len,&n)){void_set_last_error(13);return 0;}
 const char *s=query;while(*s=='\\')s++;
 while(*s){const char *end=strchr(s,'\\');size_t len=end?(size_t)(end-s):strlen(s);uint32_t child=n.children;int found=0;node c;
  while(parse(child,n.end,&c)){if(key_equal(c.key,c.value,s,len)){n=c;found=1;break;}child=(c.end+3)&~3u;}
  if(!found){void_set_last_error(1813);return 0;}s=end?end+1:s+len;
 }
 if(n.type){uint32_t dest=block+((root_len+3)&~3u)+(n.value-block)/2;check(dest,n.count,1);for(uint32_t i=0;i<n.count;i++){uint8_t b;void_u16_to_cp1252(MEM16(n.value+2*i),&b);MEM8(dest+i)=b;}MEM32(out)=dest;}
 else MEM32(out)=n.value;
 MEM32(length)=n.count;return 1;
}
