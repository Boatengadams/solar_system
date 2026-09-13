#pragma once

#include <string>
#include <vector>

#include "../core/Vector3.hpp"

namespace bag {

struct Spacecraft {
    std::string id;
    std::string name;
    double dryMassKg = 0.0;
    double propellantMassKg = 0.0;
    double maximumThrustN = 0.0;
    double specificImpulseS = 0.0;
    Vec3 positionM;
    Vec3 velocityMps;

    static constexpr double STANDARD_GRAVITY_MPS2 = 9.80665;

    bool valid() const;
    double totalMassKg() const;
    double availableDeltaVMps() const;
    double propellantForDeltaV(double deltaVMps) const;
};

struct BurnResult {
    bool success = false;
    double durationSeconds = 0.0;
    double deltaVMps = 0.0;
    double propellantConsumedKg = 0.0;
    std::string message;
};

BurnResult applyBurn(Spacecraft& spacecraft, Vec3 direction, double durationSeconds);
BurnResult applyImpulse(Spacecraft& spacecraft, Vec3 deltaVelocityMps);

} // namespace bag
