# Curriculum-Aware Solar System Learning Lab

## Purpose

BAGSOLAR already provides a Newtonian Solar System simulator and a scientific
education catalog (gravity, orbits, escape velocity, etc.). This module adds a
**Ghana curriculum-aware Learning Lab** layer so the same simulation engine can
serve Basic 1 through SHS 3 without hard-coding activities into UI components.

## Architecture

```text
data/curriculum/<version>/     versioned curriculum content (NaCCA-aligned)
src/curriculum/                loader, assessment, mastery, misconceptions
src/physics + src/simulation   unchanged scientific engine
src/education/                 existing SHS-oriented lessons remain available
```

Activities are data-driven JSON. The simulation engine consumes configuration;
it does not own curriculum text or student scoring rules beyond domain evaluators.

## Curriculum version

Current pack: `ghana_nacca_2019`

Official indicator codes are used only where verified against NaCCA / MoE
Science documents (Primary B1–B6 and CCP B7–B10). Where alignment is related
but not exact, activities are marked:

`alignment: supplementary_enrichment`
`curriculumReference: "Supplementary enrichment"`

## Presentation layers

| Layer | Grades | Experience |
| --- | --- | --- |
| Foundation | B1–B3 | Large visuals, identify/classify, minimal reading |
| Explorer | B4–JHS3 | Solar System components, missions, orbit inquiry |
| Scientist | SHS1–SHS3 | Measurements, formulas, graphs, comparison labs |

## Inquiry pattern

`LearningLabSession` implements:

Prediction → Experiment → Observation → Explanation → Assessment → Feedback

## Offline

Curriculum JSON, i18n strings, and body/scenario assets ship with the data tree
and install under `share/bagsolar/data`. Progress persistence continues to use
local files (extended in later phases).

## Application UI

Open **LAB** in the top navigation or press `F9`.

| Control | Action |
| --- | --- |
| `[` / `]` | Cycle learner grade (B1–SHS3) |
| `A` / `D` or Up/Down | Browse grade-filtered activities |
| `Enter` | Start activity or advance inquiry step |
| `N` | Advance inquiry step (records prediction/observation notes when needed) |
| `1`–`6` | Toggle assessment options |
| `C` | Submit assessment |
| `Esc` | Return to Simulation |

Presentation layer follows grade: Foundation (B1–B3), Explorer (B4–JHS3),
Scientist (SHS). Changing grade adjusts labels/orbits/vectors/grid defaults
without changing the physics engine.

## Tests

`bagsolar_curriculum_tests` verifies grade filtering, official vs enrichment
metadata, assessment scoring, misconception detection, inquiry session flow,
and mastery calculation.
