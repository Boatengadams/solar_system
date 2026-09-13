# Data Model

## Body
A body should contain at least:
- id
- name
- type
- mass
- radius
- position
- velocity
- optional visual metadata
- optional orbital metadata

## Scenario
A scenario contains:
- name
- description
- epoch
- reference frame
- bodies
- simulation settings
- optional educational metadata

## Example

```json
{
  "name": "Earth Moon System",
  "epoch": "J2000",
  "reference_frame": "heliocentric",
  "bodies": []
}
```

Scientific data belongs in data files, not source code.

## Phase 2 JSON schema

Body definitions live in `data/bodies/<id>.json`. Scientific quantities use
explicit SI suffixes and are required:

```json
{
  "id": "earth",
  "name": "Earth",
  "type": "Planet",
  "mass_kg": 5.9722e24,
  "radius_m": 6371000.0,
  "initial_position_m": [147099586259.31, 0.0, 0.0],
  "initial_velocity_mps": [0.0, 30286.777677663151, 0.0],
  "parent_id": "sun",
  "visual": {
    "color_rgba": [55, 115, 205, 255],
    "accent_rgba": [110, 210, 255, 255],
    "display_radius_px": 6.0
  },
  "orbital": {
    "semi_major_axis_m": 149597870700.0,
    "eccentricity": 0.0167,
    "orbital_period_s": 31557600.0
  }
}
```

Scenario definitions live in `data/scenarios/<id>.json` and contain metadata,
body references, and simulation settings:

```json
{
  "id": "earth_orbit",
  "name": "Earth Orbit",
  "description": "A focused two-body scenario.",
  "epoch": "J2000",
  "reference_frame": "heliocentric",
  "body_references": ["sun", "earth"],
  "settings": {
    "timestep_s": 3600.0,
    "time_scale": 1.0,
    "integrator": "velocity_verlet",
    "adaptive_timestep": false,
    "minimum_timestep_s": 1.0,
    "maximum_timestep_s": 86400.0,
    "timestep_error_tolerance": 1.0e-8,
    "minimum_safe_separation_m": 1.0e7,
    "trails_enabled": true,
    "vectors_enabled": false,
    "labels_enabled": false
  }
}
```

The loading pipeline is:

```text
JSON file → validated data definitions → BodyFactory → Body/Scenario → Simulation
```

Missing required fields, malformed JSON, invalid numeric values, unsupported
integrators, duplicate references, and unknown parent/body references are
reported as errors. No scientific values are silently substituted.

Adaptive timestep and close-approach fields are optional for backward
compatibility. When present, they must be positive and the maximum timestep
must not be below the minimum timestep.

Phase 2 snapshots use the `bagsolar_simulation_snapshot_v1` format. They store
scenario metadata, simulation time, settings, display flags, and current body
states for future save/load extensions.
