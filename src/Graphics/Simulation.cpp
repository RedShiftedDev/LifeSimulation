#include "Simulation.h"
#include "Common.h"

namespace simulation {
// Configuration parameters
size_t particleCount = 0;
bool createParticle = false;
int colours = 5;
int addParticle = 0;

// std::vector<glm::vec3> COLORS = {
//     glm::vec3(0.98F, 0.96F, 0.94F), // Soft White - slightly warmed to reduce harshness
//     glm::vec3(0.86F, 0.20F, 0.27F), // Crimson Rose - more sophisticated than pure red
//     glm::vec3(0.48F, 0.72F, 0.46F), // Sage Green - more natural than pure green
//     glm::vec3(0.36F, 0.54F, 0.85F), // Azure Blue - softer than pure blue
//     glm::vec3(0.97F, 0.85F, 0.37F), // Mellow Yellow - warmer and less intense
//     glm::vec3(0.91F, 0.45F, 0.32F), // Terracotta - richer than standard orange
//     glm::vec3(0.21F, 0.39F, 0.26F)  // Forest Green - deeper and more natural
// };

std::vector<glm::vec3> COLORS = {
    // Soft Whites & Neutrals
    glm::vec3(0.98F, 0.96F, 0.94F), // Soft White
    glm::vec3(0.96F, 0.95F, 0.98F), // Pearl White
    glm::vec3(0.94F, 0.92F, 0.90F), // Ivory Cream

    // Reds & Pinks
    glm::vec3(0.86F, 0.20F, 0.27F), // Crimson Rose
    glm::vec3(0.67F, 0.22F, 0.39F), // Mulberry
    glm::vec3(0.95F, 0.46F, 0.60F), // Coral Pink

    // Greens
    glm::vec3(0.48F, 0.72F, 0.46F), // Sage Green
    glm::vec3(0.21F, 0.39F, 0.26F), // Forest Green
    glm::vec3(0.40F, 0.59F, 0.53F), // Eucalyptus
    glm::vec3(0.57F, 0.76F, 0.64F), // Mint Leaf

    // Blues
    glm::vec3(0.36F, 0.54F, 0.85F), // Azure Blue
    glm::vec3(0.28F, 0.45F, 0.56F), // Steel Blue
    glm::vec3(0.53F, 0.81F, 0.92F), // Sky Blue
    glm::vec3(0.22F, 0.33F, 0.54F), // Navy Dusk

    // Yellows & Golds
    glm::vec3(0.97F, 0.85F, 0.37F), // Mellow Yellow
    glm::vec3(0.95F, 0.78F, 0.34F), // Golden Honey
    glm::vec3(0.89F, 0.82F, 0.51F), // Mustard Gold

    // Oranges & Browns
    glm::vec3(0.91F, 0.45F, 0.32F), // Terracotta
    glm::vec3(0.76F, 0.52F, 0.38F), // Cinnamon
    glm::vec3(0.62F, 0.44F, 0.34F), // Rustic Brown

    // Purples & Violets
    glm::vec3(0.54F, 0.42F, 0.65F), // Lavender Dusk
    glm::vec3(0.38F, 0.25F, 0.45F), // Deep Violet
    glm::vec3(0.72F, 0.52F, 0.74F), // Orchid Bloom

    // Teals & Aquas
    glm::vec3(0.25F, 0.60F, 0.62F), // Teal Ocean
    glm::vec3(0.46F, 0.76F, 0.74F), // Aquamarine

    // Special Accents
    glm::vec3(0.82F, 0.41F, 0.12F), // Burnt Sienna
    glm::vec3(0.70F, 0.65F, 0.82F), // Muted Periwinkle
    glm::vec3(0.38F, 0.35F, 0.31F), // Charcoal Ash
    glm::vec3(0.55F, 0.27F, 0.07F), // Amber Wood
    glm::vec3(0.27F, 0.51F, 0.56F)  // Deep Teal
};

// Add these new variable initializations
int desiredParticleCount = 1000;
bool shouldCreateParticles = false;
bool shouldClearParticles = false;

// Boundary parameters
bool enableBounds = false;
float boundaryLeft = 0.0F;
float boundaryRight = static_cast<float>(WINDOW_WIDTH);
float boundaryTop = 0.0F;
float boundaryBottom = static_cast<float>(WINDOW_HEIGHT);

// Simulation control
float simulationSpeed = 1.0F;
} // namespace simulation
