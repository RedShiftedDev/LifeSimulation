#pragma once
#include <cstddef>
#include <vector>
#include "glm/ext/vector_float3.hpp"

namespace simulation {
// Configuration parameters
extern size_t particleCount;
extern bool createParticle;
extern int colours;

extern int addParticle;

extern std::vector<glm::vec3> COLORS;

// Boundary parameters
extern bool enableBounds;
extern float boundaryLeft;
extern float boundaryRight;
extern float boundaryTop;
extern float boundaryBottom;

// Add these new variables to the simulation namespace
extern int desiredParticleCount;
extern bool shouldCreateParticles;
extern bool shouldClearParticles;

// Simulation control
extern float simulationSpeed;
} // namespace simulation