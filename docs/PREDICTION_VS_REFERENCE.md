# Prediction vs Reference

BAGSOLAR compares a deterministic numerical prediction with an explicit reference ephemeris state. A reference is a model or data source, not observational truth; the feature is intentionally called **Prediction vs Reference**.

Every request records Julian Date epochs, frame, frame origin and orientation, SI metres/metres-per-second, provider/source, integrator, timestep, initial reference state, and comparison epochs. The prediction starts from the same reference state at the initial epoch. Incompatible epochs, bodies, frames, origins, or units are rejected with structured status values.

The raylib-independent API is `PredictionComparisonRequest`, `PredictionComparisonResult`, and `PredictionComparisonSample` in `src/validation/PredictionComparison.hpp`. It uses the existing `PhysicsEngine::integrate` API and supports multiple samples and Euler, semi-implicit Euler, velocity-Verlet, and RK4.

Metrics include position error (m), velocity error (m/s), relative errors, reference and predicted specific orbital energy (J/kg), absolute energy difference, and relative energy difference. Relative and energy metrics expose defined flags for zero-denominator cases; successful comparisons do not emit NaN or infinity.

Controlled numerical validation against an analytical/two-body reference can measure integration accuracy. A real SPICE/Horizons comparison measures prediction/reference disagreement and may contain both numerical error and dynamical/model mismatch. It must not be described as pure integration error.

The local provider is deterministic and offline. Horizons and SPICE remain existing provider adapters; ordinary builds/tests do not require network access, CSPICE, or a kernel.

The application-owned `Simulation` supplies a configured provider to the
education activity. It defaults to the deterministic local fixture, which
contains the J2000 and one-day Earth/Mars reference states used by the learner
experiment. A caller may inject Horizons or SPICE through the same
`EphemerisProvider` interface; a missing or unavailable provider is reported
without silently falling back.

The normal education sequence is select, start, observe, ready for evaluation,
and evaluate. Observation runs the configured comparison, and the resulting
metrics are passed to `ExperimentEvaluation` and then the existing schema-v1
`EducationProgress`/`LearnerReport` path.
