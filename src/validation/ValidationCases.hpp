#pragma once

#include <cmath>
#include <string>
#include <vector>

#include "../physics/PhysicsEngine.hpp"

namespace bag {

struct ValidationCase {
    std::string id;
    std::vector<Body> bodies;
    double semiMajorAxis = 0.0;
    double eccentricity = 0.0;
    double analyticalPeriod = 0.0;
};

inline ValidationCase makeTwoBodyCase(const std::string& id, double semiMajorAxis, double eccentricity) {
    ValidationCase result;
    result.id = id;
    result.semiMajorAxis = semiMajorAxis;
    result.eccentricity = eccentricity;
    const double sunMass = PhysicsEngine::SOLAR_MASS;
    const double smallMass = PhysicsEngine::EARTH_MASS;
    const double mu = PhysicsEngine::G * (sunMass + smallMass);
    const double radius = semiMajorAxis * (1.0 - eccentricity);
    const double relativeSpeed = std::sqrt(mu * (1.0 + eccentricity) / radius);
    const double sunFraction = smallMass / (sunMass + smallMass);
    const double bodyFraction = sunMass / (sunMass + smallMass);
    Body sun;
    sun.id = "validation-sun";
    sun.mass = sunMass;
    sun.position = {-sunFraction * semiMajorAxis * (1.0 - eccentricity), 0.0, 0.0};
    sun.velocity = {0.0, -sunFraction * relativeSpeed, 0.0};
    Body body;
    body.id = "validation-body";
    body.mass = smallMass;
    body.position = {bodyFraction * radius, 0.0, 0.0};
    body.velocity = {0.0, bodyFraction * relativeSpeed, 0.0};
    result.bodies = {sun, body};
    result.analyticalPeriod = 2.0 * 3.14159265358979323846 *
        std::sqrt(semiMajorAxis * semiMajorAxis * semiMajorAxis / mu);
    return result;
}

inline ValidationCase circularOrbitCase() {
    return makeTwoBodyCase("circular-two-body", PhysicsEngine::AU, 0.0);
}

inline ValidationCase ellipticalOrbitCase() {
    return makeTwoBodyCase("elliptical-two-body", 1.4 * PhysicsEngine::AU, 0.35);
}

} // namespace bag
