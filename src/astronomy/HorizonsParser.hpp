#pragma once

#include <string>

#include "EphemerisTypes.hpp"

namespace bag {

EphemerisResult parseHorizonsResponse(const std::string& response,
                                      const EphemerisRequest& request,
                                      const std::string& source = "JPL Horizons");

} // namespace bag
