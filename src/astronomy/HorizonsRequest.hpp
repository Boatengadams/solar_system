#pragma once

#include <string>

#include "EphemerisTypes.hpp"

namespace bag {

struct HorizonsRequest {
    std::string url;
    std::string targetId;
    std::string center;
    std::string units = "KM-S";
};

bool horizonsBodyId(const std::string& bodyId, std::string& targetId);
bool horizonsRequestFor(const EphemerisRequest& request, HorizonsRequest& result, std::string& error);

} // namespace bag
