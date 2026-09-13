# Education System

## Lesson Structure
Each lesson should contain:
- title
- learning objective
- explanation
- setup
- experiment
- expected observation
- telemetry to inspect
- conclusion

## Initial Lessons
1. Newton's law of gravitation
2. Circular orbits
3. Elliptical orbits
4. Escape velocity
5. Kepler's laws
6. Numerical integration
7. Energy conservation
8. Hohmann transfers
9. Gravity assists

Lessons should encourage experimentation rather than only displaying text.

## Phase 9 implementation

The lesson catalog now includes gravity, orbit, escape, eccentricity, Kepler,
conservation, numerical methods, Hohmann transfers, and gravity assists.
Experiments include escape velocity, Kepler scaling, gravity, orbital energy,
Hohmann transfers, and gravity assists. `EducationProgress` tracks completed
lessons, completed experiments, free-form student observations, and a
deterministic completion report without depending on raylib or wall-clock
state.

Challenge workflows and numerical-method comparison scoring are now
implemented as a deterministic, raylib-free domain layer. The current
catalog contains five challenges:

- Escape velocity: analytical speed at the zero-specific-energy boundary.
- Circular orbit: analytical tangential speed at a specified radius.
- Integrator comparison: choose among Euler, Semi-Implicit Euler,
  Velocity-Verlet, and RK4 using weighted energy, position, velocity, and
  stability metrics against a smaller-step RK4 reference.
- Timestep selection: evaluate a learner-selected timestep with
  Velocity-Verlet against the same explicit numerical metrics.
- Hohmann transfer: estimate total delta-v for the coplanar circular transfer
  from Earth's orbit to Mars's orbit, with departure and arrival burn
  references shown in the result.

Each definition publishes its error thresholds and weights. Results retain
raw SI metrics, normalized score, pass/fail state, grade, feedback, physical
explanation, and a next-step suggestion. The reference trajectory is a
numerical teaching reference, not a claim of universal integrator superiority.

The raylib HUD exposes the flow `Education → Challenges → Results`: Z/X
select a challenge, `[`/`]` adjust scalar answers, I cycles integrators, and C
submits. `EducationProgress` records attempts, latest/best score, completion,
latest metrics, and a deterministic text export boundary. Challenge content
follows the existing catalog convention because the project has no education
content loader; the domain definitions are reusable and easy to extend.

## Durable progress

`EducationProgress::serialize()` exports versioned JSON with
`schema_version: 1`, lesson and experiment completion arrays, observation
strings, and per-challenge attempts, completion, best/latest scores, and all
recorded challenge metrics. `save(path)` and `load(path)` provide offline file
operations; `deserialize()` performs parse, schema/finite-value validation,
and only then commits the parsed state. Malformed JSON, missing fields,
unsupported schema versions, changed catalog sizes, and invalid metrics leave
the current in-memory progress unchanged.

The serializer is intentionally a domain boundary rather than a database or
automatic hidden save system. The application exposes explicit
`Simulation::saveEducationProgress(path)` and
`Simulation::loadEducationProgress(path)` calls.

## Hohmann challenge semantics

The Hohmann challenge uses `PhysicsEngine::hohmannTransfer` as its analytical
reference. Initial and target radii are measured from the Sun's center and
must be finite, positive, distinct, and outside the configured solar radius.
The learner submits total delta-v in m/s; the result reports absolute and
relative error against the total and also exposes the first departure burn,
second arrival burn, and their sum. The model assumes coplanar circular
orbits around a point mass, so it is an educational reference rather than a
universal mission optimizer.

Remaining Phase 9 work includes richer challenge content, broader experiment
evaluation, and a more complete education UI.
