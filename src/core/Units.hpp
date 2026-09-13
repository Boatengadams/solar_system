#pragma once

namespace bag::units {

constexpr double METERS_PER_KILOMETER = 1000.0;
constexpr double METERS_PER_AU = 1.495978707e11;
constexpr double SECONDS_PER_HOUR = 3600.0;
constexpr double SECONDS_PER_DAY = 86400.0;

constexpr double kilometersToMeters(double value) { return value * METERS_PER_KILOMETER; }
constexpr double metersToKilometers(double value) { return value / METERS_PER_KILOMETER; }
constexpr double astronomicalUnitsToMeters(double value) { return value * METERS_PER_AU; }
constexpr double metersToAstronomicalUnits(double value) { return value / METERS_PER_AU; }
constexpr double hoursToSeconds(double value) { return value * SECONDS_PER_HOUR; }
constexpr double secondsToHours(double value) { return value / SECONDS_PER_HOUR; }
constexpr double daysToSeconds(double value) { return value * SECONDS_PER_DAY; }
constexpr double secondsToDays(double value) { return value / SECONDS_PER_DAY; }
constexpr double kilometersPerSecondToMetersPerSecond(double value) { return value * METERS_PER_KILOMETER; }
constexpr double metersPerSecondToKilometersPerSecond(double value) { return value / METERS_PER_KILOMETER; }

} // namespace bag::units
