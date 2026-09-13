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

Challenge workflows and numerical-method comparison scoring are intentionally
not claimed complete yet.
