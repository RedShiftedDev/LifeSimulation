#include "Simulation.h"
#include "Common.h"

namespace simulation {
// Configuration parameters
size_t particleCount = 0;
bool createParticle = false;
int colours = 5;

// Boundary parameters
bool enableBounds = false;
float boundaryLeft = 0.0F;
float boundaryRight = static_cast<float>(WINDOW_WIDTH);
float boundaryTop = 0.0F;
float boundaryBottom = static_cast<float>(WINDOW_HEIGHT);

// Simulation control
float simulationSpeed = 1.0F;
} // namespace simulation
