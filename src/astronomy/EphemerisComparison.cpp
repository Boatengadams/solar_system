#include "EphemerisComparison.hpp"

#include <cmath>

namespace bag {

EphemerisComparison compareEphemerisStates(const EphemerisState& left, const EphemerisState& right) {
    if (!left.valid() || !right.valid()) return {false, false, 0.0, 0.0, "cannot compare invalid ephemeris states", {}};
    if (left.bodyId != right.bodyId) return {false, false, 0.0, 0.0, "ephemeris bodies differ", {}};
    if (left.epoch.type != right.epoch.type || std::abs(left.epoch.value - right.epoch.value) > 1e-12) return {false, false, 0.0, 0.0, "ephemeris epochs differ", {}};
    if (left.frame.type != right.frame.type || left.frame.originBodyId != right.frame.originBodyId || left.frame.orientation != right.frame.orientation) return {false, false, 0.0, 0.0, "ephemeris frames, orientations, or origins differ", {}};
    if (left.units != right.units || left.units != "SI") return {false, false, 0.0, 0.0, "ephemeris units are incompatible", {}};
    const bool providersDiffer = left.provider != right.provider;
    const std::string warning = providersDiffer ? "ephemeris states came from different providers; comparison is valid only because epoch, orientation, origin, units, and body match" : std::string{};
    return {true, providersDiffer, length(left.positionM - right.positionM), length(left.velocityMps - right.velocityMps), {}, warning};
}

} // namespace bag
