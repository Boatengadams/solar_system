# BAGS_LAB Project

## Vision

BAGS_LAB is an interactive C++17 scientific orbital laboratory for learning and
experimenting with Newtonian gravity, numerical integration, telemetry,
ephemeris contracts, education, and mission analysis — packaged so it can be
developed and distributed from a USB drive.

## Product principle

Prefer scientific correctness, testability, extensibility, and educational
value over visual complexity. Prefer improving the existing architecture over
rewriting it.

## Baseline preserved from BAGSOLAR

- Newtonian N-body core and integrators
- Validation suite
- Telemetry and education systems
- Optional Horizons / CSPICE boundaries
- raylib 3D laboratory UI
- Planet presentation assets
- Curriculum Learning Lab

## Non-goals

- Not a Celestia replacement
- Not a general game engine
- Not a production high-fidelity propagator
- Not a single universal binary for all OSes

## Portability goals

- Executable-relative resource discovery
- Offline default laboratory
- Platform-native runtime packages under `dist/`
- Development directly from an arbitrary project path
