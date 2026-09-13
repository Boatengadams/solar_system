# Physics Specification

## Units
Use SI units internally:
- Distance: meters
- Mass: kilograms
- Time: seconds
- Velocity: meters/second
- Acceleration: meters/second²
- Energy: joules

Display units may be converted for users.

## Gravity
Newtonian gravitational acceleration:

a = G * M / r²

For N-body systems, acceleration is the vector sum of contributions from all other bodies.

## Integration
Initial required integrators:
1. Euler
2. Semi-Implicit Euler
3. Velocity-Verlet
4. RK4

Each integrator must implement a common interface.

The physics engine supports fixed-step and adaptive stepping. Adaptive stepping
uses step doubling (one full step compared with two half steps), accepts the
half-step state when the normalized position/velocity difference is within the
configured tolerance, and clamps the next step to configured minimum and
maximum values. A rejected step is reported; it is never silently accepted.

Close approach is a numerical policy, not a physical radius. A configurable
minimum safe separation regularizes the inverse-distance calculation while
reporting the affected pair. A substantially smaller separation is reported as
numerically unstable. Body-radius overlap is reported separately as a
collision; bodies are not merged automatically.

Orbital elements classify bound, parabolic, and unbound states. Hyperbolic
states have negative semi-major axes and no apoapsis or period. Hohmann
transfers assume two circular coplanar orbits around a point-mass central body.

## Required Physics Tests
- Two-body circular orbit
- Elliptical orbit
- Escape velocity
- Gravitational acceleration
- Energy conservation
- Momentum conservation
- Kepler orbital period

## Numerical Safety
Protect against:
- Zero/near-zero separation
- NaN/Infinity
- Excessive timestep
- Extremely close encounters
- Invalid mass/radius
- Invalid initial velocity

Never hide numerical failure.
