/* GPL-3.0-or-later. Guest COM uses 32-bit pointers and stdcall, never host C++ vtables. */
#include "h4_host.h"
#include "surface_bridge.h"
#include <SDL3/SDL.h>
#define arg h4_arg
#define need h4_need
#define E_INVALIDARG 0x80070057u
#define E_NOINTERFACE 0x80004002u
#define E_NOTIMPL 0x80004001u
#define E_FAIL 0x80004005u
#define E_OUTOFMEMORY 0x8007000eu
#define DD_BUSY 0x887601aeu
#define DD_NOTLOCKED 0x88760248u
static const uint32_t iids[4][4]={{0},{0x15e65ec0,0x11d23b9c,0x60002fb9,0x5bea9797},{0x06675a80,0x11d23b9b,0x60002fb9,0x5bea9797},{0x6c14db85,0x11cea733,0x200021a5,0x60e50baf}};
static const uint32_t unknown_iid[4]={0,0,0xc0,0x46000000};
typedef struct {uint32_t va,type,refs,w,h,bits,pitch,caps,back,clipper,hwnd,key,has_key,locked,readonly,pixels;size_t bytes;void *surface;} object;
static object objects[256];
static uint32_t vtables[4],mode_w=800,mode_h=600,mode_bits=16,current_method;
extern SDL_Window *h4_sdl_window(void);
static object *find(uint32_t va) {for(unsigned i=0;i<256;i++)if(objects[i].va==va&&objects[i].refs)return objects+i;return NULL;}
static int guid(uint32_t va,const uint32_t *id) {need(va,16,0);return !memcmp(GPTR(va),id,16);}
static object *create(unsigned type) {
 unsigned i=0;while(i<256&&objects[i].refs)i++;if(i==256)return NULL;
 if(!vtables[type]){vtables[type]=void_galloc(64*4);if(!vtables[type])return NULL;for(unsigned j=0;j<64;j++)MEM32(vtables[type]+4*j)=0xd1000000u+(type<<16)+4*j;}
 uint32_t va=void_galloc(8);if(!va)return NULL;objects[i]=(object){.va=va,.type=type,.refs=1};MEM32(va)=vtables[type];return objects+i;
}
static uint32_t release(object *o) {
 if(--o->refs)return o->refs;
 if(o->surface)h4_surface_delete(o->surface);if(o->pixels)void_gfree(o->pixels);
 object *back=find(o->back),*clipper=find(o->clipper);if(back)release(back);if(clipper)release(clipper);
 void_gfree(o->va);memset(o,0,sizeof *o);return 0;
}
static void pixel_format(uint32_t out,unsigned bits) {
 need(out,32,1);memset(GPTR(out),0,32);MEM32(out)=32;MEM32(out+4)=0x40u|(bits==8?0x20u:0);MEM32(out+12)=bits;
 if(bits==16){MEM32(out+16)=0xf800;MEM32(out+20)=0x7e0;MEM32(out+24)=0x1f;}
}
static int description(object *o,uint32_t out,int locked) {
 need(out,4,0);if(MEM32(out)!=124)return 0;need(out,124,1);memset(GPTR(out),0,124);MEM32(out)=124;MEM32(out+4)=0x100fu|(locked?0x800u:0);MEM32(out+8)=o->h;MEM32(out+12)=o->w;MEM32(out+16)=o->pitch;MEM32(out+36)=locked?o->pixels:0;pixel_format(out+72,o->bits);MEM32(out+104)=o->caps;if(o->back){MEM32(out+4)|=0x20;MEM32(out+20)=1;}return 1;
}
static object *surface(unsigned w,unsigned h,unsigned bits,unsigned caps) {
 if(!w||!h||w>8192||h>8192||(bits!=8&&bits!=16))return NULL;
 object *o=create(2);if(!o)return NULL;o->w=w;o->h=h;o->bits=bits;o->pitch=(w*(bits/8)+3)&~3u;o->caps=caps;o->bytes=(size_t)o->pitch*h;
 o->surface=h4_surface_new((int)w,(int)h,(int)bits);o->pixels=void_galloc((uint32_t)o->bytes);if(!o->surface||!o->pixels){release(o);return NULL;}return o;
}
static int rect(uint32_t p,object *o,int *x,int *y,int *w,int *h) {
 *x=*y=0;*w=(int)o->w;*h=(int)o->h;if(!p)return 1;need(p,16,0);int64_t width=(int32_t)MEM32(p+8)-(int64_t)(int32_t)MEM32(p),height=(int32_t)MEM32(p+12)-(int64_t)(int32_t)MEM32(p+4);
 if(width<0||height<0||width>16384||height>16384)return 0;*x=(int32_t)MEM32(p);*y=(int32_t)MEM32(p+4);*w=(int)width;*h=(int)height;return 1;
}
static uint32_t present(object *o) {
 if(!(o->caps&0x200))return 0;if(!h4_sdl_window())return E_FAIL;
 if(!h4_surface_present(o->surface,h4_sdl_window())){void_log("SDL present failed: %s",h4_surface_error());return E_FAIL;}return 0;
}
uint32_t h4_directdraw_create(uint32_t adapter,uint32_t out,uint32_t iid,uint32_t outer) {
 need(out,4,1);MEM32(out)=0;if(outer||!iid)return E_INVALIDARG;if(!guid(iid,iids[1]))return E_NOINTERFACE;
 if(adapter&&adapter!=0xffffffffu){void_log("unsupported DirectDraw adapter %08x",adapter);return E_NOTIMPL;}
 object *o=create(1);if(!o)return E_OUTOFMEMORY;MEM32(out)=o->va;void_log("real DirectDraw7 guest COM object %08x, 32-bit vtable %08x",o->va,vtables[1]);return 0;
}
static void invoke(void) {
 unsigned type=(current_method>>16)&15,slot=(current_method&0xffff)/4;uint32_t value=0,count=0,a,b,c;object *o=find(arg(1));
 if(!o||o->type!=type)void_fatal("invalid DirectDraw COM this=%08x class=%u slot=%u",arg(1),type,slot);
 if(getenv("H4_TRACE_DDRAW"))void_log("DDraw class=%u slot=%u this=%08x from=%08x",type,slot,o->va,g_cur_func);
 if(slot==0){count=3;need(arg(3),4,1);MEM32(arg(3))=0;if(guid(arg(2),iids[type])||guid(arg(2),unknown_iid)){o->refs++;MEM32(arg(3))=o->va;}else value=E_NOINTERFACE;}
 else if(slot==1){count=1;value=++o->refs;}
 else if(slot==2){count=1;value=release(o);}
 else if(type==1){switch(slot){
 case 4:{count=4;need(arg(3),4,1);MEM32(arg(3))=0;if(arg(2)||arg(4)){value=E_INVALIDARG;break;}object *clip=create(3);if(!clip){value=E_OUTOFMEMORY;break;}MEM32(arg(3))=clip->va;break;}
 case 6:{count=4;a=arg(2);need(a,124,0);need(arg(3),4,1);MEM32(arg(3))=0;if(MEM32(a)!=124||arg(4)){value=E_INVALIDARG;break;}uint32_t caps=MEM32(a+104),bits=(MEM32(a+4)&0x1000)?MEM32(a+84):mode_bits;unsigned w=(caps&0x200)?mode_w:MEM32(a+12),h=(caps&0x200)?mode_h:MEM32(a+8);
  if(caps&~(0x200u|0x40u|0x800u|0x4000u|4u|8u|0x10u)){value=E_NOTIMPL;break;}object *s=surface(w,h,bits,caps);if(!s){value=E_OUTOFMEMORY;break;}if(MEM32(a+4)&0x20){if(MEM32(a+20)!=1){release(s);value=E_NOTIMPL;break;}object *back=surface(w,h,bits,4|0x4000);if(!back){release(s);value=E_OUTOFMEMORY;break;}s->back=back->va;}MEM32(arg(3))=s->va;void_log("DirectDraw surface %08x %ux%ux%u caps=%08x",s->va,w,h,bits,caps);break;}
 case 8:{count=5;if(arg(2)&~3u){value=E_INVALIDARG;break;}uint32_t desc=void_galloc(124);if(!desc){value=E_OUTOFMEMORY;break;}MEM32(desc)=124;object mode={.w=mode_w,.h=mode_h,.bits=mode_bits,.pitch=mode_w*(mode_bits/8)};description(&mode,desc,0);uint32_t args[]={desc,arg(4)};h4_call_guest(arg(5),2,args);void_gfree(desc);break;}
 case 11:count=3;for(unsigned i=2;i<=3;i++){a=arg(i);if(!a)continue;need(a,4,0);b=MEM32(a);if(b<172||b>380){value=E_INVALIDARG;break;}need(a,b,1);memset(GPTR(a),0,b);MEM32(a)=b;if(i==3){MEM32(a+4)=0x04400040;MEM32(a+12)=0x200;MEM32(a+60)=MEM32(a+64)=0x10000000;}}break;
 case 12:{count=2;object mode={.w=mode_w,.h=mode_h,.bits=mode_bits,.pitch=(mode_w*(mode_bits/8)+3)&~3u};if(!description(&mode,arg(2),0))value=E_INVALIDARG;break;}
 case 13:count=3;need(arg(2),4,1);MEM32(arg(2))=0;break;
 case 15:count=2;need(arg(2),4,1);MEM32(arg(2))=60;break;
 case 19:count=1;mode_w=800;mode_h=600;mode_bits=16;if(h4_sdl_window()&&!SDL_SetWindowSize(h4_sdl_window(),800,600))value=E_FAIL;break;
 case 20:count=3;void_log("DirectDraw cooperative flags=%08x",arg(3));if(!h4_sdl_window()||arg(2)!=0x50000001){value=E_INVALIDARG;break;}if(arg(3)&~0x1f5fu){value=E_NOTIMPL;break;}o->hwnd=arg(2);break;
 case 21:count=6;void_log("DirectDraw display mode %ux%ux%u",arg(2),arg(3),arg(4));if(!arg(2)||!arg(3)||arg(2)>8192||arg(3)>8192||(arg(4)!=8&&arg(4)!=16)||arg(6)||(arg(5)&&arg(5)!=60)){value=E_NOTIMPL;break;}if(!h4_sdl_window()||!SDL_SetWindowSize(h4_sdl_window(),(int)arg(2),(int)arg(3))){value=E_FAIL;break;}mode_w=arg(2);mode_h=arg(3);mode_bits=arg(4);break;
 case 23:count=4;need(arg(2),16,0);if(arg(3)){need(arg(3),4,1);MEM32(arg(3))=0x10000000;}if(arg(4)){need(arg(4),4,1);MEM32(arg(4))=0x10000000;}break;
 case 26:count=1;value=h4_sdl_window()?0:E_FAIL;break;
 default:goto unsupported;
 }}else if(type==2){switch(slot){
 case 5:{count=6;int x,y,w,h;if(!rect(arg(2),o,&x,&y,&w,&h)){value=E_INVALIDARG;break;}uint32_t flags=arg(5);void_log("Blt flags=%08x dest=%d,%d %dx%d src=%08x",flags,x,y,w,h,arg(3));if(o->locked){value=DD_BUSY;break;}if(flags&~(0x1000000u|0x400u|0x8000u|0x20000u)){value=E_NOTIMPL;break;}if(flags&0x400){a=arg(6);need(a,100,0);if(MEM32(a)!=100){value=E_INVALIDARG;break;}if(!h4_surface_fill(o->surface,x,y,w,h,MEM32(a+80)))value=E_FAIL;}
  else{object *src=find(arg(3));if(!src||src->type!=2){value=E_INVALIDARG;break;}int sx,sy,sw,sh;if(!rect(arg(4),src,&sx,&sy,&sw,&sh)){value=E_INVALIDARG;break;}if(w!=sw||h!=sh){void_log("Blt stretch needed src=%d,%d %dx%d dest=%dx%d",sx,sy,sw,sh,w,h);value=E_NOTIMPL;break;}if(flags&0x20000){need(arg(6),100,0);if(MEM32(arg(6)+8)!=0xcc0020){value=E_NOTIMPL;break;}}if((flags&0x8000)&&!src->has_key){value=E_INVALIDARG;break;}if(!h4_surface_blt(o->surface,x,y,src->surface,sx,sy,sw,sh,flags&0x8000?(int)src->key:-1))value=E_FAIL;}if(!value)value=present(o);break;}
 case 7:{count=6;object *src=find(arg(4));if(!src||src->type!=2){value=E_INVALIDARG;break;}int sx,sy,w,h;if(!rect(arg(5),src,&sx,&sy,&w,&h)){value=E_INVALIDARG;break;}if(arg(6)&~0x31u){value=E_NOTIMPL;break;}if((arg(6)&1)&&!src->has_key){value=E_INVALIDARG;break;}if(!h4_surface_blt(o->surface,(int)arg(2),(int)arg(3),src->surface,sx,sy,w,h,arg(6)&1?(int)src->key:-1))value=E_FAIL;if(!value)value=present(o);break;}
 case 11:{count=3;object *back=find(arg(2)?arg(2):o->back);if(!back||back->type!=2){value=E_INVALIDARG;break;}if(arg(3)&~0x11u){value=E_NOTIMPL;break;}if(!h4_surface_flip(o->surface,back->surface))value=E_FAIL;if(!value)value=present(o);break;}
 case 12:{count=3;need(arg(2),16,0);need(arg(3),4,1);MEM32(arg(3))=0;object *back=find(o->back);if(!back||!(MEM32(arg(2))&4)){value=E_INVALIDARG;break;}back->refs++;MEM32(arg(3))=back->va;break;}
 case 13:case 18:count=2;value=o->locked?DD_BUSY:0;break;
 case 14:count=2;need(arg(2),16,1);memset(GPTR(arg(2)),0,16);MEM32(arg(2))=o->caps;break;
 case 16:count=3;if(arg(2)!=8||!o->has_key){value=E_INVALIDARG;break;}need(arg(3),8,1);MEM32(arg(3))=MEM32(arg(3)+4)=o->key;break;
 case 21:count=2;need(arg(2),4,0);if(MEM32(arg(2))!=32)value=E_INVALIDARG;else pixel_format(arg(2),o->bits);break;
 case 22:count=2;if(!description(o,arg(2),0))value=E_INVALIDARG;break;
 case 24:case 27:count=1;break; /* Host-owned surfaces cannot be lost. */
 case 25:{count=5;if(o->locked){value=DD_BUSY;break;}if(arg(5)||arg(4)&~0x6831u){value=E_NOTIMPL;break;}int x,y,w,h;if(!rect(arg(2),o,&x,&y,&w,&h)||x<0||y<0||x+w>(int)o->w||y+h>(int)o->h){value=E_INVALIDARG;break;}if(!description(o,arg(3),1)){value=E_INVALIDARG;break;}void *pixels;size_t bytes;if(!h4_surface_lock(o->surface,&pixels,&bytes)){value=E_FAIL;break;}if(bytes!=o->bytes)void_fatal("surface pitch mismatch");memcpy(GPTR(o->pixels),pixels,bytes);o->locked=1;o->readonly=!!(arg(4)&0x10);MEM32(arg(3)+36)=o->pixels+(uint32_t)y*o->pitch+(uint32_t)x*(o->bits/8);MEM32(arg(3)+8)=h;MEM32(arg(3)+12)=w;break;}
 case 28:{count=2;object *clip=arg(2)?find(arg(2)):NULL;if(arg(2)&&(!clip||clip->type!=3)){value=E_INVALIDARG;break;}object *old=find(o->clipper);if(clip)clip->refs++;o->clipper=arg(2);if(old)release(old);break;}
 case 29:count=3;if(arg(2)!=8){value=E_NOTIMPL;break;}if(!arg(3)){o->has_key=0;break;}need(arg(3),8,0);if(MEM32(arg(3))!=MEM32(arg(3)+4)){value=E_NOTIMPL;break;}o->has_key=1;o->key=MEM32(arg(3));break;
 case 32:count=2;if(!o->locked){value=DD_NOTLOCKED;break;}if(!h4_surface_unlock(o->surface,o->readonly?NULL:GPTR(o->pixels),o->bytes)){value=E_FAIL;break;}o->locked=0;if(!o->readonly)value=present(o);break;
 default:goto unsupported;
 }}else if(type==3){switch(slot){case 4:count=2;need(arg(2),4,1);MEM32(arg(2))=o->hwnd;break;case 8:count=3;if(arg(2)||arg(3)!=0x50000001){value=E_INVALIDARG;break;}o->hwnd=arg(3);break;case 6:count=2;need(arg(2),4,1);MEM32(arg(2))=0;break;default:goto unsupported;}}
 else goto unsupported;
 if(value&0x80000000u)void_log("DirectDraw failure class=%u slot=%u HRESULT=%08x from=%08x",type,slot,value,g_cur_func);
 (void)c;g_eax=value;g_esp+=4+4*count;return;
unsupported:void_log("unimplemented DirectDraw COM class=%u slot=%u from=%08x",type,slot,g_cur_func);exit(77);
}
recomp_func_t h4_ddraw_resolve(uint32_t va) {
 if((va&0xfff00000u)!=0xd1000000u||(va&3)||(va&0xffff)>=256)return NULL;current_method=va;return invoke;
}

/* GDI screen DCs share the actual primary surface, not a separate fake canvas. */
int h4_ddraw_primary_io(int write,int x,int y,int w,int h,uint16_t *data,int stride)
{
 object *o=NULL;for(unsigned i=0;i<256;i++)if(objects[i].refs&&objects[i].type==2&&(objects[i].caps&0x200)){o=objects+i;break;}
 if(!o||o->bits!=16||o->locked||w<0||h<0||stride<w)return 0;void *pixels;size_t bytes;
 if(!h4_surface_lock(o->surface,&pixels,&bytes))return 0;
 for(int row=0;row<h;row++)for(int col=0;col<w;col++){
  int sx=x+col,sy=y+row;if(sx<0||sy<0||sx>=(int)o->w||sy>=(int)o->h)continue;
  uint8_t *p=(uint8_t*)pixels+(size_t)sy*o->pitch+2*(unsigned)sx;
  if(write)memcpy(p,data+(size_t)row*stride+col,2);else memcpy(data+(size_t)row*stride+col,p,2);
 }
 if(!h4_surface_unlock(o->surface,NULL,bytes))return 0;return !write||!present(o);
}
