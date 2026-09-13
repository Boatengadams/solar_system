#pragma once

#include "EphemerisTypes.hpp"

namespace bag {

struct EphemerisComparison {
    bool valid = false;
    bool providersDiffer = false;
    double positionDifferenceM = 0.0;
    double velocityDifferenceMps = 0.0;
    std::string error;
    std::string warning;
};

EphemerisComparison compareEphemerisStates(const EphemerisState& left, const EphemerisState& right);

} // namespace bag
