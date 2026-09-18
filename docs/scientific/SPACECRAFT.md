# Spacecraft System

## Model
Spacecraft should support:
- dry mass
- propellant mass
- engine thrust
- specific impulse
- fuel consumption
- orientation
- velocity
- trajectory

## Delta-v
Use the rocket equation where applicable:

Δv = Isp * g0 * ln(m0 / mf)

## Features
- Manual thrust
- Burn duration
- Maneuver vector
- Fuel budget
- Delta-v budget
- Predicted trajectory
- Mission constraints

Spacecraft physics must remain separate from rendering.

## Phase 7 implementation

`Spacecraft` is a raylib-free domain object containing dry mass, propellant
mass, maximum thrust, specific impulse, position, and velocity. Internal units
are kilograms, newtons, seconds, metres, and metres/second. `applyImpulse`
uses the Tsiolkovsky rocket equation and is transactional when the propellant
budget is insufficient. `applyBurn` uses thrust and mass flow

```text
mass flow = thrust / (Isp * g0)
```

and applies the resulting finite-duration delta-v. The standard gravity
constant is explicitly named and is not a rendering or physics-engine global.

Invalid spacecraft data, zero directions, non-positive durations, and fuel
shortages return explicit failure results without partially changing state.

## Scope and limitations

Phase 7 does not yet model attitude, throttle schedules, staging, tank
geometry, finite-burn gravity integration, or spacecraft-specific telemetry
fields. Spacecraft can be propagated as a massive body through the existing
N-body `PhysicsEngine`; maneuver nodes use local radial/prograde/normal
components converted at the maneuver epoch.
