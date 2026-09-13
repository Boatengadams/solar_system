# BAGSOLAR Roadmap

## Phase 0 — Baseline
- Preserve current working prototype.
- Create a reproducible build.
- Record current behavior.
- Remove compiler warnings.
- Tag the baseline.

Status: baseline build and regression-test foundation established. The
repository has Git metadata and the Phase 3 milestone is tagged
`phase-3-complete`; subsequent phase work remains in the working tree.

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
- Unit tests for gravity. [x]
- Integrator tests. [x]
- Circular orbit tests. [x]
- Elliptical orbit tests. [x]
- Energy conservation tests. [x]
- Momentum conservation tests. [x]
- Kepler-period validation. [x]
- Escape velocity validation. [x]
- Automated benchmark reports. [x]

Status: complete. The raylib-free validation runner provides deterministic
circular, elliptical, and hyperbolic analytical cases; energy and angular
momentum drift; measured orbital periods; timestep refinement; integrator
comparison fields; explicit numerical-failure states; and CSV output. CTest
separates unit, integration, regression, and validation coverage.

## Phase 5 — Telemetry and Analysis
- Telemetry recorder. [x]
- CSV export. [x]
- JSON export. [x]
- Live graphs. [ ]
- Energy drift. [x]
- Momentum drift. [x]
- Position/velocity error. [x]
- Scientific validation screen. [ ]

Status: backend complete. Raylib-free telemetry sessions, simulation-time
sampling, numerical events/status, deterministic CSV/JSON export, analysis,
run comparison, and observational simulation integration are implemented.
Live graphs and a dedicated validation screen remain UI work and are not
claimed complete in Phase 5.

## Phase 6 — Real Astronomical Data
- Implement an ephemeris-provider abstraction. [x]
- Local/static provider first. [x]
- JSON ephemeris provider. [x]
- JPL Horizons provider and isolated HTTP transport. [x]
- Cache downloaded ephemerides. [x]
- Handle units, epochs and reference frames explicitly. [x]
- Never make the application unusable when the network is unavailable. [x]

Status: backend complete for deterministic local/JSON sources and explicit
Horizons requests. Normal builds and tests are offline; live Horizons access
is opt-in through the CLI. Frame conversion, interpolation, and live
reference-data analysis remain later work. Optional SPICE integration was
implemented and verified in Phase 8.

## Phase 7 — Spacecraft and Mission Design
- Spacecraft mass. [x]
- Fuel. [x]
- Engine thrust. [x]
- Specific impulse. [x]
- Burn duration. [x]
- Delta-v. [x]
- Maneuver nodes. [x]
- Hohmann transfers. [x]
- Mission objectives. [x]
- Trajectory prediction. [x]
- Gravity assists. [x]

Status: raylib-free spacecraft and mission backend complete. The model
supports validated mass/propellant budgets, rocket-equation impulses and
finite-duration burns, maneuver analysis, Hohmann node generation,
two-body/N-body trajectory prediction through the existing PhysicsEngine, and
patched-conic gravity-assist turn-angle analysis. Mission UI, high-fidelity
engine orientation/throttle control, and advanced mission optimization remain
future work.

## Phase 8 — SPICE / Advanced Data
- Optional NASA SPICE provider boundary. [x]
- Kernel manifest validation. [x]
- CSPICE CMake integration. [x]
- Kernel loading/unloading abstraction. [x]
- Real CSPICE state extraction. [x]
- Julian Date/ET conversion. [x]
- Body, frame, origin, and SI-unit mapping. [x]
- Provider-neutral ephemeris comparison. [x]
- Mission trajectory visualization. [ ]
- Real mission timeline support. [ ]

Status: COMPLETE for the implemented Phase 8 scope. The official NAIF
header/archive link successfully, external LSK and `de440.bsp` kernels load,
and a real Earth state query at JD 2451545.0 has been verified through
`spkezr_c`, including SI conversion, explicit J2000 orientation/Sun origin,
and telemetry provenance. Visualization and mission timelines remain later
work.

## Phase 9 — Educational Platform
- Lessons. [x]
- Guided experiments. [x]
- Challenges. [ ]
- Numerical-method comparisons. [ ]
- Student experiment reports. [x]
- Progress tracking. [x]

Status: education catalog and deterministic progress/report foundations are
implemented. Challenge workflows and numerical-method comparison lessons
remain future work.

## Phase 10 — Professional Release
- Cross-platform build presets. [x]
- GitHub Actions. [x]
- Release binaries. [ ]
- Installer/package support. [x]
- Documentation package. [x]
- Screenshots and demo video. [ ]
- Changelog and versioning. [x]

Status: release infrastructure is implemented for Linux CI, CMake presets,
CPack TGZ packaging, installation rules, and changelog/version metadata.
Published binaries, media assets, and cross-platform CI remain outstanding.

## Priority Rule
Do not start advanced features until the previous phase has tests and a stable interface.
