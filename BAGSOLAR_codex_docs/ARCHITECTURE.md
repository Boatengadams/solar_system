# BAGSOLAR Architecture

## High-Level Architecture

```text
                    BAGSOLAR APPLICATION
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

## Directory Structure

```text
BAGSOLAR/
├── CMakeLists.txt
├── Makefile
├── README.md
├── LICENSE
├── CHANGELOG.md
├── CONTRIBUTING.md
├── ROADMAP.md
├── PROJECT.md
├── ARCHITECTURE.md
├── CODEX.md
├── docs/
│   ├── PHYSICS.md
│   ├── DATA_MODEL.md
│   ├── EPHEMERIS.md
│   ├── SPACECRAFT.md
│   ├── MISSION_DESIGN.md
│   ├── TELEMETRY.md
│   ├── VALIDATION.md
│   ├── EDUCATION.md
│   ├── UI_UX.md
│   ├── TESTING.md
│   ├── BUILD.md
│   └── INTEGRATIONS.md
├── src/
│   ├── main.cpp
│   ├── app/
│   ├── core/
│   ├── physics/
│   ├── astronomy/
│   ├── spacecraft/
│   ├── missions/
│   ├── data/
│   ├── telemetry/
│   ├── education/
│   ├── rendering/
│   ├── ui/
│   └── input/
├── data/
│   ├── scenarios/
│   ├── bodies/
│   └── lessons/
├── tests/
├── assets/
└── tools/
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
