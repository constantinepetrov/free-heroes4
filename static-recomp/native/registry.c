/* GPL-3.0-or-later. Private persistent registry; no Wine registry, product keys or game data. */
#include "h4_host.h"
#include <errno.h>
#include <strings.h>
#include <unistd.h>
typedef struct {char *path;} key;
typedef struct {unsigned key,type,size;char *name;void *data;} value;
static key keys[256];static value values[1024];static unsigned key_count,value_count;
static struct {unsigned key,rights;} handles[256];
static int initialized;
static char state_path[4096];
static int save(void) {
 const char *dir=getenv("H4_STATE_DIR");if(!dir)return 5;char temp[4096];if(snprintf(temp,sizeof temp,"%s/registry-XXXXXX",dir)>=(int)sizeof temp)return 87;
 int fd=mkstemp(temp);if(fd<0)return 5;FILE *f=fdopen(fd,"wb");if(!f){close(fd);unlink(temp);return 5;}int ok=fwrite("H4REG001",1,8,f)==8;
#define WRITE(p,n) do{if(fwrite((p),1,(n),f)!=(size_t)(n))ok=0;}while(0)
 WRITE(&key_count,4);WRITE(&value_count,4);
 for(unsigned i=0;i<key_count;i++){uint32_t n=(uint32_t)strlen(keys[i].path)+1;WRITE(&n,4);WRITE(keys[i].path,n);}
 for(unsigned i=0;i<value_count;i++){value *v=values+i;uint32_t fields[]={v->key,v->type,v->size,(uint32_t)strlen(v->name)+1};WRITE(fields,16);WRITE(v->name,fields[3]);WRITE(v->data,v->size);}
#undef WRITE
 if(fflush(f)||fsync(fd))ok=0;if(fclose(f))ok=0;if(!ok||rename(temp,state_path)){unlink(temp);return 5;}return 0;
}
static char *read_string(FILE *f,unsigned n) {if(!n||n>32768)return NULL;char *s=malloc(n);if(!s)return NULL;if(fread(s,1,n,f)!=n||s[n-1]||memchr(s,0,n-1)){free(s);return NULL;}return s;}
static int init(void) {
 if(initialized)return initialized==1?0:13;
 const char *dir=getenv("H4_STATE_DIR");if(!dir){void_log("H4_STATE_DIR required for a persistent registry");return 5;}
 if(snprintf(state_path,sizeof state_path,"%s/registry.bin",dir)>=(int)sizeof state_path)return 87;
 FILE *f=fopen(state_path,"rb");if(!f){if(errno!=ENOENT)return 5;keys[0].path=strdup("HKLM");keys[1].path=strdup("HKCU");key_count=2;initialized=1;return 0;}
 char magic[8];uint32_t nk=0,nv=0;int ok=fread(magic,1,8,f)==8&&!memcmp(magic,"H4REG001",8)&&fread(&nk,4,1,f)==1&&fread(&nv,4,1,f)==1&&nk>=2&&nk<=256&&nv<=1024;
 for(unsigned i=0;ok&&i<nk;i++){uint32_t n;if(fread(&n,4,1,f)!=1||!(keys[i].path=read_string(f,n)))ok=0;}
 for(unsigned i=0;ok&&i<nv;i++){uint32_t fields[4];if(fread(fields,4,4,f)!=4||fields[0]>=nk||fields[2]>1048576){ok=0;break;}value *v=values+i;v->key=fields[0];v->type=fields[1];v->size=fields[2];v->name=read_string(f,fields[3]);v->data=malloc(v->size?v->size:1);if(!v->name||!v->data||fread(v->data,1,v->size,f)!=v->size)ok=0;}
 if(ok&&(strcmp(keys[0].path,"HKLM")||strcmp(keys[1].path,"HKCU")||fgetc(f)!=EOF))ok=0;fclose(f);initialized=ok?1:-1;if(!ok){void_log("invalid private registry preserved: %s",state_path);return 13;}key_count=nk;value_count=nv;return 0;
}
static int resolve(uint32_t h,unsigned rights,unsigned *index) {
 if(h==0x80000002u){*index=0;return 0;}if(h==0x80000001u){*index=1;return 0;}unsigned i=h-0x53000000u;
 if(i>=256||!handles[i].key)return 6;if((handles[i].rights&rights)!=rights)return 5;*index=handles[i].key-1;return 0;
}
static int open_handle(unsigned k,unsigned rights,uint32_t out) {
 h4_need(out,4,1);if(rights&0x100)return 50;for(unsigned i=0;i<256;i++)if(!handles[i].key){handles[i].key=k+1;handles[i].rights=rights;MEM32(out)=0x53000000u+i;return 0;}return 8;
}
static int find_key(const char *path) {for(unsigned i=0;i<key_count;i++)if(!strcasecmp(path,keys[i].path))return (int)i;return -1;}
static int subkey(unsigned parent,const char *sub,int create,unsigned *index) {
 char path[32768];if(snprintf(path,sizeof path,"%s%s%s",keys[parent].path,*sub?"\\":"",sub)>=(int)sizeof path)return 87;
 int found=find_key(path);if(found>=0){*index=(unsigned)found;return 0;}if(!create)return 2;
 unsigned original=key_count;for(char *p=path+strlen(keys[parent].path)+1;;p++){if(*p!='\\'&&*p)continue;char end=*p;*p=0;if(find_key(path)<0){if(key_count==256){while(key_count>original)free(keys[--key_count].path);return 8;}keys[key_count++].path=strdup(path);}*p=end;if(!end)break;}
 int e=save();if(e){while(key_count>original)free(keys[--key_count].path);return e;}*index=(unsigned)find_key(path);return 0;
}
int h4_registry_api(const char *name,uint32_t *result,unsigned *argc) {
 unsigned count;if(!strcmp(name,"RegCreateKeyExA"))count=9;else if(!strcmp(name,"RegOpenKeyExA"))count=5;else if(!strcmp(name,"RegCloseKey"))count=1;else if(!strcmp(name,"RegQueryValueExA"))count=6;else if(!strcmp(name,"RegSetValueExA"))count=6;else return 0;
 uint32_t e=init();unsigned k;if(e)goto done;e=resolve(h4_arg(1),!strcmp(name,"RegSetValueExA")?2:!strcmp(name,"RegQueryValueExA")?1:!strcmp(name,"RegCreateKeyExA")?4:0,&k);if(e)goto done;
 if(!strcmp(name,"RegCloseKey")){if(h4_arg(1)<0x80000000)memset(handles+(h4_arg(1)-0x53000000),0,sizeof handles[0]);}
 else if(!strcmp(name,"RegCreateKeyExA")||!strcmp(name,"RegOpenKeyExA")){int create=count==9;unsigned output=create?8:5,access=create?6:4;if(h4_arg(3)||(create&&(h4_arg(5)||h4_arg(7)))){e=50;goto done;}const char *s=h4_arg(2)?h4_string(h4_arg(2)):"";unsigned before=key_count,index;e=subkey(k,s,create,&index);if(e)goto done;e=open_handle(index,h4_arg(access),h4_arg(output));if(!e&&create&&h4_arg(9)){h4_need(h4_arg(9),4,1);MEM32(h4_arg(9))=key_count>before?1:2;}}
 else {if(h4_arg(3)){e=87;goto done;}const char *s=h4_arg(2)?h4_string(h4_arg(2)):"";unsigned i=0;while(i<value_count&&(values[i].key!=k||strcasecmp(values[i].name,s)))i++;
  if(!strcmp(name,"RegQueryValueExA")){if(i==value_count){e=2;goto done;}value *v=values+i;if(h4_arg(4)){h4_need(h4_arg(4),4,1);MEM32(h4_arg(4))=v->type;}if(h4_arg(5)&&!h4_arg(6)){e=87;goto done;}if(h4_arg(6)){h4_need(h4_arg(6),4,1);uint32_t cap=MEM32(h4_arg(6));MEM32(h4_arg(6))=v->size;if(h4_arg(5)){if(cap<v->size){e=234;goto done;}h4_need(h4_arg(5),v->size,1);memcpy(GPTR(h4_arg(5)),v->data,v->size);}}}
  else {unsigned type=h4_arg(4),size=h4_arg(6);if((type!=1&&type!=3&&type!=4)||(type==4&&size!=4)||size>1048576){e=50;goto done;}if(i==1024){e=8;goto done;}h4_need(h4_arg(5),size,0);value old=values[i],v={k,type,size,strdup(s),malloc(size?size:1)};if(!v.name||!v.data){free(v.name);free(v.data);e=8;goto done;}memcpy(v.data,GPTR(h4_arg(5)),size);values[i]=v;int added=i==value_count;if(added)value_count++;e=save();if(e){values[i]=old;if(added)value_count--;free(v.name);free(v.data);}else{free(old.name);free(old.data);}}
 }
done:*result=e;*argc=count;return 1;
}
