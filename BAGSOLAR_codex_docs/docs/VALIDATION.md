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
