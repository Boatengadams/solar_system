#include "HorizonsParser.hpp"

#include <cmath>
#include <regex>

#include <nlohmann/json.hpp>

namespace bag {
namespace {
using Json = nlohmann::json;
bool number(const std::smatch& match, std::size_t index, double& value) {
    try { value = std::stod(match[index].str()); return std::isfinite(value); }
    catch (...) { return false; }
}
}

EphemerisResult parseHorizonsResponse(const std::string& response, const EphemerisRequest& request, const std::string& source) {
    if (!request.epoch.valid() || !request.frame.valid()) return {EphemerisStatus::INVALID_REQUEST, {}, "invalid ephemeris request"};
    Json document;
    try { document = Json::parse(response); }
    catch (const std::exception& error) { return {EphemerisStatus::PARSE_ERROR, {}, std::string("Horizons JSON parse failed: ") + error.what()}; }
    if (!document.is_object() || !document.contains("result") || !document["result"].is_string()) {
        return {EphemerisStatus::REMOTE_ERROR, {}, "Horizons response has no result payload"};
    }
    if (document.contains("error")) return {EphemerisStatus::REMOTE_ERROR, {}, document["error"].get<std::string>()};
    const std::string result = document["result"].get<std::string>();
    const std::size_t start = result.find("$$SOE");
    const std::size_t end = result.find("$$EOE", start == std::string::npos ? 0 : start);
    if (start == std::string::npos || end == std::string::npos || end <= start) return {EphemerisStatus::PARSE_ERROR, {}, "Horizons vectors delimiters are missing"};
    const std::string block = result.substr(start, end - start);
    std::smatch epochMatch;
    const std::regex epochPattern(R"(([-+]?\d+(?:\.\d+)?(?:[Ee][-+]?\d+)?)\s*=\s*A\.D\.)");
    if (!std::regex_search(block, epochMatch, epochPattern)) return {EphemerisStatus::PARSE_ERROR, {}, "Horizons epoch is missing"};
    double epochValue = 0.0;
    if (!number(epochMatch, 1, epochValue)) return {EphemerisStatus::INVALID_DATA, {}, "Horizons epoch is not finite"};
    const std::regex vectorPattern(R"(X\s*=\s*([-+]?\d+(?:\.\d+)?(?:[Ee][-+]?\d+)?)\s+Y\s*=\s*([-+]?\d+(?:\.\d+)?(?:[Ee][-+]?\d+)?)\s+Z\s*=\s*([-+]?\d+(?:\.\d+)?(?:[Ee][-+]?\d+)?)[^\n]*\n\s*VX\s*=\s*([-+]?\d+(?:\.\d+)?(?:[Ee][-+]?\d+)?)\s+VY\s*=\s*([-+]?\d+(?:\.\d+)?(?:[Ee][-+]?\d+)?)\s+VZ\s*=\s*([-+]?\d+(?:\.\d+)?(?:[Ee][-+]?\d+)?))");
    std::smatch match;
    if (!std::regex_search(block, match, vectorPattern)) return {EphemerisStatus::PARSE_ERROR, {}, "Horizons Cartesian state is missing"};
    double values[6]{};
    for (std::size_t i = 0; i < 6; ++i) if (!number(match, i + 1, values[i])) return {EphemerisStatus::INVALID_DATA, {}, "Horizons state contains a non-finite value"};
    if (std::abs(epochValue - request.epoch.value) > 1e-8) return {EphemerisStatus::EPOCH_MISMATCH, {}, "Horizons returned a different epoch"};
    EphemerisState state;
    state.bodyId = request.bodyId;
    state.epoch = Epoch::julianDate(epochValue);
    state.frame = request.frame;
    state.positionM = {values[0] * 1000.0, values[1] * 1000.0, values[2] * 1000.0};
    state.velocityMps = {values[3] * 1000.0, values[4] * 1000.0, values[5] * 1000.0};
    state.source = source;
    state.provider = "HorizonsProvider";
    state.units = "SI";
    if (!state.valid()) return {EphemerisStatus::INVALID_DATA, {}, "Horizons state is invalid"};
    return {EphemerisStatus::SUCCESS, state, {}};
}

} // namespace bag
