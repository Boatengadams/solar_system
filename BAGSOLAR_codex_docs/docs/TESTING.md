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

## Phase 3 validation coverage

The dependency-free physics tests cover all four integrators, circular and
elliptical states, hyperbolic classification, Hohmann transfers in both
directions, adaptive-step growth and bounds, rejected unstable steps,
close-approach reporting, body-overlap collision reporting, and integrator
comparison metrics. The comparison uses a smaller-step RK4 trajectory as its
reference and reports energy drift, position error, velocity error, timestep,
and integration-step count.
