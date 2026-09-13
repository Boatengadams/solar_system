#pragma once

#include "EphemerisTypes.hpp"

namespace bag {

class EphemerisProvider {
public:
    virtual ~EphemerisProvider() = default;
    virtual EphemerisResult getState(const EphemerisRequest& request) = 0;
    virtual const char* name() const = 0;
};

} // namespace bag
