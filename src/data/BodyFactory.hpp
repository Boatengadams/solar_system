#pragma once

#include <string>

#include "DataResult.hpp"
#include "../core/Color.hpp"
#include "../core/Vector3.hpp"
#include "../physics/Body.hpp"

namespace bag {

struct BodyDefinition {
    std::string id;
    std::string name;
    std::string type;
    double massKg = 0.0;
    double radiusM = 0.0;
    Vec3 initialPositionM;
    Vec3 initialVelocityMps;
    ColorRGBA color;
    ColorRGBA accent;
    double displayRadiusPx = 1.0;
    bool luminous = false;
    bool ringed = false;
    std::string parentId;
    double semiMajorAxisM = 0.0;
    double eccentricity = 0.0;
    double orbitalPeriodS = 0.0;
};

struct CustomBodyData {
    std::string id;
    std::string name;
    std::string type;
    double massKg = 0.0;
    double radiusM = 0.0;
    Vec3 positionM;
    Vec3 velocityMps;
};

class BodyFactory {
public:
    static DataResult<Body> create(const BodyDefinition& definition);
    static DataResult<Body> createCustom(const CustomBodyData& data);
};

} // namespace bag
