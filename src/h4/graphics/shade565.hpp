#pragma once
#include <algorithm>
#include <array>
#include "h4/graphics/color565.hpp"
namespace h4::graphics {
struct ShadeTables {
    std::array<std::array<std::uint16_t,32>,33> red{},blue{};
    std::array<std::array<std::uint16_t,64>,33> green{};
};
extern const ShadeTables shade_tables;
[[nodiscard]] inline Color565 shade(Color565 pixel,unsigned index) noexcept {
    index=std::min(index,32U);
    return Color565{static_cast<std::uint16_t>(shade_tables.red[index][pixel.red()] |
        shade_tables.green[index][pixel.green()] | shade_tables.blue[index][pixel.blue()])};
}
[[nodiscard]] inline int shade_index(int biased,int parity_a,int parity_b) noexcept {
    int index=biased>>11;
    switch(biased>>9&3) {
    case 1:index+=parity_a&parity_b;break;
    case 2:index+=(parity_a+parity_b)&1;break;
    case 3:index+=parity_a|parity_b;break;
    default:break;
    }
    return index;
}
}
