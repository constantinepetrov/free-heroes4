#include "ddraw_surface.hpp"
#include <cstring>
#include <stdexcept>
#include "h4/platform/window.hpp"
namespace h4::recomp {
DDrawSurface::DDrawSurface(int width,int height,int bits):width_{width},height_{height},bits_{bits},pitch_{0} {
    if(width<1||height<1||width>8192||height>8192||(bits!=8&&bits!=16))throw std::invalid_argument("invalid DirectDraw surface");
    pitch_=(width*(bits/8)+3)&~3;pixels_.resize(static_cast<std::size_t>(pitch_)*height);
    for(unsigned n=0;n<256;n++)palette(n,n,n,n);
}
void DDrawSurface::available() const {if(locked_)throw std::logic_error("surface is locked");}
std::span<std::uint8_t> DDrawSurface::lock(){available();locked_=true;return pixels_;}
void DDrawSurface::unlock(){if(!locked_)throw std::logic_error("surface is not locked");locked_=false;}
void DDrawSurface::palette(unsigned index,unsigned red,unsigned green,unsigned blue){
    if(index>=256||red>255||green>255||blue>255)throw std::invalid_argument("invalid palette entry");
    palette_[index]=graphics::Color565::from_rgb(static_cast<std::uint8_t>(red),static_cast<std::uint8_t>(green),static_cast<std::uint8_t>(blue)).bits();
}
std::uint16_t DDrawSurface::pixel(int x,int y) const {
    const auto i=static_cast<std::size_t>(y)*pitch_+static_cast<std::size_t>(x)*(bits_/8);
    if(bits_==8)return pixels_[i];std::uint16_t p;std::memcpy(&p,pixels_.data()+i,2);return p;
}
void DDrawSurface::put(int x,int y,std::uint16_t value){
    const auto i=static_cast<std::size_t>(y)*pitch_+static_cast<std::size_t>(x)*(bits_/8);
    if(bits_==8)pixels_[i]=static_cast<std::uint8_t>(value);else std::memcpy(pixels_.data()+i,&value,2);
}
void DDrawSurface::fill(core::Rect rect,std::uint16_t color){
    available();if(bits_==8&&color>255)throw std::invalid_argument("palette index exceeds 255");
    rect=rect.intersected({{},size()});for(int y=rect.top();y<rect.bottom();y++)for(int x=rect.left();x<rect.right();x++)put(x,y,color);
}
void DDrawSurface::blt(core::Point at,const DDrawSurface& source,core::Rect rect,std::optional<std::uint16_t> key){
    available();source.available();if(bits_!=source.bits_)throw std::invalid_argument("pixel formats differ");
    if(this==&source){const DDrawSurface snapshot=source;blt(at,snapshot,rect,key);return;}
    const auto clipped=rect.intersected({{},source.size()});
    for(int y=clipped.top();y<clipped.bottom();y++)for(int x=clipped.left();x<clipped.right();x++){
        const auto to=at+core::Point{x-rect.left(),y-rect.top()};
        if(to.x<0||to.y<0||to.x>=width_||to.y>=height_)continue;
        const auto p=source.pixel(x,y);if(!key||p!=*key)put(to.x,to.y,p);
    }
}
void DDrawSurface::flip(DDrawSurface& back){
    available();back.available();if(size()!=back.size()||bits_!=back.bits_)throw std::invalid_argument("flip formats differ");pixels_.swap(back.pixels_);
}
graphics::Surface565 DDrawSurface::frame() const {
    available();graphics::Surface565 result{size()};
    for(int y=0;y<height_;y++)for(int x=0;x<width_;x++){const auto p=pixel(x,y);result.set({x,y},graphics::Color565{bits_==8?palette_[p]:p});}
    return result;
}
void DDrawSurface::present(platform::Window& window) const {const auto pixels=frame();window.set_logical_size(size());window.present(pixels);}
}
