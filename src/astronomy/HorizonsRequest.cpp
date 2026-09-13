#include "HorizonsRequest.hpp"

#include <map>

namespace bag {
namespace {
std::string encode(const std::string& value) {
    const char* hex = "0123456789ABCDEF";
    std::string result;
    for (const unsigned char character : value) {
        if ((character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') || (character >= '0' && character <= '9') || character == '-' || character == '_' || character == '.' || character == '~') result += static_cast<char>(character);
        else { result += '%'; result += hex[character >> 4]; result += hex[character & 0x0F]; }
    }
    return result;
}
}

bool horizonsBodyId(const std::string& bodyId, std::string& targetId) {
    static const std::map<std::string, std::string> ids = {
        {"sun", "10"}, {"mercury", "199"}, {"venus", "299"}, {"earth", "399"},
        {"mars", "499"}, {"jupiter", "599"}, {"saturn", "699"}, {"uranus", "799"}, {"neptune", "899"},
    };
    const auto found = ids.find(bodyId);
    if (found == ids.end()) return false;
    targetId = found->second;
    return true;
}

bool horizonsRequestFor(const EphemerisRequest& request, HorizonsRequest& result, std::string& error) {
    if (!request.epoch.valid() || !request.frame.valid() || request.bodyId.empty()) { error = "body, Julian date, and frame are required"; return false; }
    if (!horizonsBodyId(request.bodyId, result.targetId)) { error = "unsupported Horizons body '" + request.bodyId + "'"; return false; }
    if (request.frame.type == ReferenceFrame::Barycentric) result.center = "500@0";
    else if (request.frame.type == ReferenceFrame::Heliocentric) result.center = "500@10";
    else if (request.frame.type == ReferenceFrame::Geocentric) result.center = "500@399";
    else { error = "unsupported Horizons frame"; return false; }
    result.url = "https://ssd.jpl.nasa.gov/api/horizons.api?format=json&COMMAND=" + encode("'" + result.targetId + "'") +
        "&MAKE_EPHEM=YES&EPHEM_TYPE=VECTORS&OBJ_DATA=NO&CENTER=" + encode("'" + result.center + "'") +
        "&TLIST=" + encode("'" + std::to_string(request.epoch.value) + "'") + "&OUT_UNITS=KM-S&REF_PLANE=FRAME&VEC_TABLE=2";
    return true;
}

} // namespace bag
