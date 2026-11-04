/* GPL-3.0-or-later. DIB pixels stay in the 32-bit guest; screen blits target DirectDraw. */
#include "h4_host.h"
#define arg h4_arg
#define need h4_need
#define SCREEN_DC 0x51000000u
#define STOCK_BITMAP 0x52000000u
static uint32_t dc_bitmap[128];
static unsigned char dc_used[128];
typedef struct {uint32_t pixels,w,h,stride,bits,red,green,blue;int top;} bitmap;
static bitmap bitmaps[128];
static int dc_index(uint32_t dc) {uint32_t i=dc-0x51000100u;return i<128&&dc_used[i]?(int)i:-1;}
static bitmap *find_bitmap(uint32_t handle) {uint32_t i=handle-STOCK_BITMAP-1;return i<128&&bitmaps[i].pixels?bitmaps+i:NULL;}
static unsigned component(uint32_t pixel,uint32_t mask,unsigned max) {if(!mask)return 0;unsigned shift=0;while(!(mask&1)){mask>>=1;shift++;}return (((pixel>>shift)&mask)*max+mask/2)/mask;}
static uint16_t get_pixel(bitmap *b,int x,int y) {
 if(x<0||y<0||x>=(int)b->w||y>=(int)b->h)return 0;unsigned row=b->top?(unsigned)y:b->h-1-(unsigned)y;
 uint32_t p=b->pixels+row*b->stride+(unsigned)x*(b->bits/8),v=0;memcpy(&v,GPTR(p),b->bits/8);
 return (uint16_t)((component(v,b->red,31)<<11)|(component(v,b->green,63)<<5)|component(v,b->blue,31));
}
static void put_pixel(bitmap *b,int x,int y,uint16_t c) {
 if(x<0||y<0||x>=(int)b->w||y>=(int)b->h)return;unsigned row=b->top?(unsigned)y:b->h-1-(unsigned)y;
 uint32_t r=(c>>11)*255/31,g=((c>>5)&63)*255/63,blue=(c&31)*255/31,v;
 if(b->bits==16)v=b->red==0xf800?c:((r>>3)<<10)|((g>>3)<<5)|(blue>>3);else v=(r<<16)|(g<<8)|blue;
 memcpy(GPTR(b->pixels+row*b->stride+(unsigned)x*(b->bits/8)),&v,b->bits/8);
}
int h4_gdi_api(const char *name,uint32_t *result,unsigned *argc) {
 uint32_t value=0,count=0,a,b;
#define API(n,c) if(!strcmp(name,n)){count=c;
#define NEXT } else
 API("GetDC",1) if(!arg(1)||arg(1)==0x50000001)value=SCREEN_DC;else void_set_last_error(1400);
 NEXT API("ReleaseDC",2) if((!arg(1)||arg(1)==0x50000001)&&arg(2)==SCREEN_DC)value=1;else void_set_last_error(6);
 NEXT API("CreateCompatibleDC",1) if(arg(1)&&arg(1)!=SCREEN_DC&&dc_index(arg(1))<0){void_set_last_error(6);}else{for(unsigned i=0;i<128;i++)if(!dc_used[i]){dc_used[i]=1;dc_bitmap[i]=STOCK_BITMAP;value=0x51000100u+i;break;}if(!value)void_set_last_error(8);}
 NEXT API("DeleteDC",1) int i=dc_index(arg(1));if(i<0)void_set_last_error(6);else{dc_used[i]=0;dc_bitmap[i]=0;value=1;}
 NEXT API("CreateDIBSection",6) a=arg(2);need(a,40,0);need(arg(4),4,1);MEM32(arg(4))=0;int32_t w=(int32_t)MEM32(a+4),h=(int32_t)MEM32(a+8);b=MEM16(a+14);uint32_t compression=MEM32(a+16);void_log("CreateDIBSection header=%u %dx%d planes=%u bits=%u compression=%u",MEM32(a),w,h,MEM16(a+12),b,compression);
  if(MEM32(a)!=40||w<=0||w>8192||!h||h< -8192||h>8192||MEM16(a+12)!=1){void_set_last_error(87);}
  else if(arg(3)||arg(5)||arg(6)||(b!=16&&b!=24&&b!=32)||(compression!=0&&compression!=3)||(compression==3&&b!=16)){void_log("unsupported DIB bits=%u compression=%u",b,compression);void_set_last_error(50);}
  else{unsigned i=0;while(i<128&&bitmaps[i].pixels)i++;if(i==128){void_set_last_error(8);goto done;}uint32_t height=(uint32_t)(h<0?-h:h),stride=(((uint32_t)w*b+31)/32)*4;uint32_t p=void_galloc(stride*height);if(!p){void_set_last_error(8);goto done;}uint32_t red=b==16?0x7c00:0xff0000,green=b==16?0x3e0:0xff00,blue=0x1f;if(b!=16)blue=0xff;
   if(compression==3){need(a+40,12,0);red=MEM32(a+40);green=MEM32(a+44);blue=MEM32(a+48);if(red!=0xf800||green!=0x7e0||blue!=0x1f){void_gfree(p);void_set_last_error(50);goto done;}}
   bitmaps[i]=(bitmap){p,(uint32_t)w,height,stride,b,red,green,blue,h<0};MEM32(arg(4))=p;value=STOCK_BITMAP+1+i;void_log("real guest DIB %dx%ux%u pitch=%u pixels=%08x",w,height,b,stride,p);}
 NEXT API("SelectObject",2) int i=dc_index(arg(1));if(i<0)void_set_last_error(6);else if(arg(2)!=STOCK_BITMAP&&!find_bitmap(arg(2)))void_set_last_error(6);else{value=dc_bitmap[i];dc_bitmap[i]=arg(2);}
 NEXT API("DeleteObject",1) bitmap *bm=find_bitmap(arg(1));if(!bm)void_set_last_error(6);else{int selected=0;for(unsigned i=0;i<128;i++)if(dc_used[i]&&dc_bitmap[i]==arg(1))selected=1;if(selected)void_set_last_error(87);else{void_gfree(bm->pixels);memset(bm,0,sizeof *bm);value=1;}}
 NEXT API("BitBlt",9) int dst=dc_index(arg(1)),src=dc_index(arg(6));int w=(int)arg(4),h=(int)arg(5);if(w<0||h<0||w>8192||h>8192||(!find_bitmap(src<0?0:dc_bitmap[src])&&arg(6)!=SCREEN_DC)||(dst<0&&arg(1)!=SCREEN_DC)){void_set_last_error(87);}
  else if(arg(9)!=0xcc0020){void_log("unsupported BitBlt ROP %08x",arg(9));void_set_last_error(50);}
  else{size_t count=(size_t)w*(unsigned)h;uint16_t *pixels=calloc(count?count:1,2);if(!pixels){void_set_last_error(8);goto done;}if(src<0){if(!h4_ddraw_primary_io(0,(int)arg(7),(int)arg(8),w,h,pixels,w)){free(pixels);void_set_last_error(50);goto done;}}
   else{bitmap *bm=find_bitmap(dc_bitmap[src]);for(int y=0;y<h;y++)for(int x=0;x<w;x++)pixels[(size_t)y*w+x]=get_pixel(bm,(int)arg(7)+x,(int)arg(8)+y);}
   if(dst<0)value=h4_ddraw_primary_io(1,(int)arg(2),(int)arg(3),w,h,pixels,w);else{bitmap *bm=find_bitmap(dc_bitmap[dst]);if(bm){for(int y=0;y<h;y++)for(int x=0;x<w;x++)put_pixel(bm,(int)arg(2)+x,(int)arg(3)+y,pixels[(size_t)y*w+x]);value=1;}else void_set_last_error(50);}free(pixels);}
 NEXT {return 0;}
#undef API
#undef NEXT
done:*result=value;*argc=count;return 1;
}
