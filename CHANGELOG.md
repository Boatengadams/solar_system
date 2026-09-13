# Changelog

## Unreleased

- Added versioned offline JSON persistence for education progress with safe
  parse-validate-commit imports.
- Added a Hohmann transfer challenge using the existing analytical physics
  reference, including departure/arrival burn reporting.
- Added deterministic Phase 9 education challenges for escape velocity,
  circular orbits, integrator comparison, and timestep selection.
- Added explicit numerical scoring, learner feedback, challenge progress, and
  a minimal raylib challenge panel/input flow.
- Added education challenge regression coverage and deterministic progress
  export.
- Added deterministic experiment-result evaluators for analytical references,
  bounded physical behavior, numerical-method comparison, timestep
  convergence, and conservation metrics.
- Added experiment evaluation persistence and a minimal `Y`-key HUD result
  flow without changing the physics engine or adding UI dependencies.
- Centralized experiment-evaluation thresholds, rejected unsafe/non-finite
  numerical observations before scoring, and exposed concise measured/reference
  output in the existing experiment panel.
- Added a raylib-independent Phase 9.4 education workflow with guarded
  select/start/observe/evaluate/retry/continue states, home progress summaries,
  activity descriptions, and existing evaluator/progress routing.

## 0.10.0

- Added deterministic scientific validation, telemetry, ephemeris providers,
  spacecraft and mission-design backends.
- Added CMake presets, Linux CI, installation rules, and CPack TGZ packaging.
- Added guided education progress and experiment reporting foundations.
- SPICE remains an optional integration boundary and is not enabled without a
  CSPICE installation and kernels.

## Future work

- Future work includes richer mission and education UI, cross-platform
  packaging, release assets, and broader external-data analysis.
