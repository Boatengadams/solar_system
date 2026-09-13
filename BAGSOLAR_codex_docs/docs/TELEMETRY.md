# Telemetry and Scientific Analysis

## Purpose

Telemetry observes a simulation without owning or modifying its physical
state. The pipeline is:

```text
Simulation → TelemetrySession → samples/events → analysis/export
```

The subsystem is raylib-free. Rendering may display telemetry later, but CSV,
JSON, analysis, and comparison work without a window.

## Time, epoch, and frame

`simulation_time_s` is elapsed simulation time in seconds. It is not a wall
clock or an astronomical timestamp. Scenario epoch metadata is copied when it
exists; current scenario epoch labels are metadata, not converted ephemeris
times.

Positions, velocities, accelerations, energy, and angular momentum use SI
units. The current scenario reference-frame label is preserved in exports;
Phase 5 does not claim ICRF/J2000 Cartesian coordinates. A reference body is
always identified explicitly by `reference_body_id`; an empty or unavailable
reference makes relative/orbital quantities unavailable rather than silently
assuming the Sun.

## Samples

`TelemetrySample` records one body at one simulation time. It includes:

- Cartesian position, velocity, and read-only calculated acceleration;
- mass, kinetic energy, reference-body potential energy, total mechanical
  system energy, and specific orbital energy;
- specific angular-momentum vector and magnitude;
- eccentricity, semi-major axis, periapsis, and apoapsis when defined;
- reference distance and relative speed;
- actual and requested timestep, integrator, and numerical status.

Unavailable quantities are represented internally as NaN and exported as
empty CSV fields or JSON `null`.

## Sampling

Sampling is simulation-time based, never wall-clock based. `startTelemetry`
records an immediate initial sample. Subsequent samples are recorded when the
simulation crosses the configured interval. `stopTelemetry` records the final
state when it is not already represented at that simulation time. Sampling
does not force an integration step and does not write files during simulation.

## Sessions and events

A `TelemetrySession` stores deterministic metadata, ordered samples, and
ordered events. Events use an enum and severity, with optional body IDs and a
message. Current event types include collision, close approach, invalid state,
integration failure, and adaptive timestep reduction. Physics failures remain
visible in the session; they are not converted into successful samples.

## CSV

`Simulation::exportTelemetryCsv(path)` and `writeTelemetryCsv` produce a
stable, locale-independent CSV. The first line identifies
`bagsolar_telemetry_schema_version=1`; the second line is the stable header.
Numeric values use deterministic precision and unavailable values are empty.
The format is suitable for pandas, NumPy, MATLAB, Excel, and plotting tools.

## JSON

`Simulation::exportTelemetryJson(path)` and `writeTelemetryJson` produce a
versioned JSON document with `session`, `samples`, and `events` sections.
`schema_version` is currently `1`; unavailable scalar values are `null` and
unavailable vectors are `null`. nlohmann/json is already a project dependency
confined to data/export implementation, so no new dependency was introduced.

## Analysis and comparison

`analyzeTelemetry` reports sample count, duration, timestep statistics,
distance/speed ranges, initial/final energy, relative energy drift,
angular-momentum drift, orbital-element changes, periapsis/apoapsis ranges,
event count, and numerical failures. Energy drift uses

```text
abs(E_final - E_initial) / max(abs(E_initial), 1)
```

`compareTelemetry` compares two sessions for a selected body, including final
position and velocity differences, duration, sample counts, timestep, and
conservation-drift differences.

## Limitations and future use

The Phase 5 telemetry layer contains no provider-specific networking,
spacecraft propulsion, or external reference-data dependency. The telemetry
schema is intentionally general so
future ephemeris and Prediction vs Reference datasets can add reference states
without changing the simulation or renderer contracts.

Phase 9.6 does not add comparison samples to telemetry. Comparison results use
the validation API and HUD directly; the existing telemetry schema remains
unchanged and no second telemetry architecture is introduced.

## Ephemeris provenance

When a run is initialized from an ephemeris snapshot, session metadata records
the provider, source, Julian Date, and origin body. CSV writes these as stable
comment metadata lines; JSON writes them under `session`. The existing schema
version remains 1 because these are optional metadata additions and old sample
columns remain unchanged.
