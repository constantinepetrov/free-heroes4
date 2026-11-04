#include "ddraw_surface.hpp"
#include <iostream>
#include <stdexcept>
#include <string_view>
#ifdef H4_POC_SDL
#include "h4/platform/sdl3/sdl_window.hpp"
#endif
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main(int argc,char** argv){try{
 using namespace h4;recomp::DDrawSurface a{3,2,16},b{3,2,16};check(a.pitch()==8,"RGB565 pitch");
 a.fill({{},a.size()},0x1234);auto bytes=b.lock();bytes[0]=0x00;bytes[1]=0xf8;b.unlock();
 a.blt({1,0},b,{{0,0},{1,1}});check(a.frame().at({1,0}).bits()==0xf800,"Lock/Blt pixel");
 a.blt({0,1},a,{{0,0},{3,1}});check(a.frame().at({1,1}).bits()==0xf800,"Self blit snapshot");
 b.fill({{},b.size()},0x07e0);a.flip(b);check(a.frame().at({2,1}).bits()==0x07e0&&b.frame().at({1,0}).bits()==0xf800,"Flip exchanges backing");
 recomp::DDrawSurface clipped{3,2,16};clipped.blt({-1,0},b,{{0,0},{3,1}});
 check(clipped.frame().at({0,0}).bits()==0xf800&&clipped.frame().at({1,0}).bits()==0x1234&&clipped.frame().at({2,0}).bits()==0,"Destination clipping");
 clipped.fill({{},clipped.size()},0);clipped.blt({0,0},b,{{-1,0},{3,1}});
 check(clipped.frame().at({0,0}).bits()==0&&clipped.frame().at({1,0}).bits()==0x1234&&clipped.frame().at({2,0}).bits()==0xf800,"Source clipping offset");
 recomp::DDrawSurface pal{3,2,8};check(pal.pitch()==4,"Indexed pitch");pal.palette(1,255,0,0);pal.fill({{},pal.size()},1);
 check(pal.frame().at({2,1}).bits()==0xf800,"Palette to RGB565");
 recomp::DDrawSurface indexed{3,2,8};indexed.fill({{},indexed.size()},2);indexed.blt({-1,0},pal,{{0,0},{3,2}},1);
 check(indexed.frame().at({0,0}).bits()==graphics::Color565::from_rgb(2,2,2).bits(),"Clipped keyed blit");
 bool busy=false;(void)pal.lock();try{(void)pal.frame();}catch(const std::logic_error&){busy=true;}pal.unlock();check(busy,"Lock excludes presentation");
 bool bad=false;try{recomp::DDrawSurface invalid{-1,2,16};}catch(const std::invalid_argument&){bad=true;}check(bad,"Invalid dimensions");
#ifdef H4_POC_SDL
 if(argc==2&&std::string_view{argv[1]}=="--sdl"){
  auto opened=platform::sdl3::create_window("DirectDraw surface test",{3,2});if(!opened)throw std::runtime_error(opened.error().message());
  pal.present(**opened);a.present(**opened);std::cout<<"PASS SDL presentation\n";
 }
#else
 if(argc==2&&std::string_view{argv[1]}=="--sdl")throw std::runtime_error("SDL presentation was requested but not built");
#endif
 std::cout<<"PASS DirectDraw primitives: pitch, palette, lock, clipping, color key, overlap, flip\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
