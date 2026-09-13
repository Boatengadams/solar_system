# BAGSOLAR Project Specification

## Vision
BAGSOLAR is an interactive C++17/raylib astrodynamics laboratory for learning and experimenting with orbital mechanics.

It combines:
- Real SI-unit physics
- N-body Newtonian gravity
- Multiple numerical integrators
- Real astronomical data
- Spacecraft and mission design
- Scientific telemetry and validation
- Interactive lessons and experiments
- A modular, testable software architecture

## Product Principle
Prefer scientific correctness, testability, extensibility, and educational value over visual complexity.

## Current Baseline
The existing prototype contains:
- Sun and eight planets
- Educational spacecraft
- Newtonian gravity
- Velocity-Verlet integration
- Orbit trails and vectors
- Camera controls
- Planet selection
- Telemetry
- Lessons and experiments
- CMake/Makefile support

The prototype should be refactored rather than discarded.

## Target
Turn the prototype into a maintainable, modular application without prematurely overengineering it.

## Non-Goals
Do not turn BAGSOLAR into:
- A full Celestia replacement
- A general-purpose game engine
- An unnecessarily network-dependent application
- A feature collection without tests
