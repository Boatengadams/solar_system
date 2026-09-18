# Mission Design

## Initial Missions
1. Earth circular orbit
2. Earth escape
3. Earth-to-Mars Hohmann transfer
4. Earth-to-Venus transfer
5. Gravity-assist demonstration

## Mission Objects
- Origin body
- Destination body
- Departure epoch
- Arrival epoch
- Spacecraft
- Maneuvers
- Constraints
- Objective

## Maneuver Node
A maneuver contains:
- epoch
- radial delta-v
- prograde delta-v
- normal delta-v
- total delta-v
- optional burn duration

## Mission Result
Report:
- total delta-v
- fuel consumed
- time of flight
- closest approach
- arrival velocity
- success/failure

## Phase 7 implementation

`Mission` stores origin/destination IDs, departure and arrival simulation
times, a spacecraft, objective, and ordered maneuver nodes. Mission analysis
reports total delta-v, sequential rocket-equation propellant consumption, fuel
feasibility, and time of flight. Hohmann transfer generation reuses the
validated `PhysicsEngine::hohmannTransfer` calculation and creates prograde
departure/arrival nodes.

`predictTrajectory` is deterministic and raylib-free. It propagates a
spacecraft and central body with the existing Velocity-Verlet integrator and
applies explicit impulsive maneuver nodes. `gravityAssistTurn` provides the
patched-conic hyperbolic turn angle from gravitational parameter, periapsis,
and incoming relative speed.

The current implementation does not claim full mission optimization,
ephemeris-targeted arrival matching, finite-burn thrust integration, or
high-fidelity gravity-assist trajectory design.

The Phase 9 Hohmann education challenge reuses the same
`PhysicsEngine::hohmannTransfer` analytical reference as mission design. Its
inputs and outputs use SI metres and metres/second internally; orbit radii are
measured from the central body's center. The challenge reports total delta-v
for the learner answer and the two reference tangential burns separately.
