/* GPL-3.0-or-later. Read-only resource mount plus separate writable PoC user files. */
#define _GNU_SOURCE
#include "h4_host.h"
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <limits.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>
#define arg h4_arg
#define need h4_need
#define str h4_string
static char resource_root[PATH_MAX],write_root[PATH_MAX],cwd[PATH_MAX];
static int ready;
static struct {int used,fd,write;} files[256];
static struct {int used,fd,write;uint32_t size;} mappings[128];
static uint32_t views[128];
static struct {DIR *dir;char parent[PATH_MAX],pattern[256];} searches[64];
static int error(void) {int e=errno==ENOENT?2:errno==EACCES||errno==EPERM||errno==ELOOP?5:errno==EEXIST?80:errno==ENOTDIR?3:errno==ENOSPC?112:errno==EBADF?6:87;void_set_last_error((uint32_t)e);return e;}
static int init(void) {
 if(ready)return 1;const char *root=getenv("H4_GAME_ROOT"),*state=getenv("H4_STATE_DIR");if(!root||!state||!realpath(root,resource_root)){void_log("valid H4_GAME_ROOT and H4_STATE_DIR required for files");void_set_last_error(3);return 0;}
 char buf[PATH_MAX];if(snprintf(buf,sizeof buf,"%s/files",state)>=(int)sizeof buf){void_set_last_error(87);return 0;}if(mkdir(buf,0700)&&errno!=EEXIST){error();return 0;}if(!realpath(buf,write_root)){error();return 0;}ready=1;return 1;
}
static int normalize(const char *input,char *out) {
 char combined[PATH_MAX],buffer[PATH_MAX];if(strlen(input)>=sizeof buffer){void_set_last_error(206);return 0;}strcpy(buffer,input);for(char *p=buffer;*p;p++)if(*p=='\\')*p='/';char *p=buffer;int absolute=0;
 if(p[0]&&p[1]==':'){if(tolower((unsigned char)p[0])!='c'){void_set_last_error(15);return 0;}p+=2;if(*p=='/'){if(strncasecmp(p,"/HeroesIV",9)||(p[9]&&p[9]!='/')){void_set_last_error(5);return 0;}p+=9;absolute=1;}}
 if(*p=='/'||absolute){while(*p=='/')p++;if(snprintf(combined,sizeof combined,"%s",p)>=(int)sizeof combined)return 0;}else if(snprintf(combined,sizeof combined,"%s%s%s",cwd,*cwd?"/":"",p)>=(int)sizeof combined)return 0;
 out[0]=0;size_t n=0;char *save=NULL;for(char *part=strtok_r(combined,"/",&save);part;part=strtok_r(NULL,"/",&save)){if(!strcmp(part,"."))continue;if(!strcmp(part,"..")){if(!n){void_set_last_error(5);return 0;}while(n&&out[n-1]!='/')n--;if(n)n--;out[n]=0;continue;}if(strchr(part,':')){void_set_last_error(123);return 0;}size_t len=strlen(part);if(n+len+2>PATH_MAX){void_set_last_error(206);return 0;}if(n)out[n++]='/';memcpy(out+n,part,len+1);n+=len;}
 return 1;
}
static int inside(const char *root,const char *path){size_t n=strlen(root);return !strncmp(root,path,n)&&(!path[n]||path[n]=='/');}
/* Resolve each existing component case-insensitively, then reject symlink escapes. */
static int resolve(const char *root,const char *rel,char *out) {
 char path[PATH_MAX],copy[PATH_MAX];strcpy(path,root);strcpy(copy,rel);char *save=NULL;
 for(char *part=strtok_r(copy,"/",&save);part;part=strtok_r(NULL,"/",&save)){
  DIR *d=opendir(path);if(!d)return 0;struct dirent *entry;char name[NAME_MAX+1]={0};while((entry=readdir(d)))if(!strcasecmp(entry->d_name,part)){snprintf(name,sizeof name,"%s",entry->d_name);break;}closedir(d);if(!*name)return 0;size_t n=strlen(path);if(n+strlen(name)+2>sizeof path)return 0;path[n++]='/';strcpy(path+n,name);
 }
 return realpath(path,out)&&inside(root,out);
}
static int existing(const char *rel,char *out,int *private_file) {if(resolve(write_root,rel,out)){if(private_file)*private_file=1;return 1;}if(resolve(resource_root,rel,out)){if(private_file)*private_file=0;return 1;}void_set_last_error(2);return 0;}
static int file_index(uint32_t handle){unsigned i=handle-0x54000000u;return i<256&&files[i].used?(int)i:-1;}
int h4_file_close(uint32_t handle) {int i=file_index(handle);if(i>=0){int ok=close(files[i].fd)==0;files[i].used=0;if(!ok)error();return ok;}unsigned m=handle-0x55000000u;if(m<128&&mappings[m].used){int ok=close(mappings[m].fd)==0;mappings[m].used=0;if(!ok)error();return ok;}return 0;}
uint32_t h4_file_type(uint32_t handle){return file_index(handle)>=0?1:0;}
static int fill_find(unsigned index,uint32_t out) {
 struct dirent *entry;while((entry=readdir(searches[index].dir))){if(!strcmp(entry->d_name,".")||!strcmp(entry->d_name,"..")||!strncmp(entry->d_name,"._",2)||!strcmp(entry->d_name,".DS_Store"))continue;
 const char *pattern=!strcmp(searches[index].pattern,"*.*")?"*":searches[index].pattern;if(fnmatch(pattern,entry->d_name,FNM_CASEFOLD))continue;struct stat st;if(fstatat(dirfd(searches[index].dir),entry->d_name,&st,AT_SYMLINK_NOFOLLOW)||S_ISLNK(st.st_mode))continue;
 need(out,320,1);memset(GPTR(out),0,320);MEM32(out)=S_ISDIR(st.st_mode)?0x10:0x20;uint64_t ft=((uint64_t)st.st_mtime+11644473600ull)*10000000ull;MEM32(out+20)=(uint32_t)ft;MEM32(out+24)=(uint32_t)(ft>>32);MEM32(out+28)=(uint32_t)((uint64_t)st.st_size>>32);MEM32(out+32)=(uint32_t)st.st_size;snprintf(GPTR(out+44),260,"%s",entry->d_name);return 1;
 }void_set_last_error(18);return 0;
}
int h4_file_api(const char *name,uint32_t *result,unsigned *argc) {
 static const struct {const char *name;unsigned argc;} supported[]={{"SetCurrentDirectoryA",1},{"GetCurrentDirectoryA",2},{"GetFullPathNameA",4},{"CreateFileA",7},{"GetFileSize",2},{"SetFilePointer",4},{"ReadFile",5},{"WriteFile",5},{"FlushFileBuffers",1},{"SetEndOfFile",1},{"CreateFileMappingA",6},{"MapViewOfFile",5},{"MapViewOfFileEx",6},{"UnmapViewOfFile",1},{"FindFirstFileA",2},{"FindNextFileA",2},{"FindClose",1}};
 unsigned count=0;for(unsigned i=0;i<sizeof supported/sizeof *supported;i++)if(!strcmp(name,supported[i].name)){count=supported[i].argc;break;}if(!count)return 0;
 uint32_t value=0,a,b;char rel[PATH_MAX],path[PATH_MAX];if(!init())goto done;
#define API(n) if(!strcmp(name,n)){
#define NEXT } else
 API("SetCurrentDirectoryA") void_log("SetCurrentDirectoryA %s",str(arg(1)));if(normalize(str(arg(1)),rel)&&existing(rel,path,NULL)){struct stat st;if(!stat(path,&st)&&S_ISDIR(st.st_mode)){strcpy(cwd,rel);value=1;}else void_set_last_error(3);}
 NEXT API("GetCurrentDirectoryA") char full[PATH_MAX+16];snprintf(full,sizeof full,"C:\\HeroesIV%s%s",*cwd?"\\":"",cwd);for(char *p=full;*p;p++)if(*p=='/')*p='\\';unsigned n=(unsigned)strlen(full);if(arg(1)<=n)value=n+1;else{need(arg(2),n+1,1);memcpy(GPTR(arg(2)),full,n+1);value=n;}
 NEXT API("GetFullPathNameA") if(normalize(str(arg(1)),rel)){char full[PATH_MAX+16];snprintf(full,sizeof full,"C:\\HeroesIV%s%s",*rel?"\\":"",rel);for(char *p=full;*p;p++)if(*p=='/')*p='\\';unsigned n=(unsigned)strlen(full);if(arg(2)<=n)value=n+1;else{need(arg(3),n+1,1);memcpy(GPTR(arg(3)),full,n+1);value=n;if(arg(4)){need(arg(4),4,1);char *last=strrchr(full,'\\');MEM32(arg(4))=arg(3)+(uint32_t)(last?last+1-full:0);}}}
 NEXT API("CreateFileA") void_log("CreateFileA %s access=%08x share=%x creation=%u flags=%08x security=%08x template=%08x",str(arg(1)),arg(2),arg(3),arg(5),arg(6),arg(4),arg(7));value=0xffffffffu;int inherit=0;if(arg(4)){need(arg(4),12,0);if(MEM32(arg(4))!=12||MEM32(arg(4)+4)){void_set_last_error(50);goto done;}inherit=!!MEM32(arg(4)+8);}
 if(arg(7)||arg(2)&~0xc0000000u||arg(5)<1||arg(5)>5){void_set_last_error(50);goto done;}if(!normalize(str(arg(1)),rel))goto done;int private_file=0,exists=existing(rel,path,&private_file),writable=!!(arg(2)&0x40000000u);if(exists&&!private_file&&writable){void_log("resource write denied: %s",rel);void_set_last_error(5);goto done;}
 if(!exists){if(arg(5)==3||arg(5)==5){void_set_last_error(2);goto done;}char parent[PATH_MAX],leaf[NAME_MAX+1];strcpy(parent,rel);char *slash=strrchr(parent,'/');snprintf(leaf,sizeof leaf,"%s",slash?slash+1:parent);if(slash)*slash=0;else *parent=0;if(!*leaf||!resolve(write_root,parent,path)){void_set_last_error(3);goto done;}size_t n=strlen(path);if(n+strlen(leaf)+2>sizeof path){void_set_last_error(206);goto done;}path[n++]='/';strcpy(path+n,leaf);private_file=1;}
 int flags=writable?((arg(2)&0x80000000u)?O_RDWR:O_WRONLY):O_RDONLY;if(arg(5)==1)flags|=O_CREAT|O_EXCL;else if(arg(5)==2)flags|=O_CREAT|O_TRUNC;else if(arg(5)==4&&!exists)flags|=O_CREAT;else if(arg(5)==5)flags|=O_TRUNC;if((flags&O_TRUNC)&&!writable){void_set_last_error(5);goto done;}unsigned i=0;while(i<256&&files[i].used)i++;if(i==256){void_set_last_error(4);goto done;}int fd=open(path,flags|O_NOFOLLOW|(inherit?0:O_CLOEXEC),0600);if(fd<0){error();goto done;}struct stat st;if(fstat(fd,&st)||!S_ISREG(st.st_mode)){close(fd);void_set_last_error(5);goto done;}files[i]=(typeof(files[0])){1,fd,writable};value=0x54000000u+i;void_set_last_error(exists&&(arg(5)==2||arg(5)==4)?183:0);void_log("guest file %s: %s",writable?"private write":"read",rel);
 NEXT API("GetFileSize") int i=file_index(arg(1));struct stat st;if(i<0||fstat(files[i].fd,&st)){value=0xffffffff;void_set_last_error(6);}else{value=(uint32_t)st.st_size;if(arg(2)){need(arg(2),4,1);MEM32(arg(2))=(uint32_t)((uint64_t)st.st_size>>32);}void_set_last_error(0);}
 NEXT API("SetFilePointer") int i=file_index(arg(1));value=0xffffffff;if(i<0){void_set_last_error(6);goto done;}int64_t distance=(int32_t)arg(2);if(arg(3)){need(arg(3),4,1);distance=(int64_t)(((uint64_t)MEM32(arg(3))<<32)|arg(2));}if(arg(4)>2){void_set_last_error(87);goto done;}off_t pos=lseek(files[i].fd,(off_t)distance,(int)arg(4));if(pos<0){error();goto done;}value=(uint32_t)pos;if(arg(3))MEM32(arg(3))=(uint32_t)((uint64_t)pos>>32);void_set_last_error(0);
 NEXT API("ReadFile") int i=file_index(arg(1));if(i<0){void_set_last_error(6);goto done;}if(arg(5)){void_set_last_error(50);goto done;}need(arg(2),arg(3),1);if(arg(4)){need(arg(4),4,1);MEM32(arg(4))=0;}ssize_t n;do{n=read(files[i].fd,GPTR(arg(2)),arg(3));}while(n<0&&errno==EINTR);if(n<0)error();else{if(arg(4))MEM32(arg(4))=(uint32_t)n;value=1;}
 NEXT API("WriteFile") int i=file_index(arg(1));int fd=i>=0?files[i].fd:arg(1)==0x101?1:arg(1)==0x102?2:-1;if(fd<0){void_set_last_error(6);goto done;}if((i>=0&&!files[i].write)||arg(5)){void_set_last_error(5);goto done;}need(arg(2),arg(3),0);if(arg(4)){need(arg(4),4,1);MEM32(arg(4))=0;}ssize_t n;do{n=write(fd,GPTR(arg(2)),arg(3));}while(n<0&&errno==EINTR);if(n<0)error();else{if(arg(4))MEM32(arg(4))=(uint32_t)n;value=1;}
 NEXT API("FlushFileBuffers") int i=file_index(arg(1));if(i<0)void_set_last_error(6);else if(!files[i].write)void_set_last_error(5);else if(fsync(files[i].fd))error();else value=1;
 NEXT API("SetEndOfFile") int i=file_index(arg(1));if(i<0)void_set_last_error(6);else if(!files[i].write)void_set_last_error(5);else{off_t pos=lseek(files[i].fd,0,SEEK_CUR);if(pos<0||ftruncate(files[i].fd,pos))error();else value=1;}
 NEXT API("CreateFileMappingA") int i=file_index(arg(1));if(i<0){void_set_last_error(6);goto done;}if(arg(2)||arg(4)||arg(6)||(arg(3)!=2&&arg(3)!=4)||(arg(3)==4&&!files[i].write)){void_set_last_error(50);goto done;}struct stat st;if(fstat(files[i].fd,&st)){error();goto done;}uint64_t size=arg(5)?arg(5):(uint64_t)st.st_size;if(!size||size>0x7fff0000u||size>(uint64_t)st.st_size){void_set_last_error(87);goto done;}unsigned j=0;while(j<128&&mappings[j].used)j++;if(j==128){void_set_last_error(4);goto done;}int fd=dup(files[i].fd);if(fd<0){error();goto done;}mappings[j]=(typeof(mappings[0])){1,fd,arg(3)==4,(uint32_t)size};value=0x55000000u+j;
 NEXT API("MapViewOfFile") goto map_view;
 NEXT API("MapViewOfFileEx") if(arg(6)){void_set_last_error(50);goto done;}goto map_view;
 NEXT API("UnmapViewOfFile") unsigned j=0;while(j<128&&views[j]!=arg(1))j++;if(j==128||!arg(1))void_set_last_error(487);else{value=void_vm_unmap_file(views[j]);if(value)views[j]=0;}
 NEXT API("FindFirstFileA") void_log("FindFirstFileA %s cwd=%s",str(arg(1)),cwd);value=0xffffffff;if(!normalize(str(arg(1)),rel))goto done;char *slash=strrchr(rel,'/'),pattern[256];snprintf(pattern,sizeof pattern,"%s",slash?slash+1:rel);if(slash)*slash=0;else *rel=0;if(!existing(rel,path,NULL))goto done;unsigned i=0;while(i<64&&searches[i].dir)i++;if(i==64){void_set_last_error(4);goto done;}DIR *d=opendir(path);if(!d){error();goto done;}searches[i].dir=d;strcpy(searches[i].parent,path);strcpy(searches[i].pattern,pattern);if(fill_find(i,arg(2)))value=0x56000000u+i;else{closedir(d);searches[i].dir=NULL;void_set_last_error(2);}
 NEXT API("FindNextFileA") unsigned i=arg(1)-0x56000000u;if(i>=64||!searches[i].dir)void_set_last_error(6);else value=fill_find(i,arg(2));
 NEXT API("FindClose") unsigned i=arg(1)-0x56000000u;if(i>=64||!searches[i].dir)void_set_last_error(6);else{value=closedir(searches[i].dir)==0;searches[i].dir=NULL;}
 NEXT {void_fatal("file API registry mismatch");}
 goto done;
map_view:{unsigned i=arg(1)-0x55000000u;if(i>=128||!mappings[i].used){void_set_last_error(6);goto done;}if(arg(3)||arg(4)&0xffffu||arg(2)&~6u||(!(arg(2)&4)&&!(arg(2)&2))||((arg(2)&2)&&!mappings[i].write)){void_set_last_error(50);goto done;}a=arg(4);b=arg(5)?arg(5):mappings[i].size-a;if(a>=mappings[i].size||b>mappings[i].size-a||!b){void_set_last_error(87);goto done;}unsigned j=0;while(j<128&&views[j])j++;if(j==128){void_set_last_error(4);goto done;}value=void_vm_map_file(mappings[i].fd,a,b,!!(arg(2)&2),!!(arg(2)&2));if(value)views[j]=value;}
#undef API
#undef NEXT
done:*result=value;*argc=count;return 1;
}
