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
