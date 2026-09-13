#pragma once

#include "Vector2.hpp"

namespace bag {

struct Star {
    Vec2 position;
    float radius = 1.0f;
    float brightness = 1.0f;
};

} // namespace bag
