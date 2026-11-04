#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>
#include "h4/core/geometry.hpp"
#include "h4/graphics/surface565.hpp"
namespace h4::platform { class Window; }
namespace h4::recomp {
// DirectDraw surface primitives only. COM vtables, clippers and overlays are not implemented.
class DDrawSurface {
public:
    DDrawSurface(int width,int height,int bits);
    [[nodiscard]] int pitch() const noexcept {return pitch_;}
    [[nodiscard]] core::Size size() const noexcept {return {width_,height_};}
    [[nodiscard]] std::span<std::uint8_t> lock();
    void unlock();
    void palette(unsigned index,unsigned red,unsigned green,unsigned blue);
    void fill(core::Rect rect,std::uint16_t color);
    void blt(core::Point at,const DDrawSurface& source,core::Rect rect,std::optional<std::uint16_t> key={});
    void flip(DDrawSurface& back);
    [[nodiscard]] graphics::Surface565 frame() const;
    void present(platform::Window& window) const;
private:
    int width_,height_,bits_,pitch_;
    bool locked_=false;
    std::vector<std::uint8_t> pixels_;
    std::array<std::uint16_t,256> palette_{};
    [[nodiscard]] std::uint16_t pixel(int x,int y) const;
    void put(int x,int y,std::uint16_t value);
    void available() const;
};
}
