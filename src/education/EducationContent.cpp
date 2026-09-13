#include "EducationContent.hpp"

namespace bag {
namespace {

constexpr Lesson LESSONS[] = {
    {"1  Gravity", "Every massive body attracts every other body. The force grows with mass and falls with the square of distance."},
    {"2  Orbits", "An orbit is continuous free-fall. Tangential velocity makes a body keep missing the object it falls toward."},
    {"3  Escape velocity", "Escape velocity is the speed needed for total mechanical energy to reach zero."},
    {"4  Eccentricity", "Eccentricity measures how stretched an orbit is: 0 is circular, values near 1 are highly elongated."},
    {"5  Kepler's third law", "For bodies orbiting the same central mass, orbital period increases strongly with orbital distance."},
    {"6  Conservation", "In an isolated system, momentum and energy provide powerful checks on a numerical simulation."},
    {"7  Numerical methods", "BAGSOLAR uses a fixed simulation step with velocity-Verlet integration for better long-term stability."},
    {"8  Hohmann transfers", "Two carefully chosen tangential burns can move a spacecraft between circular orbits efficiently."},
    {"9  Gravity assists", "A flyby exchanges energy with a moving planet while the spacecraft speed relative to that planet remains nearly constant."},
};

constexpr Experiment EXPERIMENTS[] = {
    {"escape-velocity", "ESCAPE VELOCITY", "How fast must BAGSOLAR-1 travel to escape the Sun from its current distance?", "vₑ = √(2GM/r)"},
    {"kepler-test", "KEPLER TEST", "Move through the planets and compare distance with orbital period.", "T² ∝ a³"},
    {"gravity-lab", "GRAVITY LAB", "Change mass and distance mentally, then observe the acceleration.", "F = Gm₁m₂/r²"},
    {"orbit-energy", "ORBIT ENERGY", "A negative specific orbital energy means the object is gravitationally bound.", "ε = v²/2 − GM/r"},
    {"hohmann-lab", "HOHMANN LAB", "Compare the departure and arrival burns required for two circular orbits.", "Δv = v_transfer − v_circular"},
    {"assist-lab", "ASSIST LAB", "Vary periapsis and incoming hyperbolic excess speed to inspect turn angle.", "δ = 2 asin(1/e)"},
    {"numerical-methods", "NUMERICAL METHODS", "Compare integrators using energy, endpoint, and stability metrics.", "error = reference − propagated state"},
    {"timestep-sensitivity", "TIMESTEP TEST", "Halve the timestep and inspect convergence of the propagated orbit.", "error decreases as Δt decreases"},
    {"conservation", "CONSERVATION TEST", "Inspect energy and angular-momentum drift in an isolated orbit.", "ΔE/E and ΔL/L"},
    {"prediction-reference", "PREDICTION VS REFERENCE", "Compare a BAGSOLAR numerical prediction with a selected reference ephemeris.", "prediction error = prediction − reference"},
};

} // namespace

const Lesson& lessonAt(int index) { return LESSONS[index % lessonCount()]; }
const Experiment& experimentAt(int index) { return EXPERIMENTS[index % experimentCount()]; }
int lessonCount() { return static_cast<int>(sizeof(LESSONS) / sizeof(LESSONS[0])); }
int experimentCount() { return static_cast<int>(sizeof(EXPERIMENTS) / sizeof(EXPERIMENTS[0])); }

} // namespace bag
