#pragma once

#include <string>
#include <vector>

#include "../core/Color.hpp"
#include "../core/Vector2.hpp"
#include "../core/Vector3.hpp"

namespace bag {

struct Body {
    std::string id;
    std::string name;
    std::string type;
    Vec3 position;
    Vec3 velocity;
    double mass = 0.0;
    double radius = 1.0;
    double realRadius = 1.0;
    double axialTilt = 0.0;
    double rotationPeriod = 86400.0;
    double semiMajorAxis = 0.0;
    double eccentricity = 0.0;
    double orbitalPeriod = 0.0;
    ColorRGBA color;
    ColorRGBA accent;
    bool luminous = false;
    bool ringed = false;
    bool active = true;
    bool userCreated = false;
    int parent = -1;
    std::string parentId;
    // Historical physical positions in astronomical units. Rendering applies
    // the same presentation transform as the current body position.
    std::vector<Vec3> trail;
};

} // namespace bag
