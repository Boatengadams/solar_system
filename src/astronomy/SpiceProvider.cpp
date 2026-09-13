#include "SpiceProvider.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <map>
#include <sstream>
#include <utility>

#include <nlohmann/json.hpp>

#ifndef BAGSOLAR_ENABLE_SPICE
#define BAGSOLAR_ENABLE_SPICE 0
#endif

#if BAGSOLAR_ENABLE_SPICE
#include <SpiceUsr.h>
#endif

namespace bag {
namespace {
using Json = nlohmann::json;
constexpr double J2000_JULIAN_DATE = 2451545.0;
constexpr double SECONDS_PER_DAY = 86400.0;

#if BAGSOLAR_ENABLE_SPICE
std::string joinPaths(const std::vector<std::filesystem::path>& paths) {
    std::ostringstream output;
    for (std::size_t i = 0; i < paths.size(); ++i) {
        if (i != 0) output << ";";
        output << paths[i].string();
    }
    return output.str();
}
#endif

#if BAGSOLAR_ENABLE_SPICE
std::string spiceErrorMessage() {
    char message[1841]{};
    getmsg_c("LONG", static_cast<SpiceInt>(sizeof(message)), message);
    reset_c();
    return message[0] == '\0' ? "CSPICE reported an unspecified error" : std::string(message);
}
#endif
}

const char* spiceKernelTypeName(SpiceKernelType type) {
    switch (type) {
    case SpiceKernelType::SPK: return "SPK";
    case SpiceKernelType::LSK: return "LSK";
    case SpiceKernelType::PCK: return "PCK";
    case SpiceKernelType::FK: return "FK";
    }
    return "UNKNOWN";
}

bool parseSpiceKernelType(const std::string& value, SpiceKernelType& type) {
    if (value == "SPK") type = SpiceKernelType::SPK;
    else if (value == "LSK") type = SpiceKernelType::LSK;
    else if (value == "PCK") type = SpiceKernelType::PCK;
    else if (value == "FK") type = SpiceKernelType::FK;
    else return false;
    return true;
}

bool SpiceKernelManifest::valid() const {
    if (entries.empty()) return false;
    bool anyEnabled = false;
    std::vector<std::filesystem::path> seen;
    for (const SpiceKernelEntry& entry : entries) {
        if (!entry.enabled) continue;
        anyEnabled = true;
        if (entry.path.empty() || !std::filesystem::is_regular_file(entry.path)) return false;
        if (std::find(seen.begin(), seen.end(), entry.path) != seen.end()) return false;
        seen.push_back(entry.path);
    }
    return anyEnabled;
}

std::string SpiceKernelManifest::error() const {
    if (entries.empty()) return "SPICE kernel manifest contains no enabled entries";
    bool anyEnabled = false;
    std::vector<std::filesystem::path> seen;
    for (const SpiceKernelEntry& entry : entries) {
        if (!entry.enabled) continue;
        anyEnabled = true;
        if (entry.path.empty()) return "SPICE kernel manifest contains an empty path";
        if (!std::filesystem::is_regular_file(entry.path)) return "SPICE kernel is unavailable: " + entry.path.string();
        if (std::find(seen.begin(), seen.end(), entry.path) != seen.end()) return "SPICE kernel is listed more than once: " + entry.path.string();
        seen.push_back(entry.path);
    }
    if (!anyEnabled) return "SPICE kernel manifest contains no enabled entries";
    return {};
}

SpiceManifestResult loadSpiceKernelManifest(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) return {false, {}, "cannot open SPICE kernel manifest: " + path.string()};
    Json document;
    try { input >> document; }
    catch (const std::exception& error) { return {false, {}, std::string("SPICE manifest JSON parse failed: ") + error.what()}; }
    try {
        if (!document.is_object() || document.value("schema_version", 0) != 1 || !document.contains("kernels") || !document.at("kernels").is_array()) return {false, {}, "SPICE manifest requires schema_version 1 and a kernels array"};
        SpiceKernelManifest manifest;
        const std::filesystem::path base = path.parent_path();
        for (const Json& item : document.at("kernels")) {
            if (!item.is_object() || !item.contains("path") || !item.contains("type")) return {false, {}, "SPICE manifest kernel requires path and type"};
            SpiceKernelType type;
            if (!parseSpiceKernelType(item.at("type").get<std::string>(), type)) return {false, {}, "unsupported SPICE kernel type"};
            std::filesystem::path kernelPath = item.at("path").get<std::string>();
            if (kernelPath.is_relative()) kernelPath = base / kernelPath;
            SpiceKernelEntry entry{kernelPath.lexically_normal(), type, item.value("load_order", 0), item.value("enabled", true)};
            manifest.entries.push_back(entry);
        }
        std::stable_sort(manifest.entries.begin(), manifest.entries.end(), [](const SpiceKernelEntry& left, const SpiceKernelEntry& right) {
            if (left.loadOrder != right.loadOrder) return left.loadOrder < right.loadOrder;
            return left.path.string() < right.path.string();
        });
        if (!manifest.valid()) return {false, {}, manifest.error()};
        return {true, manifest, {}};
    } catch (const std::exception& error) { return {false, {}, std::string("SPICE manifest field error: ") + error.what()}; }
}

SpiceLoadResult SpiceKernelManager::load(const SpiceKernelManifest& manifest) {
    unload();
    if (!manifest.valid()) return {false, EphemerisStatus::KERNEL_NOT_FOUND, manifest.error(), {}};
#if !BAGSOLAR_ENABLE_SPICE
    return {false, EphemerisStatus::PROVIDER_UNAVAILABLE, "CSPICE support is not enabled; configure with -DBAGSOLAR_ENABLE_SPICE=ON", {}};
#else
    for (const SpiceKernelEntry& entry : manifest.entries) {
        if (!entry.enabled) continue;
        furnsh_c(entry.path.string().c_str());
        if (failed_c()) {
            const std::string error = spiceErrorMessage();
            unload();
            return {false, EphemerisStatus::KERNEL_LOAD_FAILURE, "failed to load " + entry.path.string() + ": " + error, {}};
        }
        loadedKernels.push_back(entry.path);
    }
    return {true, EphemerisStatus::SUCCESS, {}, loadedKernels};
#endif
}

void SpiceKernelManager::unload() {
#if BAGSOLAR_ENABLE_SPICE
    for (auto iterator = loadedKernels.rbegin(); iterator != loadedKernels.rend(); ++iterator) unload_c(iterator->string().c_str());
#endif
    loadedKernels.clear();
}

SpiceEphemerisProvider::SpiceEphemerisProvider(SpiceKernelManifest manifest) : kernelManifest(std::move(manifest)) {}
SpiceEphemerisProvider::~SpiceEphemerisProvider() { unloadKernels(); }
bool SpiceEphemerisProvider::compiledIn() { return BAGSOLAR_ENABLE_SPICE != 0; }
SpiceLoadResult SpiceEphemerisProvider::loadKernels() { return manager.load(kernelManifest); }
void SpiceEphemerisProvider::unloadKernels() { manager.unload(); }

double julianDateToSpiceEt(const Epoch& epoch, bool* valid) {
    const bool isValid = epoch.valid();
    if (valid != nullptr) *valid = isValid;
    return isValid ? (epoch.value - J2000_JULIAN_DATE) * SECONDS_PER_DAY : 0.0;
}

bool spiceTargetId(const std::string& bodyId, std::string& targetId) {
    static const std::map<std::string, std::string> ids = {
        {"sun", "SUN"}, {"mercury", "MERCURY BARYCENTER"}, {"venus", "VENUS BARYCENTER"},
        {"earth", "EARTH"}, {"moon", "MOON"}, {"mars", "MARS BARYCENTER"},
        {"jupiter", "JUPITER BARYCENTER"}, {"saturn", "SATURN BARYCENTER"},
        {"uranus", "URANUS BARYCENTER"}, {"neptune", "NEPTUNE BARYCENTER"},
    };
    const auto found = ids.find(bodyId);
    if (found == ids.end()) return false;
    targetId = found->second;
    return true;
}

bool spiceOriginName(const Frame& frame, std::string& observer) {
    if (!frame.valid()) return false;
    if (frame.type == ReferenceFrame::Heliocentric) observer = "SUN";
    else if (frame.type == ReferenceFrame::Geocentric) observer = "EARTH";
    else if (frame.type == ReferenceFrame::Barycentric) observer = "SOLAR SYSTEM BARYCENTER";
    else return false;
    return true;
}

bool spiceFrameName(const Frame& frame, std::string& frameName) {
    if (!frame.valid()) return false;
    frameName = "J2000";
    return true;
}

EphemerisResult SpiceEphemerisProvider::getState(const EphemerisRequest& request) {
    if (request.bodyId.empty()) return {EphemerisStatus::INVALID_REQUEST, {}, "SPICE request body is empty"};
    if (!request.epoch.valid()) return {EphemerisStatus::INVALID_EPOCH, {}, "SPICE requires a finite Julian Date"};
    if (!request.frame.valid()) return {EphemerisStatus::INVALID_FRAME, {}, "SPICE request frame or origin is unsupported"};
    std::string target;
    std::string observer;
    std::string frame;
    if (!spiceTargetId(request.bodyId, target)) return {EphemerisStatus::BODY_NOT_FOUND, {}, "SPICE body is not mapped: " + request.bodyId};
    if (!spiceOriginName(request.frame, observer)) return {EphemerisStatus::INVALID_ORIGIN, {}, "SPICE origin is unsupported"};
    if (!spiceFrameName(request.frame, frame)) return {EphemerisStatus::INVALID_FRAME, {}, "SPICE frame is unsupported"};
    if (!manager.loaded()) return {EphemerisStatus::PROVIDER_UNAVAILABLE, {}, "SPICE kernels are not loaded"};
#if !BAGSOLAR_ENABLE_SPICE
    return {EphemerisStatus::PROVIDER_UNAVAILABLE, {}, "CSPICE support is not enabled; configure with -DBAGSOLAR_ENABLE_SPICE=ON"};
#else
    bool epochValid = false;
    const double et = julianDateToSpiceEt(request.epoch, &epochValid);
    if (!epochValid) return {EphemerisStatus::INVALID_EPOCH, {}, "SPICE epoch conversion failed"};
    SpiceDouble state[6]{};
    SpiceDouble lightTime = 0.0;
    if (target != observer) {
        spkezr_c(target.c_str(), et, frame.c_str(), "NONE", observer.c_str(), state, &lightTime);
        if (failed_c()) return {EphemerisStatus::SPICE_ERROR, {}, spiceErrorMessage()};
    }
    EphemerisState result;
    result.bodyId = request.bodyId;
    result.epoch = request.epoch;
    result.frame = request.frame;
    result.positionM = {state[0] * 1000.0, state[1] * 1000.0, state[2] * 1000.0};
    result.velocityMps = {state[3] * 1000.0, state[4] * 1000.0, state[5] * 1000.0};
    result.source = "CSPICE kernels: " + joinPaths(manager.loadedKernelsView());
    result.provider = "SpiceEphemerisProvider";
    result.units = "SI";
    if (!result.valid()) return {EphemerisStatus::UNIT_CONVERSION_ERROR, {}, "SPICE returned a non-finite state"};
    return {EphemerisStatus::SUCCESS, result, {}};
#endif
}

} // namespace bag
