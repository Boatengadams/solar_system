# UI/UX Specification

## Main Screens
- Main Menu
- Simulation
- Scenario Browser
- Body Inspector
- Mission Designer
- Telemetry
- Lessons
- Settings
- Help
- Validation

## Simulation Controls
- Pause/resume
- Reset
- Simulation speed
- Timestep
- Integrator
- Trails
- Vectors
- Labels
- Grid
- Camera controls
- Unit system

The application must remain usable without reading source code.

## Education challenge flow

The existing raylib HUD provides a compact challenge panel without changing
the scientific simulation view. It shows the objective, current answer or
integrator, result score/grade, feedback, and next-step context. Controls are:

- `Z` / `X`: previous/next challenge
- `[` / `]`: adjust scalar answer by five percent
- `I`: cycle numerical integrator
- `C`: submit the current answer

The Hohmann challenge uses the same scalar answer controls for total delta-v.
After submission, the result panel also shows the analytical departure and
arrival burn components in km/s. Progress file save/load is exposed through
the simulation domain API with an explicit caller-provided path; the HUD does
not perform hidden filesystem writes.

Challenge evaluation remains in the raylib-free education domain layer; the
HUD only presents simulation state and the latest result.

## Experiment evaluation flow

The experiment panel uses the existing layout and adds `Y` to evaluate the
current simulation state. It displays the evaluator status, grade, score, a
concise measured/reference line when available, and short feedback. The evaluator remains raylib-independent and valid results
are recorded through `EducationProgress`; no automatic filesystem writes are
performed by the HUD.

## Suggested UI Technology
raylib remains the renderer.
Dear ImGui may be used for developer/scientific panels where appropriate.
