#pragma once
#include <cstddef>

namespace simulation {
// Configuration parameters
extern size_t particleCount;
extern bool createParticle;
extern int colours;

// Boundary parameters
extern bool enableBounds;
extern float boundaryLeft;
extern float boundaryRight;
extern float boundaryTop;
extern float boundaryBottom;

// Simulation control
extern float simulationSpeed;
} // namespace simulation