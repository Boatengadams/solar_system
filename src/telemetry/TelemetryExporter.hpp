#pragma once

#include <filesystem>
#include <string>

#include "TelemetryTypes.hpp"

namespace bag {

std::string telemetryCsv(const TelemetrySession& session);
std::string telemetryJson(const TelemetrySession& session);
bool writeTelemetryCsv(const std::filesystem::path& path, const TelemetrySession& session);
bool writeTelemetryJson(const std::filesystem::path& path, const TelemetrySession& session);

} // namespace bag
