#include "h4/graphics/shade565.hpp"

#include <algorithm>
#include <array>
#include <cstdint>

namespace h4::graphics {

namespace {

struct BlendMasks {
    std::array<std::uint32_t, 4> quarter{};
    std::uint32_t sum_low  = 0;
    std::uint32_t keep_lsb = 0;
    std::uint32_t overflow = 0;
    std::array<std::uint32_t, 3> carry{};
    std::array<std::uint32_t, 3> clear{};
    std::array<std::uint32_t, 3> channel{};
};

constexpr BlendMasks make_masks() noexcept {
    constexpr std::uint32_t red = 0xF800, green = 0x07E0, blue = 0x001F;
    BlendMasks m;
    m.quarter[0]            = (2 * red & red) + (2 * green & green) + (2 * blue & blue);
    m.quarter[1]            = 2 * m.quarter[0] & m.quarter[0];
    m.quarter[2]            = 4 * m.quarter[0] & m.quarter[1];
    m.quarter[3]            = 8 * m.quarter[0] & m.quarter[2];
    const std::uint32_t lsb = (red & ~(2 * red)) | (green & ~(2 * green)) | (blue & ~(2 * blue));
    m.carry                 = {~blue & (2 * blue), ~green & (2 * green), ~red & (2 * red)};
    m.sum_low               = ~lsb;
    m.keep_lsb              = lsb;
    m.overflow              = m.carry[0] | m.carry[1] | m.carry[2];
    m.clear                 = {~m.carry[0], ~m.carry[1], ~m.carry[2]};
    m.channel               = {blue, green, red};
    return m;
}

constexpr BlendMasks kMasks = make_masks();

}

constexpr Color565 reference_shade(Color565 pixel, unsigned index) noexcept {
    const std::uint32_t p = pixel.bits();
    const auto term = [p](std::size_t quarter, unsigned shift) {
        return (kMasks.quarter[quarter] >> shift) & (p >> shift);
    };

    if (index <= 16) {
        if (index == 0)
            return Color565{};
        std::uint32_t out   = p;
        const unsigned bits = 16 - index;
        if ((bits & 1U) != 0)
            out -= term(3, 4);
        if ((bits & 2U) != 0)
            out -= term(2, 3);
        if ((bits & 4U) != 0)
            out -= term(1, 2);
        if ((bits & 8U) != 0)
            out -= term(0, 1);
        return Color565{static_cast<std::uint16_t>(out)};
    }

    index             = std::min(index, 32U);
    std::uint32_t sum = 0;
    if (index == 32) {
        sum = p;
    } else {
        const unsigned bits = index - 16;
        if ((bits & 1U) != 0)
            sum += term(3, 4);
        if ((bits & 2U) != 0)
            sum += term(2, 3);
        if ((bits & 4U) != 0)
            sum += term(1, 2);
        if ((bits & 8U) != 0)
            sum += term(0, 1);
    }

    std::uint32_t low = (p & kMasks.sum_low) + (sum & kMasks.sum_low);
    if ((low & kMasks.overflow) != 0)
        for (std::size_t ch = 0; ch < 3; ++ch)
            if ((low & kMasks.carry[ch]) != 0)
                low = (low & kMasks.clear[ch]) | kMasks.channel[ch];
    return Color565{static_cast<std::uint16_t>((p & kMasks.keep_lsb) | low)};
}


namespace {
constexpr ShadeTables make_tables() {
    ShadeTables t;
    for(unsigned s=0;s<=32;++s) {
        for(unsigned v=0;v<32;++v) {
            t.red[s][v]=reference_shade(Color565{static_cast<std::uint16_t>(v<<11)},s).bits();
            t.blue[s][v]=reference_shade(Color565{static_cast<std::uint16_t>(v)},s).bits();
        }
        for(unsigned v=0;v<64;++v)
            t.green[s][v]=reference_shade(Color565{static_cast<std::uint16_t>(v<<5)},s).bits();
    }
    return t;
}
}
constinit const ShadeTables shade_tables=make_tables();

}
