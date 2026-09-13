# Changelog

## Phase 10.2 — Repository and licensing hygiene

- Removed tracked generated build outputs and the tracked development binary.
- Added the project MIT license and a third-party software/external-data
  summary.
- Identified `BAGSOLAR_codex_docs/` as the canonical documentation tree and
  corrected stale Phase 9, SPICE, and Prediction vs Reference wording.

## Phase 9.7

- Added compact application navigation, Help/Controls, scenario discovery,
  telemetry presentation, live settings, and an honest Mission Tools status
  view without changing scientific or telemetry calculations.
- Improved Education home hierarchy while keeping activity scoring and learner
  reporting in the existing education domain layer.

## Phase 9.6

- Added deterministic Prediction vs Reference propagation, metrics, education evaluation, and offline validation coverage.
- Completed learner-facing provider injection and end-to-end Prediction vs Reference education execution.

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
- Added a deterministic nine-lesson Phase 9.5 education catalog with
  prerequisite/reference validation and links to existing activities.
- Added a dedicated education screen, progress visualization, deterministic
  learner reports, and next-activity recommendations using the existing HUD,
  InputController, EducationWorkflow, and schema-v1 progress.

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
