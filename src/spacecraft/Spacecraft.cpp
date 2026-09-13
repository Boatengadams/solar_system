#include "Spacecraft.hpp"

#include <cmath>
#include <limits>

namespace bag {

bool Spacecraft::valid() const {
    return std::isfinite(dryMassKg) && std::isfinite(propellantMassKg) && std::isfinite(maximumThrustN) &&
           std::isfinite(specificImpulseS) && dryMassKg > 0.0 && propellantMassKg >= 0.0 && maximumThrustN >= 0.0 &&
           specificImpulseS > 0.0 && std::isfinite(positionM.x) && std::isfinite(positionM.y) && std::isfinite(positionM.z) &&
           std::isfinite(velocityMps.x) && std::isfinite(velocityMps.y) && std::isfinite(velocityMps.z);
}

double Spacecraft::totalMassKg() const { return dryMassKg + propellantMassKg; }

double Spacecraft::availableDeltaVMps() const {
    if (!valid() || propellantMassKg <= 0.0) return 0.0;
    return specificImpulseS * STANDARD_GRAVITY_MPS2 * std::log(totalMassKg() / dryMassKg);
}

double Spacecraft::propellantForDeltaV(double deltaVMps) const {
    if (!valid() || deltaVMps < 0.0 || !std::isfinite(deltaVMps)) return std::numeric_limits<double>::quiet_NaN();
    const double finalMass = totalMassKg() / std::exp(deltaVMps / (specificImpulseS * STANDARD_GRAVITY_MPS2));
    return totalMassKg() - finalMass;
}

BurnResult applyBurn(Spacecraft& spacecraft, Vec3 direction, double durationSeconds) {
    if (!spacecraft.valid() || durationSeconds <= 0.0 || !std::isfinite(durationSeconds)) return {false, 0.0, 0.0, 0.0, "invalid spacecraft or burn duration"};
    const double directionLength = length(direction);
    if (directionLength <= 0.0 || !std::isfinite(directionLength)) return {false, 0.0, 0.0, 0.0, "burn direction must be non-zero"};
    if (spacecraft.maximumThrustN <= 0.0) return {false, 0.0, 0.0, 0.0, "spacecraft has no engine thrust"};
    const double massFlowKgPerSecond = spacecraft.maximumThrustN / (spacecraft.specificImpulseS * Spacecraft::STANDARD_GRAVITY_MPS2);
    const double consumed = massFlowKgPerSecond * durationSeconds;
    if (!std::isfinite(consumed) || consumed > spacecraft.propellantMassKg) return {false, 0.0, 0.0, 0.0, "burn requires more propellant than available"};
    const double initialMass = spacecraft.totalMassKg();
    spacecraft.propellantMassKg -= consumed;
    const double deltaV = spacecraft.specificImpulseS * Spacecraft::STANDARD_GRAVITY_MPS2 * std::log(initialMass / spacecraft.totalMassKg());
    spacecraft.velocityMps = spacecraft.velocityMps + direction * (deltaV / directionLength);
    return {true, durationSeconds, deltaV, consumed, {}};
}

BurnResult applyImpulse(Spacecraft& spacecraft, Vec3 deltaVelocityMps) {
    const double deltaV = length(deltaVelocityMps);
    if (!spacecraft.valid() || !std::isfinite(deltaV)) return {false, 0.0, 0.0, 0.0, "invalid spacecraft or impulse"};
    const double consumed = spacecraft.propellantForDeltaV(deltaV);
    if (!std::isfinite(consumed) || consumed > spacecraft.propellantMassKg) return {false, 0.0, 0.0, 0.0, "impulse requires more propellant than available"};
    spacecraft.propellantMassKg -= consumed;
    spacecraft.velocityMps = spacecraft.velocityMps + deltaVelocityMps;
    return {true, 0.0, deltaV, consumed, {}};
}

} // namespace bag
