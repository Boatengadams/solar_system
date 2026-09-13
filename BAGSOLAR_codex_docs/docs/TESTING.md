# Testing Strategy

## Unit Tests
Test pure calculations independently of raylib.

## Integration Tests
Test:
- scenario loading
- simulation initialization
- telemetry
- save/load
- ephemeris conversion

## Regression Tests
Keep baseline scenarios so refactoring does not silently change results.

## Performance Tests
Measure:
- simulation steps/sec
- body count scaling
- telemetry overhead
- rendering overhead

Every physics bug should result in a regression test when practical.

## Phase 4 validation coverage

`bagsolar_validation_tests` runs the deterministic validation suite through
CTest. The standalone `bagsolar_validation` executable prints CSV to stdout or
writes it with `--csv <path>`. `make validation` builds and runs the same CLI;
`make test` runs the complete CTest suite.

The suite measures absolute, relative, maximum, and RMS error where
applicable; energy drift; angular-momentum drift; position and velocity error;
orbital-period error; timestep; duration; and integration-step count.

## Phase 3 regression coverage

The dependency-free physics tests cover all four integrators, circular and
elliptical states, hyperbolic classification, Hohmann transfers in both
directions, adaptive-step growth and bounds, rejected unstable steps,
close-approach reporting, body-overlap collision reporting, and integrator
comparison metrics. The comparison uses a smaller-step RK4 trajectory as its
reference and reports energy drift, position error, velocity error, timestep,
and integration-step count.

## Phase 5 telemetry coverage

`bagsolar_telemetry_tests` covers empty/initial/multiple samples, simulation
time sampling intervals, explicit and missing reference bodies, Cartesian and
derived orbital values, timestep/integrator/status capture, event recording,
CSV determinism and schema headers, JSON schema/metadata/samples/events,
analysis, run comparison, session reset behavior, invalid numerical values,
multiple bodies, and a bounded long-running observational collection.

The test also runs the public `Simulation` telemetry API beside an identical
non-telemetry simulation and verifies identical physical state component by
component. Telemetry is therefore regression-tested as observational rather
than integrative.

## Phase 6 ephemeris coverage

`bagsolar_ephemeris_tests` is deterministic and offline. It covers local
provider success/failure, JSON schema and unit conversion, malformed data,
epoch/frame rejection, coherent multi-body snapshots, Horizons request
parsing through a fake HTTP client, HTTP/network failures, cache reuse, and
simulation initialization with provenance carried into telemetry. The
`bagsolar_ephemeris` CLI provides explicit local/JSON and live Horizons paths;
live network calls are not part of CTest.

## Phase 7 spacecraft coverage

`bagsolar_spacecraft_tests` covers spacecraft validity, total mass, available
delta-v, rocket-equation propellant use, transactional fuel failure, finite
burns, sequential mission fuel accounting, Hohmann maneuver generation,
gravity-assist turn angles, and deterministic trajectory prediction with an
impulsive maneuver.

## Phase 8 SPICE coverage

`bagsolar_advanced_data_tests` runs without CSPICE and verifies explicit
provider-unavailable behavior, manifest syntax, supported kernel types,
missing/duplicate kernel rejection, deterministic load ordering, body mapping,
Julian Date to SPICE ET conversion, and frame/origin mapping. It never needs
network access or kernel files. When a local CSPICE installation and manifest
are available, configure a separate `BAGSOLAR_ENABLE_SPICE=ON` build and run
the same test target plus a user-supplied integration test; normal CI remains
SPICE-free.

The optional `bagsolar_spice_live_tests` target is compiled only in a
CSPICE-enabled build. It loads the manifest named by
`BAGSOLAR_SPICE_TEST_MANIFEST`, queries Earth at JD 2451545.0, checks finite SI
state and provenance, then unloads kernels. If the variable is absent, CTest
marks it skipped with return code 77.

## Phase 9 education coverage

`bagsolar_education_tests` covers challenge catalog validation, deterministic
escape/circular/integrator/timestep scoring, Hohmann transfer scoring against
the existing analytical physics reference, invalid radii and non-finite
answers, pass/fail boundaries, and repeated-result determinism.

It also covers version-1 education-progress JSON round trips, preservation of
attempts/best scores/metrics, file save/load, malformed JSON, unsupported
schema versions, missing fields, invalid metric values, and the requirement
that failed imports do not mutate existing progress.
