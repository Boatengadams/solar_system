#include "JsonEphemerisProvider.hpp"

#include <cmath>
#include <fstream>

#include <nlohmann/json.hpp>

namespace bag {
namespace {
using Json = nlohmann::json;

EphemerisResult invalid(EphemerisStatus status, const std::string& message) { return {status, {}, message}; }

bool readVector(const Json& value, Vec3& output) {
    if (!value.is_array() || value.size() != 3) return false;
    try {
        output = {value.at(0).get<double>(), value.at(1).get<double>(), value.at(2).get<double>()};
        return std::isfinite(output.x) && std::isfinite(output.y) && std::isfinite(output.z);
    } catch (...) { return false; }
}
}

JsonEphemerisProvider::JsonEphemerisProvider(std::filesystem::path file) : path(std::move(file)) {}

EphemerisResult JsonEphemerisProvider::getState(const EphemerisRequest& request) {
    if (request.bodyId.empty() || !request.epoch.valid()) return invalid(EphemerisStatus::INVALID_REQUEST, "body and a valid Julian Date are required");
    if (!request.frame.valid()) return invalid(EphemerisStatus::UNSUPPORTED_FRAME, "requested frame is unsupported");
    std::ifstream input(path);
    if (!input) return invalid(EphemerisStatus::PROVIDER_UNAVAILABLE, "cannot open ephemeris file '" + path.string() + "'");
    Json document;
    try { input >> document; }
    catch (const std::exception& error) { return invalid(EphemerisStatus::PARSE_ERROR, std::string("ephemeris JSON parse failed: ") + error.what()); }
    try {
        if (!document.is_object() || document.value("schema_version", 0) != 1) return invalid(EphemerisStatus::PARSE_ERROR, "unsupported or missing ephemeris schema_version");
        if (!document.contains("body_id") || !document.contains("epoch") || !document.contains("frame") || !document.contains("units") || !document.contains("position") || !document.contains("velocity")) return invalid(EphemerisStatus::PARSE_ERROR, "ephemeris JSON is missing a required field");
        const std::string bodyId = document.at("body_id").get<std::string>();
        if (bodyId != request.bodyId) return invalid(EphemerisStatus::BODY_NOT_FOUND, "ephemeris file contains a different body");
        const Json& epoch = document.at("epoch");
        if (epoch.value("type", "") != "julian_date" || !epoch.contains("value")) return invalid(EphemerisStatus::UNSUPPORTED_EPOCH, "only Julian Date epochs are supported");
        const double epochValue = epoch.at("value").get<double>();
        if (!std::isfinite(epochValue)) return invalid(EphemerisStatus::INVALID_DATA, "ephemeris epoch is not finite");
        if (std::abs(epochValue - request.epoch.value) > 1e-12) return invalid(EphemerisStatus::EPOCH_MISMATCH, "ephemeris file contains a different epoch");
        Frame frame;
        if (!parseReferenceFrame(document.at("frame").value("name", ""), frame)) return invalid(EphemerisStatus::UNSUPPORTED_FRAME, "ephemeris frame is unsupported");
        if (document.at("frame").value("origin_body_id", "") != frame.originBodyId) return invalid(EphemerisStatus::INVALID_DATA, "ephemeris frame origin does not match its name");
        if (document.at("frame").value("orientation", "J2000") != frame.orientation) return invalid(EphemerisStatus::UNSUPPORTED_FRAME, "ephemeris orientation is unsupported");
        if (frame.type != request.frame.type || frame.originBodyId != request.frame.originBodyId || frame.orientation != request.frame.orientation) return invalid(EphemerisStatus::UNSUPPORTED_FRAME, "ephemeris file contains a different frame");
        const Json& units = document.at("units");
        const std::string positionUnits = units.value("position", "");
        const std::string velocityUnits = units.value("velocity", "");
        double positionScale = 1.0;
        double velocityScale = 1.0;
        if (positionUnits == "km" && velocityUnits == "km/s") { positionScale = 1000.0; velocityScale = 1000.0; }
        else if (positionUnits != "m" || velocityUnits != "m/s") return invalid(EphemerisStatus::INVALID_DATA, "ephemeris units must be m/m/s or km/km/s");
        Vec3 position;
        Vec3 velocity;
        if (!readVector(document.at("position"), position) || !readVector(document.at("velocity"), velocity)) return invalid(EphemerisStatus::INVALID_DATA, "ephemeris vectors are invalid");
        EphemerisState state{bodyId, Epoch::julianDate(epochValue), frame, position * positionScale, velocity * velocityScale,
                             document.value("source", "JSON ephemeris fixture"), "JsonEphemerisProvider", "SI", EphemerisStatus::SUCCESS, {}};
        return {EphemerisStatus::SUCCESS, state, {}};
    } catch (const std::exception& error) { return invalid(EphemerisStatus::PARSE_ERROR, std::string("ephemeris JSON field error: ") + error.what()); }
}

} // namespace bag
