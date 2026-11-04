/* GPL-3.0-or-later. Native presentation contains only pixels supplied by the guest. */
#include "surface_bridge.h"
#include "ddraw_surface.hpp"
#include <SDL3/SDL.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <stdexcept>
using h4::recomp::DDrawSurface;
struct Surface {DDrawSurface native;std::span<std::uint8_t> locked;Surface(int w,int h,int b):native(w,h,b){}};
static std::string error;
static SDL_Renderer *renderer;
static SDL_Texture *texture;
static SDL_Window *render_window;
static int tw,th;
static unsigned frames;
static bool captured;
#define TRY try
#define FAIL catch(const std::exception& e){error=e.what();return 0;}
extern "C" void *h4_surface_new(int w,int h,int bits){TRY{return new Surface(w,h,bits);}FAIL}
extern "C" void h4_surface_delete(void *p){delete static_cast<Surface*>(p);}
extern "C" int h4_surface_lock(void *p,void **pixels,size_t *bytes){TRY{auto& s=*static_cast<Surface*>(p);s.locked=s.native.lock();*pixels=s.locked.data();*bytes=s.locked.size();return 1;}FAIL}
extern "C" int h4_surface_unlock(void *p,const void *pixels,size_t bytes){TRY{auto& s=*static_cast<Surface*>(p);if(bytes!=s.locked.size())throw std::invalid_argument("lock buffer size");if(pixels)std::memcpy(s.locked.data(),pixels,bytes);s.native.unlock();s.locked={};return 1;}FAIL}
extern "C" int h4_surface_fill(void *p,int x,int y,int w,int h,unsigned color){TRY{static_cast<Surface*>(p)->native.fill({{x,y},{w,h}},static_cast<uint16_t>(color));return 1;}FAIL}
extern "C" int h4_surface_blt(void *p,int x,int y,void *src,int sx,int sy,int w,int h,int key){TRY{static_cast<Surface*>(p)->native.blt({x,y},static_cast<Surface*>(src)->native,{{sx,sy},{w,h}},key<0?std::nullopt:std::optional<uint16_t>(static_cast<uint16_t>(key)));return 1;}FAIL}
extern "C" int h4_surface_flip(void *p,void *b){TRY{static_cast<Surface*>(p)->native.flip(static_cast<Surface*>(b)->native);return 1;}FAIL}
extern "C" int h4_surface_palette(void *p,unsigned i,unsigned r,unsigned g,unsigned b){TRY{static_cast<Surface*>(p)->native.palette(i,r,g,b);return 1;}FAIL}
extern "C" void h4_surface_window_closed(void){if(texture)SDL_DestroyTexture(texture);if(renderer)SDL_DestroyRenderer(renderer);texture=nullptr;renderer=nullptr;render_window=nullptr;tw=th=0;}
extern "C" const char *h4_surface_error(void){return error.c_str();}
extern "C" int h4_surface_present(void *p,void *win){TRY{
 auto frame=static_cast<Surface*>(p)->native.frame();auto size=frame.size();auto *window=static_cast<SDL_Window*>(win);
 if(render_window!=window){h4_surface_window_closed();renderer=SDL_CreateRenderer(window,nullptr);if(!renderer)throw std::runtime_error(SDL_GetError());render_window=window;}
 if(!texture||tw!=size.width||th!=size.height){if(texture)SDL_DestroyTexture(texture);texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGB565,SDL_TEXTUREACCESS_STREAMING,size.width,size.height);if(!texture)throw std::runtime_error(SDL_GetError());tw=size.width;th=size.height;SDL_SetRenderLogicalPresentation(renderer,tw,th,SDL_LOGICAL_PRESENTATION_LETTERBOX);}
 if(!SDL_UpdateTexture(texture,nullptr,frame.bits().data(),tw*2)||!SDL_RenderClear(renderer)||!SDL_RenderTexture(renderer,texture,nullptr,nullptr)||!SDL_RenderPresent(renderer))throw std::runtime_error(SDL_GetError());
 ++frames;if(frames<=3)std::fprintf(stderr,"DirectDraw present %u: %dx%d, guest surface pixels\n",frames,tw,th);
 const char *capture=std::getenv("H4_FRAME_OUTPUT");if(capture&&!captured){unsigned nonzero=0;bool different=false;auto first=frame.bits()[0];for(auto pixel:frame.bits()){nonzero+=pixel!=0;different|=pixel!=first;}if(nonzero>256&&different){SDL_Surface *out=SDL_CreateSurfaceFrom(tw,th,SDL_PIXELFORMAT_RGB565,const_cast<uint16_t*>(frame.bits().data()),tw*2);if(!out||!SDL_SaveBMP(out,capture)){if(out)SDL_DestroySurface(out);throw std::runtime_error(SDL_GetError());}SDL_DestroySurface(out);captured=true;std::fprintf(stderr,"Captured nonuniform guest frame: %s (%u nonzero pixels)\n",capture,nonzero);}}
 return 1;
 }FAIL}
