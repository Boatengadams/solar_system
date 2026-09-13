#include "TelemetryExporter.hpp"

#include <fstream>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>

#include <nlohmann/json.hpp>

namespace bag {
namespace {

using Json = nlohmann::json;

std::string csvEscape(const std::string& value) {
    if (value.find_first_of(",\"\n\r") == std::string::npos) return value;
    std::string escaped = "\"";
    for (char character : value) {
        if (character == '\"') escaped += "\"\"";
        else escaped += character;
    }
    return escaped + "\"";
}

void csvNumber(std::ostringstream& output, double value) {
    if (std::isfinite(value)) output << std::setprecision(17) << value;
}

void csvVec(std::ostringstream& output, Vec3 value) {
    csvNumber(output, value.x); output << ','; csvNumber(output, value.y); output << ','; csvNumber(output, value.z);
}

Json numberOrNull(double value) { return std::isfinite(value) ? Json(value) : Json(nullptr); }
Json vecOrNull(Vec3 value) {
    if (!std::isfinite(value.x) || !std::isfinite(value.y) || !std::isfinite(value.z)) return Json(nullptr);
    return Json::array({value.x, value.y, value.z});
}

} // namespace

std::string telemetryCsv(const TelemetrySession& session) {
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << "# bagsolar_telemetry_schema_version=" << TELEMETRY_SCHEMA_VERSION << '\n';
    output << "# ephemeris_provider=" << csvEscape(session.metadata.ephemerisProvider) << '\n';
    output << "# ephemeris_source=" << csvEscape(session.metadata.ephemerisSource) << '\n';
    output << "# ephemeris_origin_body_id=" << csvEscape(session.metadata.ephemerisOriginBodyId) << '\n';
    output << "# ephemeris_units=" << csvEscape(session.metadata.ephemerisUnits) << '\n';
    output << "# ephemeris_epoch_jd="; csvNumber(output, session.metadata.ephemerisEpochJulianDate); output << '\n';
    output << "simulation_time_s,epoch,reference_frame,body_id,reference_body_id,position_x_m,position_y_m,position_z_m,velocity_x_mps,velocity_y_mps,velocity_z_mps,acceleration_x_mps2,acceleration_y_mps2,acceleration_z_mps2,mass_kg,kinetic_energy_j,potential_energy_j,total_mechanical_energy_j,specific_energy_j_per_kg,angular_momentum_x_m2_per_s,angular_momentum_y_m2_per_s,angular_momentum_z_m2_per_s,angular_momentum_magnitude_m2_per_s,eccentricity,semi_major_axis_m,periapsis_m,apoapsis_m,distance_to_reference_m,relative_speed_mps,timestep_s,requested_timestep_s,integrator,status\n";
    for (const TelemetrySample& sample : session.samples) {
        csvNumber(output, sample.simulationTimeSeconds); output << ',' << csvEscape(sample.epoch) << ',' << csvEscape(sample.referenceFrame) << ',' << csvEscape(sample.bodyId) << ',' << csvEscape(sample.referenceBodyId) << ',';
        csvVec(output, sample.positionM); output << ','; csvVec(output, sample.velocityMps); output << ','; csvVec(output, sample.accelerationMps2); output << ',';
        csvNumber(output, sample.massKg); output << ','; csvNumber(output, sample.kineticEnergyJ); output << ','; csvNumber(output, sample.potentialEnergyJ); output << ','; csvNumber(output, sample.totalMechanicalEnergyJ); output << ','; csvNumber(output, sample.specificEnergyJPerKg); output << ',';
        csvVec(output, sample.angularMomentumM2PerS); output << ','; csvNumber(output, sample.angularMomentumMagnitudeM2PerS); output << ','; csvNumber(output, sample.eccentricity); output << ','; csvNumber(output, sample.semiMajorAxisM); output << ','; csvNumber(output, sample.periapsisM); output << ','; csvNumber(output, sample.apoapsisM); output << ','; csvNumber(output, sample.distanceToReferenceM); output << ','; csvNumber(output, sample.relativeSpeedMps); output << ',';
        csvNumber(output, sample.timestepSeconds); output << ','; csvNumber(output, sample.requestedTimestepSeconds); output << ',' << integratorName(sample.integrator) << ',' << telemetryStatusName(sample.status) << '\n';
    }
    return output.str();
}

std::string telemetryJson(const TelemetrySession& session) {
    Json document;
    document["schema_version"] = TELEMETRY_SCHEMA_VERSION;
    document["session"] = {
        {"session_id", session.metadata.sessionId},
        {"scenario_id", session.metadata.scenarioId},
        {"scenario_name", session.metadata.scenarioName},
        {"epoch", session.metadata.epoch.empty() ? Json(nullptr) : Json(session.metadata.epoch)},
        {"reference_frame", session.metadata.referenceFrame},
        {"reference_body_id", session.metadata.referenceBodyId.empty() ? Json(nullptr) : Json(session.metadata.referenceBodyId)},
        {"ephemeris_provider", session.metadata.ephemerisProvider.empty() ? Json(nullptr) : Json(session.metadata.ephemerisProvider)},
        {"ephemeris_source", session.metadata.ephemerisSource.empty() ? Json(nullptr) : Json(session.metadata.ephemerisSource)},
        {"ephemeris_origin_body_id", session.metadata.ephemerisOriginBodyId.empty() ? Json(nullptr) : Json(session.metadata.ephemerisOriginBodyId)},
        {"ephemeris_units", session.metadata.ephemerisUnits},
        {"ephemeris_epoch_jd", numberOrNull(session.metadata.ephemerisEpochJulianDate)},
        {"integrator", session.metadata.integrator},
        {"initial_timestep_s", numberOrNull(session.metadata.initialTimestepSeconds)},
        {"sampling_interval_s", numberOrNull(session.metadata.samplingIntervalSeconds)},
        {"simulation_start_time_s", numberOrNull(session.metadata.simulationStartTimeSeconds)},
        {"simulation_end_time_s", numberOrNull(session.metadata.simulationEndTimeSeconds)},
        {"status", telemetryStatusName(session.metadata.status)},
    };
    document["samples"] = Json::array();
    for (const TelemetrySample& sample : session.samples) {
        document["samples"].push_back({
            {"simulation_time_s", sample.simulationTimeSeconds}, {"epoch", sample.epoch.empty() ? Json(nullptr) : Json(sample.epoch)},
            {"reference_frame", sample.referenceFrame}, {"body_id", sample.bodyId}, {"reference_body_id", sample.referenceBodyId.empty() ? Json(nullptr) : Json(sample.referenceBodyId)},
            {"position_m", vecOrNull(sample.positionM)}, {"velocity_mps", vecOrNull(sample.velocityMps)}, {"acceleration_mps2", vecOrNull(sample.accelerationMps2)},
            {"mass_kg", numberOrNull(sample.massKg)}, {"kinetic_energy_j", numberOrNull(sample.kineticEnergyJ)}, {"potential_energy_j", numberOrNull(sample.potentialEnergyJ)}, {"total_mechanical_energy_j", numberOrNull(sample.totalMechanicalEnergyJ)}, {"specific_energy_j_per_kg", numberOrNull(sample.specificEnergyJPerKg)},
            {"angular_momentum_m2_per_s", vecOrNull(sample.angularMomentumM2PerS)}, {"angular_momentum_magnitude_m2_per_s", numberOrNull(sample.angularMomentumMagnitudeM2PerS)},
            {"eccentricity", numberOrNull(sample.eccentricity)}, {"semi_major_axis_m", numberOrNull(sample.semiMajorAxisM)}, {"periapsis_m", numberOrNull(sample.periapsisM)}, {"apoapsis_m", numberOrNull(sample.apoapsisM)},
            {"distance_to_reference_m", numberOrNull(sample.distanceToReferenceM)}, {"relative_speed_mps", numberOrNull(sample.relativeSpeedMps)},
            {"timestep_s", sample.timestepSeconds}, {"requested_timestep_s", sample.requestedTimestepSeconds}, {"integrator", integratorName(sample.integrator)}, {"status", telemetryStatusName(sample.status)},
        });
    }
    document["events"] = Json::array();
    for (const TelemetryEvent& event : session.events) {
        document["events"].push_back({{"type", telemetryEventTypeName(event.type)}, {"severity", telemetrySeverityName(event.severity)}, {"simulation_time_s", event.simulationTimeSeconds}, {"body_id", event.bodyId.empty() ? Json(nullptr) : Json(event.bodyId)}, {"other_body_id", event.otherBodyId.empty() ? Json(nullptr) : Json(event.otherBodyId)}, {"message", event.message}});
    }
    return document.dump(2) + "\n";
}

bool writeTelemetryCsv(const std::filesystem::path& path, const TelemetrySession& session) {
    std::ofstream output(path);
    if (!output) return false;
    output.imbue(std::locale::classic());
    output << telemetryCsv(session);
    return static_cast<bool>(output);
}

bool writeTelemetryJson(const std::filesystem::path& path, const TelemetrySession& session) {
    std::ofstream output(path);
    if (!output) return false;
    output.imbue(std::locale::classic());
    output << telemetryJson(session);
    return static_cast<bool>(output);
}

} // namespace bag
