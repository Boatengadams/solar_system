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

## Phase 9.4 education workflow

The existing panels expose a compact education home summary and selected
activity state. Controls are:

- `A` / `D`: previous/next lesson.
- `E` / `Up` / `Down`: select experiments.
- `Z` / `X`: select challenges.
- `Enter`: start; `B`: begin observation.
- `Y`: complete a lesson or evaluate an experiment; `C`: evaluate a challenge.
- `Q`: retry; `N`: continue to the next catalog activity.

Result panels show only metrics available from the result: measured/reference
values, normalized numerical error, or energy/angular-momentum drift as
appropriate. Scoring and physics remain outside the UI.

For Prediction vs Reference, the compact panel additionally shows the
configured provider, body, Julian Date, frame/origin, integrator, timestep,
sample count, position/velocity error, energy comparison when defined,
comparison status, and education result. Provider failure is shown as a
status/explanation; there is no silent provider fallback.

## Phase 9.5 Education screen

`L` toggles a dedicated Education screen over the existing simulation view.
It is organized as Education, progress summary, lessons, selected lesson
details, linked activities, and a compact learner report. The selected lesson
shows its objective, explanation, objectives, prerequisites/status, difficulty,
duration, experiment/challenge links, and completion bar. The report shows
attempt-aware average performance, strongest areas, areas needing practice,
and the deterministic next activity. It does not fabricate unavailable
scientific metrics and does not write progress implicitly.

Education-screen controls are `A`/`D` for lesson selection, `Enter` to start,
`B` to begin observation, `Y` to complete/evaluate, `N` for the recommended
next activity, and Backspace to return to simulation. The normal simulation
HUD remains available when the screen is closed.

## Suggested UI Technology
raylib remains the renderer.
Dear ImGui may be used for developer/scientific panels where appropriate.
# Prediction/reference HUD

Education mode labels Prediction vs Reference and reports provider/source,
epoch, frame/origin, integrator, timestep, errors, status, and evaluation.
Disagreement is not labeled as pure numerical error.

## Phase 9.7 navigation

The application has compact top navigation for Simulation, Education,
Scenarios, Telemetry, Mission Tools, Settings, and Help. The same views are
keyboard-accessible with `Tab`, `L`, `F5`, `F6`, `F7`, `F8`, and `H`; `Esc`
returns to Simulation. The simulation remains the visual centerpiece.

The scenario browser loads the existing data scenarios. Telemetry shows the
latest existing sample, reports a useful empty state, and exposes the existing
session/export APIs (`K`, `J`, and `C`). Mission Tools honestly describes the
available raylib-free mission API; it does not present a fake mission editor.
Settings exposes only current integrator, timestep, speed, trail, vector,
orbit, and grid state. Help documents the implemented keyboard and mouse
controls, and modal screens stop simulation input from leaking through.
