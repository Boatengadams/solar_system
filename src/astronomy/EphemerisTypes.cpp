#include "EphemerisTypes.hpp"

#include <algorithm>

namespace bag {

const char* ephemerisStatusName(EphemerisStatus status) {
    switch (status) {
    case EphemerisStatus::SUCCESS: return "SUCCESS";
    case EphemerisStatus::INVALID_REQUEST: return "INVALID_REQUEST";
    case EphemerisStatus::BODY_NOT_FOUND: return "BODY_NOT_FOUND";
    case EphemerisStatus::UNSUPPORTED_FRAME: return "UNSUPPORTED_FRAME";
    case EphemerisStatus::UNSUPPORTED_EPOCH: return "UNSUPPORTED_EPOCH";
    case EphemerisStatus::NETWORK_ERROR: return "NETWORK_ERROR";
    case EphemerisStatus::TIMEOUT: return "TIMEOUT";
    case EphemerisStatus::REMOTE_ERROR: return "REMOTE_ERROR";
    case EphemerisStatus::PARSE_ERROR: return "PARSE_ERROR";
    case EphemerisStatus::INVALID_DATA: return "INVALID_DATA";
    case EphemerisStatus::CACHE_MISS: return "CACHE_MISS";
    case EphemerisStatus::PROVIDER_UNAVAILABLE: return "PROVIDER_UNAVAILABLE";
    case EphemerisStatus::EPOCH_MISMATCH: return "EPOCH_MISMATCH";
    case EphemerisStatus::ORIGIN_MISMATCH: return "ORIGIN_MISMATCH";
    case EphemerisStatus::INVALID_EPOCH: return "INVALID_EPOCH";
    case EphemerisStatus::INVALID_FRAME: return "INVALID_FRAME";
    case EphemerisStatus::INVALID_ORIGIN: return "INVALID_ORIGIN";
    case EphemerisStatus::KERNEL_NOT_FOUND: return "KERNEL_NOT_FOUND";
    case EphemerisStatus::KERNEL_LOAD_FAILURE: return "KERNEL_LOAD_FAILURE";
    case EphemerisStatus::SPICE_ERROR: return "SPICE_ERROR";
    case EphemerisStatus::STATE_UNAVAILABLE: return "STATE_UNAVAILABLE";
    case EphemerisStatus::UNIT_CONVERSION_ERROR: return "UNIT_CONVERSION_ERROR";
    }
    return "UNKNOWN";
}

const char* epochTypeName(EpochType type) {
    switch (type) {
    case EpochType::JulianDate: return "julian_date";
    }
    return "unknown";
}

const char* referenceFrameName(ReferenceFrame frame) {
    switch (frame) {
    case ReferenceFrame::Heliocentric: return "heliocentric";
    case ReferenceFrame::Geocentric: return "geocentric";
    case ReferenceFrame::Barycentric: return "barycentric";
    }
    return "unknown";
}

bool Frame::valid() const {
    if (originBodyId.empty() || orientation != "J2000") return false;
    switch (type) {
    case ReferenceFrame::Heliocentric: return originBodyId == "sun";
    case ReferenceFrame::Geocentric: return originBodyId == "earth";
    case ReferenceFrame::Barycentric: return originBodyId == "solar_system_barycenter";
    }
    return false;
}

bool parseReferenceFrame(const std::string& value, Frame& frame) {
    if (value == "heliocentric") frame = Frame::heliocentric();
    else if (value == "geocentric") frame = Frame::geocentric();
    else if (value == "barycentric") frame = Frame::barycentric();
    else return false;
    return true;
}

bool EphemerisSnapshot::add(const EphemerisState& state) {
    if (!state.valid()) { status = state.status == EphemerisStatus::SUCCESS ? EphemerisStatus::INVALID_DATA : state.status; message = state.message; return false; }
    if (states.empty()) { epoch = state.epoch; frame = state.frame; }
    if (state.epoch.type != epoch.type || std::abs(state.epoch.value - epoch.value) > 1e-12) {
        status = EphemerisStatus::EPOCH_MISMATCH; message = "all snapshot states must share one Julian date"; return false;
    }
    if (state.frame.type != frame.type || state.frame.originBodyId != frame.originBodyId || state.frame.orientation != frame.orientation || state.units != "SI") {
        status = EphemerisStatus::ORIGIN_MISMATCH; message = "all snapshot states must share one frame and origin"; return false;
    }
    const auto duplicate = std::find_if(states.begin(), states.end(), [&](const EphemerisState& current) { return current.bodyId == state.bodyId; });
    if (duplicate != states.end()) { status = EphemerisStatus::INVALID_DATA; message = "snapshot contains a duplicate body"; return false; }
    states.push_back(state);
    return true;
}

} // namespace bag
