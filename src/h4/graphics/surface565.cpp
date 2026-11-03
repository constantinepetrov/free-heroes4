#include "h4/graphics/surface565.hpp"

#include <algorithm>

#include "h4/formats/raw_bitmap.hpp"

namespace h4::graphics {

Surface565::Surface565(core::Size size, Color565 fill_color)
: size_{size.empty() ? core::Size{} : size},
  bits_(static_cast<std::size_t>(size_.width) * static_cast<std::size_t>(size_.height), fill_color.bits()) {
}

Surface565 Surface565::from_raw_bitmap(const formats::RawBitmap& bitmap) {
    Surface565 out{bitmap.size()};
    const auto rgb = bitmap.rgb();
    for (std::size_t i = 0; i < out.bits_.size(); ++i)
        out.bits_[i] = Color565::from_rgb(rgb[i * 3], rgb[i * 3 + 1], rgb[i * 3 + 2]).bits();
    return out;
}

void Surface565::fill(Color565 c) noexcept {
    std::ranges::fill(bits_, c.bits());
}

std::vector<std::uint8_t> Surface565::to_rgba() const {
    std::vector<std::uint8_t> out;
    out.reserve(bits_.size() * 4);
    for (const std::uint16_t b : bits_) {
        const Rgba c = Color565{b}.to_rgba();
        out.insert(out.end(), {c.r, c.g, c.b, c.a});
    }
    return out;
}

}
