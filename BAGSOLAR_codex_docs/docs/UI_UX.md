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

Challenge evaluation remains in the raylib-free education domain layer; the
HUD only presents simulation state and the latest result.

## Suggested UI Technology
raylib remains the renderer.
Dear ImGui may be used for developer/scientific panels where appropriate.
