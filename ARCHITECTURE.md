# BAGS_LAB Architecture

## High-Level Architecture

```text
                    BAGS_LAB APPLICATION
                           |
          +----------------+----------------+
          |                |                |
       UI/Input         Simulation       Education
          |                |                |
          v                v                v
    InputController   PhysicsEngine     Lessons/Experiments
          |                |
          +-------+--------+
                  |
             Simulation
               State
                  |
        +---------+----------+
        |                    |
   Data/Ephemeris       Telemetry
        |                    |
        v                    v
 JSON / Horizons /      CSV / Graphs
 SPICE / Custom
        |
        v
      Renderer
        |
      raylib
```

Prediction vs Reference provider ownership follows the application boundary:
`Simulation` owns a deterministic offline local provider by default and may be
configured with a non-owning `EphemerisProvider` supplied by the application.
Education invokes the existing comparison API through `Simulation`; physics
and education do not depend on SPICE, Horizons, networking, or provider-
specific types. A configured provider failure is preserved as a structured
result rather than replaced by the local provider.

## Directory Structure

```text
BAGS_LAB/
├── CMakeLists.txt
├── Makefile
├── README.md
├── LICENSE
├── CHANGELOG.md
├── PROJECT.md
├── ARCHITECTURE.md
├── ROADMAP.md
├── CONTRIBUTING.md
├── docs/
│   ├── BUILD.md
│   ├── TESTING.md
│   ├── THIRD_PARTY_LICENSES.md
│   ├── scientific/          # physics, ephemeris, telemetry, …
│   └── history/             # completed phase / planning provenance
├── src/
│   ├── app/
│   ├── core/
│   ├── physics/
│   ├── astronomy/
│   ├── spacecraft/
│   ├── missions/
│   ├── data/
│   ├── telemetry/
│   ├── education/
│   ├── curriculum/
│   ├── rendering/
│   ├── ui/
│   ├── simulation/
│   ├── validation/
│   └── input/
├── data/                    # bodies, scenarios, curriculum
├── assets/planets/          # presentation GLBs + manifest
├── tests/
├── tools/                   # package_portable.sh, release_smoke.sh
├── cmake/
├── platform/{linux,windows,macos}/
├── launcher/{linux,windows,macos}/
├── dist/{linux-x64,windows-x64,macos}/
└── third_party/             # vendored nlohmann/json headers
```

## Dependency Direction

Allowed direction:

```text
app
 ├── ui
 ├── input
 ├── rendering
 ├── education
 └── simulation

simulation
 ├── physics
 ├── astronomy
 ├── spacecraft
 ├── missions
 └── telemetry

data
 └── used by domain systems

physics MUST NOT depend on raylib UI code.
```

The physics core must remain independently testable.

## Core Domain Types

Recommended concepts:
- Vector3
- Body
- BodyType
- SimulationState
- SimulationClock
- ReferenceFrame
- OrbitalElements
- Integrator
- PhysicsEngine
- EphemerisProvider
- Spacecraft
- Maneuver
- Mission
- TelemetrySample
- ValidationResult
- Scenario

## Architectural Rule
Rendering reads simulation state. Rendering must not own or secretly modify physics state.

## Phase 1 implementation status

The current implementation has begun the transition from the original
single-file prototype into the modular foundation:

```text
src/
  app/main.cpp
  core/Vector2.hpp, Vector3.hpp, Color.hpp, Star.hpp
  core/Logger.hpp/.cpp
  physics/Body.hpp, PhysicsEngine.hpp/.cpp
  simulation/Simulation.hpp/.cpp
  data/ScenarioLoader.hpp/.cpp
  education/EducationContent.hpp/.cpp
  rendering/Renderer.hpp/.cpp
  ui/HUD.hpp/.cpp
  input/InputController.hpp/.cpp
tests/physics_tests.cpp
```

The Phase 1 scenario loader is intentionally a built-in adapter around the
existing baseline data. Phase 2 replaces that adapter with the JSON-backed
`ScenarioLoader`, `BodyFactory`, and `ScenarioSerializer` in `src/data/`.

## Phase 2 data flow

```text
data/bodies/*.json       data/scenarios/*.json
          \                    /
           ScenarioLoader + validation
                         |
                    BodyFactory
                         |
                 Body / LoadedScenario
                         |
                    Simulation
```

`nlohmann::json` is confined to data-layer implementation files. Physics,
simulation state, rendering, and UI consume validated domain objects only.

## Phase 4 validation flow

```text
ValidationCases → ValidationRunner → PhysicsEngine
                         |
                         +→ ValidationMetrics → CSV / CTest / CLI
```

The validation layer is raylib-free and depends on domain physics only. It
contains deterministic analytical two-body fixtures, conservation metrics,
timestep refinement, and machine-readable reporting. It does not alter the
physics engine or rendering state.

## Phase 5 telemetry flow

```text
Simulation → TelemetryCollector → TelemetrySession
                                      |
                         +------------+------------+
                         v                         v
                    TelemetryAnalyzer       CSV / JSON exporters
```

Telemetry observes state at simulation-time intervals. It has no raylib
dependency, does not perform file I/O while stepping, and preserves explicit
reference-body, epoch, frame, integrator, timestep, and numerical-status
metadata.

## Phase 6 ephemeris flow

```text
Local / JSON / Horizons Provider
              ↓
       EphemerisState
   (epoch, frame, origin, SI)
              ↓
      EphemerisSnapshot
              ↓
Simulation initialization → Physics / Telemetry
```

The physics engine has no provider or HTTP dependency. Horizons request
construction, transport, parsing, body-ID mapping, unit conversion, and cache
behavior are isolated under `src/astronomy/`. Network access occurs only on an
explicit provider query, never during rendering, integration, or telemetry
sampling.

## Phase 8 SPICE flow

```text
JSON kernel manifest
          ↓
SpiceKernelManager → CSPICE furnsh_c/unload_c
          ↓
SpiceEphemerisProvider → EphemerisState (SI, epoch, frame, origin)
          ↓
Simulation / Telemetry
```

`BAGSOLAR_ENABLE_SPICE` defaults to OFF. CSPICE types and linking are confined
to the astronomy implementation target. The rest of the application sees the
existing provider/result abstraction only.

## Phase 7 spacecraft and mission flow

```text
Spacecraft + ManeuverNode
          ↓
       Mission
          ↓
 MissionAnalysis / Hohmann generation
          ↓
   TrajectoryPrediction → PhysicsEngine
          ↓
       Telemetry (future integration)
```

Spacecraft and mission calculations are independent of raylib and external
ephemeris providers. The prediction layer reuses the existing integrator and
does not alter the main simulation state.

## Phase 9.4 education flow

`EducationWorkflow` is a raylib-independent coordinator between the catalog,
existing evaluation authorities, and durable progress. `Simulation` owns the
workflow and exposes narrow activity-control methods to `InputController`;
`HUD` reads the selected activity, workflow state, home summary, and result.
The workflow does not calculate physics, duplicate scoring, or replace the
existing persistence architecture.

## Phase 9.5 education content and reporting flow

The authored catalog and learner report extend the existing education domain:

```text
Physics → Simulation → Telemetry / Validation
                         ↓
              ExperimentEvaluation / Challenges
                         ↓
              EducationProgress (schema v1)
                         ↓
             EducationCatalog / LearnerReport
                         ↓
                 EducationWorkflow
                         ↓
                HUD + InputController
```

`EducationCatalog` is a small deterministic C++ content model with validation
for IDs, references, ordering, durations, and prerequisite cycles. It links
authored lessons to the existing activity catalogs rather than duplicating
scientific definitions. `LearnerReport` is a pure projection of persisted
progress; it adds no persistence schema and owns no physics or scoring rules.
The report recommendation is deterministic and prioritizes incomplete
prerequisites, incomplete lessons, weak results, then the next unattempted
activity. The dedicated screen only renders these domain results and routes
existing workflow actions.
