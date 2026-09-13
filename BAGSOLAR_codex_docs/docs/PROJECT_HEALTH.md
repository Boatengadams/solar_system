# BAGSOLAR Project Health

## Completed phases

Phases 0 through 8 are complete for their documented scope. Phase 3 is
preserved by the `phase-3-complete` tag. Phase 8 has additionally passed a
real CSPICE verification using the NAIF CSPICE N0067 toolkit and the external
NAIF `de440.bsp` planetary kernel.

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

## Current Phase 9 status

### Implemented

- Nine-lesson deterministic catalog.
- Six guided experiment definitions.
- Raylib-independent progress tracking.
- Lesson and experiment completion state.
- Student observation recording.
- Deterministic completion reports.
- Four deterministic interactive challenge definitions.
- Analytical escape-velocity and circular-orbit scoring.
- Integrator and timestep comparison/scoring using existing benchmark metrics.
- Hohmann transfer scoring using the existing analytical physics reference.
- Learner-facing challenge results in the raylib HUD.
- Challenge attempts, best/latest scores, metrics, versioned JSON persistence,
  and parse-validate-commit import.
- Education unit tests and CTest integration.

### Remaining

- Broader interactive challenge catalog.
- Broader lesson-specific experiment result evaluation.
- Richer education UI integration.

Phase 9 remains partial: challenge/scoring and durable progress checklist work
is complete, while broader education scope remains.

## Current Phase 10 status

### Implemented

- Debug and release CMake presets.
- Linux GitHub Actions build/test/package workflow.
- CPack TGZ packaging.
- Install rules for binaries, data, and documentation.
- Version and changelog metadata.

### Remaining

- Published release binaries.
- Cross-platform CI and packages.
- Installer formats beyond the current TGZ package.
- Screenshots, demo video, and release media.
- Release signing, artifact publication, and a formal 1.0 process.

Estimated Phase 10 completion: 71% of the roadmap checklist.

## Architecture health

The architecture is healthy for the current scope. Physics is raylib-free;
rendering, UI, and input consume simulation state; telemetry observes state;
and CSPICE is isolated in the astronomy provider implementation behind the
ephemeris abstraction. No direct CSPICE dependency appears in physics,
simulation, telemetry, spacecraft, missions, or rendering.

The main remaining architectural concern is that the repository historically
contains tracked generated build files. A `.gitignore` now prevents new build
trees, binaries, packages, kernels, and local manifests from being added, but
removing already tracked generated files should be handled as a separate
repository-hygiene change.

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

The normal CMake suite passes 8/8 tests. The SPICE-enabled suite passes 9/9,
including the real kernel-backed live test. The live test verifies kernel
loading, `spkezr_c` extraction, finite state values, SI-scale conversion,
frame/origin metadata, simulation initialization, and telemetry provenance.

The Makefile targets `make`, `make test`, `make validation`, `make education`,
and `make ephemeris` pass. The Makefile compiles with
`-Wall -Wextra -Wpedantic`; its validation, education, and ephemeris
compilations completed without warnings. A separate clean CMake warning build
was started but did not finish within its bounded window. Exact `git diff
--check` currently reports trailing whitespace/new-blank-line errors in
generated, historically tracked `build/` files.

## Release blockers

- The working tree contains uncommitted Phase 4–10 implementation changes.
- Generated build files are historically tracked and need a separate cleanup
  decision before a clean release commit; they currently prevent an exact
  clean `git diff --check` after verification.
- Phase 9 education challenges and Phase 10 publication/media work remain.
- External CSPICE and kernels remain optional runtime/build inputs and are not
  distributed by BAGSOLAR.
