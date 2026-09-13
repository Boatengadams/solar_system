# Scientific Validation

## Goal
Prove that the simulator behaves according to known physical relationships.

## Validation Categories
### Analytical
Compare against equations with known solutions.

### Reference Data
Compare against trusted ephemeris/reference implementations.

### Numerical
Measure:
- energy drift
- momentum drift
- position error
- velocity error
- timestep sensitivity

### Prediction vs Reference

The raylib-free comparison layer records Julian Date, frame/origin, SI units,
provider, integrator, timestep, initial state, and sample epochs. Controlled
analytical/two-body comparisons can measure numerical integration accuracy. A
SPICE/Horizons comparison measures prediction/reference disagreement and may
also contain dynamical/model mismatch; it is not automatically pure numerical
error or observational truth. See `docs/PREDICTION_VS_REFERENCE.md`.

## Example Report

```text
Scenario: Earth Circular Orbit
Integrator: Velocity Verlet
Timestep: 3600 s
Duration: 365 days

Orbital period error: 0.002%
Energy drift: 0.0008%
Momentum drift: 0.0001%

RESULT: PASS
```

Validation thresholds must be scenario-specific and documented.

## Phase 4 validation methodology

The validation layer is implemented in `src/validation/` and is independent of
raylib. It uses deterministic barycentric Sun/Earth-mass two-body states. The
relative orbit uses

```text
T = 2π √(a³ / (G (M + m)))
```

with SI quantities from `PhysicsEngine`; no separate scientific constants are
introduced. Circular and elliptical cases are propagated with all four
integrators, while a positive-energy state verifies hyperbolic classification.
The measured period is found from one complete unwrapped relative-position
angle, and the convergence study compares six-hour and three-hour
Velocity-Verlet endpoints against a 90-minute RK4 reference.

The report records case, initial-state identifier, integrator, timestep,
duration, step count, errors, conservation drift, period measurements,
tolerance, status, and message. `bagsolar_validation --csv <path>` writes the
same deterministic report without timestamps.

## Metrics and tolerances

`ValidationTolerances` centralizes the Phase 4 acceptance envelope. The
bounded-orbit radius/energy/angular-momentum envelope is 20%, 25%, and 25%
respectively; the elliptical periapsis/apoapsis envelope is 5%; and measured
period error is 5%. These are experiment-scale tolerances for one-year,
three-to-six-hour integrations: they expose runaway behavior while allowing
low-order integrators to show their expected qualitative drift. Timestep
convergence requires at least a 5% reduction in endpoint position error.
These are validation envelopes, not claims of physical accuracy for arbitrary
scales or timesteps.

## Numerical failure semantics

Results distinguish `PASS`, `EXPECTED_FAILURE`, `NUMERICAL_FAILURE`, and
`INVALID_INPUT`. Non-finite states and failed integration steps are never
treated as ordinary measurements. Existing regression tests cover invalid
states, close approach, collision, adaptive bounds/failure, rollback, and
Hohmann transfers.

## Phase 3 analytical validation

Orbital elements are checked against circular and prescribed elliptical
two-body states. Escape-speed states are classified as hyperbolic rather than
invalid elliptical orbits. Hohmann transfer time is checked against
π√(a_transfer³/μ), and the reverse transfer is checked for the same transfer
axis and time of flight. Adaptive integration uses step-doubling error as its
acceptance criterion and reports failure when the minimum step cannot satisfy
the tolerance or a close approach is numerically unstable.

The benchmark API is a physics-only comparison utility; it does not render or
export telemetry. Its reference trajectory is an RK4 integration at a smaller
step and is therefore a numerical reference, not external ephemeris data.
## Ephemeris source comparison

`compareEphemerisStates` compares valid states only when body, Julian Date,
frame, orientation, origin, and SI units match. It reports position difference
in metres and velocity difference in metres/second, without claiming that independent providers are
identical. This is the foundation for the implemented Prediction vs Reference
workflow.
