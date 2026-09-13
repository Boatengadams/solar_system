#pragma once

#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include "../core/Vector3.hpp"

namespace bag {

enum class EphemerisStatus {
    SUCCESS,
    INVALID_REQUEST,
    BODY_NOT_FOUND,
    UNSUPPORTED_FRAME,
    UNSUPPORTED_EPOCH,
    NETWORK_ERROR,
    TIMEOUT,
    REMOTE_ERROR,
    PARSE_ERROR,
    INVALID_DATA,
    CACHE_MISS,
    PROVIDER_UNAVAILABLE,
    EPOCH_MISMATCH,
    ORIGIN_MISMATCH,
    INVALID_EPOCH,
    INVALID_FRAME,
    INVALID_ORIGIN,
    KERNEL_NOT_FOUND,
    KERNEL_LOAD_FAILURE,
    SPICE_ERROR,
    STATE_UNAVAILABLE,
    UNIT_CONVERSION_ERROR,
};

const char* ephemerisStatusName(EphemerisStatus status);

enum class EpochType { JulianDate };

struct Epoch {
    EpochType type = EpochType::JulianDate;
    double value = std::numeric_limits<double>::quiet_NaN();

    static Epoch julianDate(double value) { return {EpochType::JulianDate, value}; }
    bool valid() const { return type == EpochType::JulianDate && std::isfinite(value); }
};

enum class ReferenceFrame { Heliocentric, Geocentric, Barycentric };

struct Frame {
    ReferenceFrame type = ReferenceFrame::Heliocentric;
    std::string originBodyId = "sun";
    std::string orientation = "J2000";

    static Frame heliocentric() { return {ReferenceFrame::Heliocentric, "sun", "J2000"}; }
    static Frame geocentric() { return {ReferenceFrame::Geocentric, "earth", "J2000"}; }
    static Frame barycentric() { return {ReferenceFrame::Barycentric, "solar_system_barycenter", "J2000"}; }
    bool valid() const;
};

const char* epochTypeName(EpochType type);
const char* referenceFrameName(ReferenceFrame frame);
bool parseReferenceFrame(const std::string& value, Frame& frame);

struct EphemerisState {
    std::string bodyId;
    Epoch epoch;
    Frame frame;
    Vec3 positionM;
    Vec3 velocityMps;
    std::string source;
    std::string provider;
    std::string units = "SI";
    EphemerisStatus status = EphemerisStatus::SUCCESS;
    std::string message;

    bool valid() const {
        return status == EphemerisStatus::SUCCESS && !bodyId.empty() && epoch.valid() && frame.valid() && units == "SI" &&
               std::isfinite(positionM.x) && std::isfinite(positionM.y) && std::isfinite(positionM.z) &&
               std::isfinite(velocityMps.x) && std::isfinite(velocityMps.y) && std::isfinite(velocityMps.z);
    }
};

struct EphemerisRequest {
    std::string bodyId;
    Epoch epoch;
    Frame frame;
};

struct EphemerisResult {
    EphemerisStatus status = EphemerisStatus::INVALID_REQUEST;
    EphemerisState state;
    std::string message;

    explicit operator bool() const { return status == EphemerisStatus::SUCCESS && state.valid(); }
};

struct EphemerisSnapshot {
    Epoch epoch;
    Frame frame;
    std::vector<EphemerisState> states;
    EphemerisStatus status = EphemerisStatus::SUCCESS;
    std::string message;

    bool add(const EphemerisState& state);
    bool valid() const { return status == EphemerisStatus::SUCCESS && epoch.valid() && frame.valid() && !states.empty(); }
};

} // namespace bag
