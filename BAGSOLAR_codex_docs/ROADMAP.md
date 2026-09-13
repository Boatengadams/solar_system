# BAGSOLAR Roadmap

## Phase 0 — Baseline
- Preserve current working prototype.
- Create a reproducible build.
- Record current behavior.
- Remove compiler warnings.
- Tag the baseline.

Status: build and regression-test baseline established. A Git tag is blocked
because this workspace does not contain Git metadata.

## Phase 1 — Modular Foundation
- Split `project.cpp`.
- Create physics domain types.
- Create simulation manager.
- Create renderer.
- Create HUD/UI layer.
- Create input controller.
- Create data loader.
- Add logging/error handling.
- Keep behavior equivalent to the prototype.

Status: complete. The modular build, raylib-free physics core, baseline
scenario adapter, education content module, renderer, HUD, input controller,
dependency-free logger, and physics regression test are implemented and
verified. See `BAGSOLAR_codex_docs/IMPLEMENTATION_PLAN.md` for the verification
record.

## Phase 2 — Data-Driven Simulation
- Add JSON body definitions.
- Add JSON scenarios.
- Add scenario loader.
- Add save/load.
- Add custom body creation.
- Add simulation settings.
- Add selectable units.
- Add scenario reset.

Status: complete. Body definitions and scenarios load from validated JSON,
scenario selection/reset is available, snapshots can be serialized and
restored, custom-body construction exists, unit conversions are centralized,
and the data layer has CTest coverage.

## Phase 3 — Scientific Core
- Add Euler. [x]
- Add Semi-Implicit Euler. [x]
- Add Velocity-Verlet. [x]
- Add RK4. [x]
- Add configurable timestep. [x]
- Add adaptive timestep where appropriate. [x]
- Add collision/close-approach handling. [x]
- Add orbital-element calculations. [x]
- Add Hohmann-transfer calculations. [x]

Status: complete. Integrator selection is data-driven and supported by the
physics engine and snapshot validation. Orbital elements, Hohmann transfers,
adaptive step-doubling, transactional rejection handling, close-approach
reporting, and distinct physical-radius collision reporting are implemented
and covered by the dependency-free regression suite. The adaptive scheduler
tracks the accepted step separately from the proposed next step and does not
advance simulation time on rejected steps.

## Phase 4 — Testing and Validation
- Unit tests for gravity.
- Integrator tests.
- Circular orbit tests.
- Elliptical orbit tests.
- Energy conservation tests.
- Momentum conservation tests.
- Kepler-period validation.
- Escape velocity validation.
- Automated benchmark reports.

## Phase 5 — Telemetry and Analysis
- Telemetry recorder.
- CSV export.
- JSON export.
- Live graphs.
- Energy drift.
- Momentum drift.
- Position/velocity error.
- Scientific validation screen.

## Phase 6 — Real Astronomical Data
- Implement an ephemeris-provider abstraction.
- Local/static provider first.
- JPL Horizons provider.
- Cache downloaded ephemerides.
- Handle units, epochs and reference frames explicitly.
- Never make the application unusable when the network is unavailable.

## Phase 7 — Spacecraft and Mission Design
- Spacecraft mass.
- Fuel.
- Engine thrust.
- Specific impulse.
- Burn duration.
- Delta-v.
- Maneuver nodes.
- Hohmann transfers.
- Mission objectives.
- Trajectory prediction.
- Gravity assists.

## Phase 8 — SPICE / Advanced Data
- Optional NASA SPICE integration.
- Kernel management.
- Reference-frame handling.
- Mission trajectory visualization.
- Real mission timeline support.

## Phase 9 — Educational Platform
- Lessons.
- Guided experiments.
- Challenges.
- Numerical-method comparisons.
- Student experiment reports.
- Progress tracking.

## Phase 10 — Professional Release
- Cross-platform builds.
- GitHub Actions.
- Release binaries.
- Installer/package support.
- Documentation site or complete docs.
- Screenshots and demo video.
- Changelog and versioning.

## Priority Rule
Do not start advanced features until the previous phase has tests and a stable interface.
