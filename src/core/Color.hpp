#pragma once

#include <cstdint>

namespace bag {

struct ColorRGBA {
    std::uint8_t r = 255;
    std::uint8_t g = 255;
    std::uint8_t b = 255;
    std::uint8_t a = 255;
};

} // namespace bag
