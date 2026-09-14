# BAGSOLAR Project Health

## Current status

The last clean tagged checkpoint is `phase-11.1-complete`. The current working
tree contains the Phase 11.2 public-presentation documentation updates and
follow-up interaction/rendering improvements covered by the automated checks
below. BAGSOLAR is a public release candidate for its documented offline Linux
scope. The default build is deterministic and does not require network access,
CSPICE, or external kernels.
Optional Horizons and CSPICE paths remain explicitly configured integrations
rather than default dependencies.

Phases 0 through 11.1 are complete for their documented scope. Historical
phase sections remain useful as implementation history; this document
distinguishes automated verification, GUI/manual verification, and screenshot
capture status.

## Phase 8 real verification

The verified query was:

- body: Earth
- epoch: Julian Date 2451545.0
- orientation: J2000
- observer/origin: Sun
- units returned to BAGSOLAR: SI metres and metres/second
- provider: `SpiceEphemerisProvider`
- kernel: external `de440.bsp`, with an external LSK loaded before it

The official `cspice.a` archive linked successfully and exported `spkezr_c`.
Real kernel loading and real state extraction succeeded. The live CTest also
verified finite SI-scale state values and preserved SPICE source, provider,
epoch, orientation, origin, and units in telemetry. Kernels and machine-local
manifests are intentionally outside the repository.

J2000 is treated as the reference orientation. Sun, Earth, and Solar System
Barycenter are observer/origin semantics; changing the observer does not claim
a coordinate rotation. The current supported ephemeris model does not perform
arbitrary frame rotations.

## Education status

### Implemented

- Nine-lesson deterministic catalog.
- Nine guided experiment definitions.
- Raylib-independent progress tracking.
- Lesson and experiment completion state.
- Student observation recording.
- Deterministic completion reports.
- Five deterministic interactive challenge definitions.
- Analytical escape-velocity and circular-orbit scoring.
- Integrator and timestep comparison/scoring using existing benchmark metrics.
- Hohmann transfer scoring using the existing analytical physics reference.
- Learner-facing challenge results in the raylib HUD.
- Challenge attempts, best/latest scores, metrics, versioned JSON persistence,
  and parse-validate-commit import.
- Deterministic experiment-result evaluators for all nine catalog experiments,
  reusing PhysicsEngine, mission, and IntegratorBenchmark references.
- Experiment evaluation attempts, completion, scores, grades, metrics, and
  backward-compatible schema-1 persistence.
- Minimal HUD evaluation flow: `Y` evaluates the current experiment and shows
  its status, score, grade, and feedback.
- Education unit tests and CTest integration.

The main CMake application target links the evaluator implementation used by
the education tests, so the running HUD path exercises the same API.
Evaluator policy constants are shared and malformed numerical observations are
rejected before scoring.

### Scope beyond Phase 9.7

- A general education content CMS.
- Broader optional challenge/content authoring.
- Additional presentation polish beyond the compact documented UI.

## Release engineering status

### Implemented

- Debug and release CMake presets.
- Linux GitHub Actions build/test/package workflow.
- CPack TGZ packaging.
- Install rules for binaries, data, and documentation.
- Version and changelog metadata.

### Remaining

- Public screenshots, demo video, and release media.
- Cross-platform CI and packages.
- Installer formats beyond the current Linux TGZ package.
- Release signing, artifact publication, and a formal 1.0 process.
- Final manual UI/release-candidate testing.

Repository hygiene, licensing, documentation consistency, installed-resource
discovery, and offline package smoke validation are implemented in the current
checkpoint.

## Architecture health

The architecture is healthy for the current scope. Physics is raylib-free;
rendering, UI, and input consume simulation state; telemetry observes state;
and CSPICE is isolated in the astronomy provider implementation behind the
ephemeris abstraction. No direct CSPICE dependency appears in physics,
simulation, telemetry, spacecraft, missions, or rendering.

The main remaining architectural concern is the breadth of `Simulation`, which
coordinates physics, education, telemetry, ephemeris, and application state.
That coupling is accepted for the current release scope and is not being
refactored in the public-release cleanup.

## Scientific health

The validated model uses SI units internally and explicit Julian Date epochs.
Ephemeris states preserve provider, source, epoch, orientation, observer/origin,
and units. Comparisons reject incompatible epochs, orientations, origins, and
units; cross-provider comparisons are explicitly flagged.

Known limitations include Newtonian model simplifications, incomplete
perturbation fidelity, no arbitrary frame rotation, no interpolation between
ephemeris epochs, and no claim of universal astronomical accuracy merely from
using Horizons or SPICE reference states.

## Testing health

The default CMake configuration registers 12 tests, including resource,
installation, and package smoke tests. The deterministic scientific,
education, and local-ephemeris executables are also available offline. The
optional SPICE live test requires a user-supplied CSPICE installation, kernel
manifest, and external kernels; it is not part of the normal offline CI path.
The Makefile application path uses the same warning flags, while `make test`
delegates to the CMake Debug preset so that CTest is configured consistently.
Linux CI covers configure, build, CTest, validation, education, local
ephemeris, installation, and package smoke execution.

## Current release work

- Phase 11.1 covers public README, documentation consistency, and repository
  identity cleanup.
- Phase 11.2 covers screenshots, demonstration media, and public presentation.
  The README capture sequence is documented, but no screenshots were captured
  in the current environment because the raylib window could not establish a
  usable display. Phase 11.2 is therefore not complete.
- Phase 11.3 covers manual UI and release-candidate testing.
- Phase 11.4 covers final release packaging, publication, and release process.

External CSPICE and kernels remain optional runtime/build inputs and are not
distributed by BAGSOLAR. The default offline release path does not depend on
them.
