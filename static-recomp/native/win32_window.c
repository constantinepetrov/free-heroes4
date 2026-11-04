/* GPL-3.0-or-later. A single real SDL window, with x86 Win32 message callbacks. */
#include "h4_host.h"
#include "surface_bridge.h"
#include <SDL3/SDL.h>
#include <strings.h>
#define arg h4_arg
#define need h4_need
#define str h4_string
#define HWND 0x50000001u
static SDL_Window *window;
static uint32_t wndproc,active,capture,user_data,style,ex_style;
static struct {char *name;uint32_t proc,style,extra;} classes[32];
static unsigned class_count;
static struct {uint32_t root,size;} accelerators[32];
static unsigned accel_count;
static struct {uint32_t hwnd,msg,wp,lp,time,x,y;} messages[256];
static unsigned message_count;
static SDL_Cursor *cursor;
static int video(void) {if(SDL_WasInit(SDL_INIT_VIDEO))return 1;if(SDL_InitSubSystem(SDL_INIT_VIDEO))return 1;void_log("SDL video: %s",SDL_GetError());void_set_last_error(50);return 0;}
SDL_Window *h4_sdl_window(void) {return window;}
static void post(uint32_t hwnd,uint32_t msg,uint32_t wp,uint32_t lp) {
 if(message_count==256)void_fatal("Win32 message queue exhausted");
 messages[message_count++]=(typeof(messages[0])){hwnd,msg,wp,lp,(uint32_t)SDL_GetTicks(),0,0};
}
/* Adapted from H5 void_call_guest; every callback must balance its stdcall stack. */
uint32_t h4_call_guest(uint32_t va,unsigned count,const uint32_t *args) {
 recomp_func_t fn=void_resolve(va);uint32_t sp=g_esp,caller=g_cur_func,ebx=g_ebx,esi=g_esi,edi=g_edi,ebp=g_ebp;
 for(unsigned i=count;i;i--)PUSH32(g_esp,args[i-1]);PUSH32(g_esp,RECOMP_RETADDR);fn();
 if(g_esp!=sp)void_fatal("guest callback %08x ESP=%08x expected=%08x",va,g_esp,sp);
 g_ebx=ebx;g_esi=esi;g_edi=edi;g_ebp=ebp;g_cur_func=caller;return g_eax;
}
static uint32_t send(uint32_t msg,uint32_t wp,uint32_t lp) {uint32_t args[]={HWND,msg,wp,lp};return h4_call_guest(wndproc,4,args);}
static void pump(void) {
 SDL_Event e;while(SDL_PollEvent(&e)){
  switch(e.type){
  case SDL_EVENT_QUIT:post(HWND,0x10,0,0);break;
  case SDL_EVENT_WINDOW_EXPOSED:post(HWND,0xf,0,0);break;
  case SDL_EVENT_WINDOW_FOCUS_GAINED:post(HWND,6,1,0);post(HWND,7,0,0);break;
  case SDL_EVENT_WINDOW_FOCUS_LOST:post(HWND,6,0,0);post(HWND,8,0,0);break;
  case SDL_EVENT_MOUSE_MOTION:post(HWND,0x200,e.motion.state,((uint32_t)(int)e.motion.x&0xffff)|((uint32_t)(int)e.motion.y<<16));break;
  case SDL_EVENT_MOUSE_BUTTON_DOWN:case SDL_EVENT_MOUSE_BUTTON_UP:{unsigned down=e.type==SDL_EVENT_MOUSE_BUTTON_DOWN;uint32_t msg=e.button.button==SDL_BUTTON_RIGHT?(down?0x204:0x205):(down?0x201:0x202);post(HWND,msg,0,((uint32_t)(int)e.button.x&0xffff)|((uint32_t)(int)e.button.y<<16));break;}
  case SDL_EVENT_KEY_DOWN:case SDL_EVENT_KEY_UP:{uint32_t key=e.key.key;if(key>='a'&&key<='z')key-=32;if(key<128)post(HWND,e.type==SDL_EVENT_KEY_DOWN?0x100:0x101,key,0);break;}
  default:break;
  }
 }
}
int h4_window_api(const char *name,uint32_t *result,unsigned *argc) {
 uint32_t value=0,a,b;unsigned count=0;
#define API(n,c) if(!strcmp(name,n)){count=c;
#define NEXT } else
 API("LoadIconA",2) void_log("optional LoadIconA unsupported, returning NULL/ERROR_NOT_SUPPORTED");void_set_last_error(50);
 NEXT API("LoadAcceleratorsA",2) if(arg(1)!=H4_IMAGE_BASE||arg(2)>65535||accel_count==32){void_set_last_error(50);}else{uint32_t size=0,root=h4_pe_resource(9,(int)arg(2),&size);if(!root||size%8){void_set_last_error(1813);}else{accelerators[accel_count]=(typeof(accelerators[0])){root,size};value=0x50000200u+accel_count++;}}
 NEXT API("TranslateAcceleratorA",3) a=arg(2)-0x50000200u;need(arg(3),28,0);if(arg(1)==HWND&&a<accel_count){uint32_t msg=MEM32(arg(3)+4),key=MEM32(arg(3)+8);SDL_Keymod mods=SDL_GetModState();if(msg==0x100||msg==0x104){for(uint32_t off=0;off<accelerators[a].size;off+=8){uint32_t p=accelerators[a].root+off,f=MEM16(p);if(!(f&1))continue;if(MEM16(p+2)!=key||!!(f&4)!=!!(mods&SDL_KMOD_SHIFT)||!!(f&8)!=!!(mods&SDL_KMOD_CTRL)||!!(f&16)!=!!(mods&SDL_KMOD_ALT))continue;send(0x111,0x10000u|MEM16(p+4),0);value=1;break;}}}
 NEXT API("GetSystemMetrics",1) if(video()){const SDL_DisplayMode *m=SDL_GetCurrentDisplayMode(SDL_GetPrimaryDisplay());a=arg(1);if(a==0||a==16)value=m?(uint32_t)m->w:800;else if(a==1||a==17)value=m?(uint32_t)m->h:600;else if(a==11||a==12||a==13||a==14)value=32;else{void_log("unsupported GetSystemMetrics index %u",a);void_set_last_error(50);}}
 NEXT API("AdjustWindowRectEx",4) need(arg(1),16,1);if(arg(3)||(arg(2)&0x00c00000)){void_log("decorated AdjustWindowRectEx unsupported");void_set_last_error(50);}else value=1;
 NEXT API("LoadCursorA",2) if(arg(1)||arg(2)!=32512){void_log("unsupported cursor resource %08x/%08x",arg(1),arg(2));void_set_last_error(50);}else if(video()){if(!cursor)cursor=SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_DEFAULT);if(cursor){SDL_SetCursor(cursor);value=0x50000100;}else{void_log("SDL cursor: %s",SDL_GetError());void_set_last_error(50);}}
 NEXT API("RegisterClassExA",1) a=arg(1);need(a,48,0);if(MEM32(a)!=48||class_count==32||MEM32(a+12)||MEM32(a+16)>64){void_set_last_error(87);}else{const char *s=str(MEM32(a+40));for(unsigned i=0;i<class_count;i++)if(!strcasecmp(s,classes[i].name)){void_set_last_error(1410);goto done;}classes[class_count]=(typeof(classes[0])){strdup(s),MEM32(a+8),MEM32(a+4),MEM32(a+16)};value=++class_count;}
 NEXT API("CreateWindowExA",12) if(window){void_set_last_error(50);}else{a=arg(2);unsigned index=32;if(a<65536){if(a&&a<=class_count)index=a-1;}else{const char *s=str(a);for(unsigned i=0;i<class_count;i++)if(!strcasecmp(s,classes[i].name))index=i;}if(index==32){void_set_last_error(1407);goto done;}int w=(int)arg(7),h=(int)arg(8);if(w<=0)w=800;if(h<=0)h=600;if(w>8192||h>8192){void_set_last_error(87);goto done;}if(!video())goto done;const char *title=arg(3)?str(arg(3)):"Heroes IV";window=SDL_CreateWindow(title,w,h,SDL_WINDOW_HIDDEN);if(!window){void_log("SDL_CreateWindow: %s",SDL_GetError());void_set_last_error(8);goto done;}wndproc=classes[index].proc;style=arg(4);ex_style=arg(1);uint32_t cs=void_galloc(48);if(!cs)void_fatal("CREATESTRUCT allocation failed");uint32_t fields[]={arg(12),arg(11),arg(10),arg(9),h,w,arg(6),arg(5),style,arg(3),arg(2),ex_style};memcpy(GPTR(cs),fields,48);if(!send(0x81,0,cs)||send(1,0,cs)==0xffffffff){h4_surface_window_closed();SDL_DestroyWindow(window);window=NULL;void_gfree(cs);goto done;}void_gfree(cs);if(style&0x10000000){SDL_ShowWindow(window);active=HWND;post(HWND,6,1,0);post(HWND,7,0,0);}value=HWND;void_log("real SDL window created %dx%d, original WndProc=%08x; no game frame yet",w,h,wndproc);}
 NEXT API("DefWindowProcA",4) a=arg(2);if(a==0x81)value=1;else if(a==0x10){if(window){h4_surface_window_closed();SDL_DestroyWindow(window);window=NULL;active=0;}post(0,0x12,0,0);}
 NEXT API("SetWindowLongA",3) if(arg(1)!=HWND){void_set_last_error(1400);}else{a=arg(2);if(a==0xfffffffc){value=wndproc;wndproc=arg(3);}else if(a==0xffffffeb){value=user_data;user_data=arg(3);}else if(a==0xfffffff0){value=style;style=arg(3);}else if(a==0xffffffec){value=ex_style;ex_style=arg(3);}else void_fatal("unsupported SetWindowLongA index %08x",a);}
 NEXT API("GetClientRect",2) if(!window||arg(1)!=HWND){void_set_last_error(1400);}else{need(arg(2),16,1);int w,h;SDL_GetWindowSize(window,&w,&h);MEM32(arg(2))=MEM32(arg(2)+4)=0;MEM32(arg(2)+8)=w;MEM32(arg(2)+12)=h;value=1;}
 NEXT API("GetWindowRect",2) if(!window||arg(1)!=HWND){void_set_last_error(1400);}else{need(arg(2),16,1);int x,y,w,h;SDL_GetWindowPosition(window,&x,&y);SDL_GetWindowSize(window,&w,&h);MEM32(arg(2))=x;MEM32(arg(2)+4)=y;MEM32(arg(2)+8)=x+w;MEM32(arg(2)+12)=y+h;value=1;}
 NEXT API("ShowWindow",2) if(window&&arg(1)==HWND){value=!!(SDL_GetWindowFlags(window)&SDL_WINDOW_HIDDEN)?0:1;if(!arg(2))SDL_HideWindow(window);else{SDL_ShowWindow(window);active=HWND;post(HWND,6,1,0);post(HWND,7,0,0);post(HWND,0xf,0,0);}}else void_set_last_error(1400);
 NEXT API("UpdateWindow",1) if(window&&arg(1)==HWND){send(0xf,0,0);value=1;}else void_set_last_error(1400);
 NEXT API("MoveWindow",6) if(window&&arg(1)==HWND&&arg(4)&&arg(5)&&arg(4)<=8192&&arg(5)<=8192){value=SDL_SetWindowPosition(window,(int)arg(2),(int)arg(3))&&SDL_SetWindowSize(window,(int)arg(4),(int)arg(5));if(value){post(HWND,5,0,(arg(4)&0xffff)|(arg(5)<<16));if(arg(6))post(HWND,0xf,0,0);}}else void_set_last_error(87);
 NEXT API("GetActiveWindow",0) value=active;
 NEXT API("SetActiveWindow",1) value=active;if(arg(1)!=0&&arg(1)!=HWND)void_set_last_error(1400);else{active=arg(1);if(active&&window)SDL_RaiseWindow(window);}
 NEXT API("PostQuitMessage",1) post(0,0x12,arg(1),0);
 NEXT API("PeekMessageA",5) pump();need(arg(1),28,1);for(unsigned i=0;i<message_count;i++){typeof(messages[0]) m=messages[i];if(arg(2)&&m.hwnd!=arg(2)&&m.msg!=0x12)continue;if((arg(3)||arg(4))&&m.msg!=0x12&&(m.msg<arg(3)||m.msg>arg(4)))continue;memcpy(GPTR(arg(1)),&m,28);if(arg(5)&1){memmove(messages+i,messages+i+1,(message_count-i-1)*sizeof m);--message_count;}value=1;break;}
 NEXT API("DispatchMessageA",1) need(arg(1),28,0);if(MEM32(arg(1))==HWND&&window)value=send(MEM32(arg(1)+4),MEM32(arg(1)+8),MEM32(arg(1)+12));
 NEXT API("TranslateMessage",1) need(arg(1),28,0);a=MEM32(arg(1)+4);if(a==0x100||a==0x101){b=MEM32(arg(1)+8);if(a==0x100&&b>=32&&b<127)post(HWND,0x102,b,0);value=1;}
 NEXT API("DestroyWindow",1) if(window&&arg(1)==HWND){send(2,0,0);send(0x82,0,0);h4_surface_window_closed();SDL_DestroyWindow(window);window=NULL;active=0;value=1;}else void_set_last_error(1400);
 NEXT {return 0;}
#undef API
#undef NEXT
done:*result=value;*argc=count;return 1;
}
