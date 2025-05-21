#include "ParticleSystem.h"
#include <algorithm>
#include <cmath>
#include <random>
#include "Graphics/Simulation.h"

ParticleSystem::ParticleSystem(size_t maxParticles) : maxParticles(maxParticles) {
  // Reserve space for particles
  particles.reserve(maxParticles);

  // Initialize shared particle resources
  Particle::initializeSharedResources();
}

ParticleSystem::~ParticleSystem() {
  // Clean up particles
  particles.clear();

  // Clean up shared resources when the system is destroyed
  Particle::cleanupSharedResources();
}

void ParticleSystem::update(float deltaTime) {
  // Calculate interaction forces between particles
  calculateInteractionForces(deltaTime);

  // Process physics updates and handle lifecycle for all particles
  for (auto &particle : particles) {
    particle.update(deltaTime);
  }

  // Optional: Remove inactive particles (if your implementation supports this)
  // This is more efficient than removing individual particles throughout the frame
  if (autoRemoveInactive) {
    particles.erase(std::remove_if(particles.begin(), particles.end(),
                                   [](const Particle &p) { return !p.isActive(); }),
                    particles.end());
  }
}

void ParticleSystem::calculateInteractionForces(float deltaTime) {
  const float R_MAX = 200.0F;
  const float gridCellSize = R_MAX;
  const float invRMax = 1.0F / R_MAX;
  const float R_MAX_SQR = R_MAX * R_MAX;
  auto hashCell = [](int x, int y) -> size_t {
    return static_cast<size_t>((x * 73856093) ^ (y * 19349663));
  };
  struct GridCell {
    std::vector<size_t> particleIndices;
  };
  std::vector<size_t> activeParticles;
  activeParticles.reserve(particles.size());
  struct ParticleGridInfo {
    int cellX;
    int cellY;
  };
  std::vector<ParticleGridInfo> particleGridInfo(particles.size());
  std::unordered_map<size_t, GridCell> grid;
  for (size_t i = 0; i < particles.size(); ++i) {
    if (!particles[i].isActive()) {
      continue;
    }
    activeParticles.push_back(i);
    const glm::vec2 &pos = particles[i].getPos();
    int cellX = static_cast<int>(std::floor(pos.x / gridCellSize));
    int cellY = static_cast<int>(std::floor(pos.y / gridCellSize));
    size_t cellHash = hashCell(cellX, cellY);
    particleGridInfo[i].cellX = cellX;
    particleGridInfo[i].cellY = cellY;
    grid[cellHash].particleIndices.push_back(i);
  }
  std::vector<glm::vec2> forceBuffer(particles.size(), glm::vec2(0.0F));
#pragma omp parallel for schedule(dynamic, 64)
  for (int idx = 0; idx < static_cast<int>(activeParticles.size()); ++idx) {
    size_t i = activeParticles[idx];
    glm::vec2 totalForce(0.0F);
    const glm::vec2 &pos_i = particles[i].getPos();
    int type_i = particles[i].getType();
    int cellX = particleGridInfo[i].cellX;
    int cellY = particleGridInfo[i].cellY;
    for (int dx = -1; dx <= 1; ++dx) {
      for (int dy = -1; dy <= 1; ++dy) {
        size_t neighborHash = hashCell(cellX + dx, cellY + dy);
        auto it = grid.find(neighborHash);
        if (it == grid.end()) {
          continue;
        }
        const auto &cellParticles = it->second.particleIndices;
        for (size_t j : cellParticles) {
          if (i == j || !particles[j].isActive()) {
            continue;
          }
          const glm::vec2 &pos_j = particles[j].getPos();
          glm::vec2 distVec = pos_j - pos_i;
          float distSqr = glm::dot(distVec, distVec);
          if (distSqr < 2.5F || distSqr >= R_MAX_SQR) {
            continue;
          }
          float distance = std::sqrt(distSqr);
          float interaction = getInteractionStrength(type_i, particles[j].getType());
          float normDist = distance * invRMax;
          float forceMag = Particle::calculateForce(normDist, interaction);
          glm::vec2 forceDir = distVec * (1.0F / distance);
          totalForce += forceDir * forceMag;
        }
      }
    }
    forceBuffer[i] = totalForce * R_MAX;
  }
#pragma omp parallel for
  for (int idx = 0; idx < static_cast<int>(activeParticles.size()); ++idx) {
    size_t i = activeParticles[idx];
    glm::vec2 newVel = particles[i].getVel() + forceBuffer[i] * deltaTime;
    particles[i].setVel(newVel);
  }
}

void ParticleSystem::render(const glm::mat4 &projection) {
  // Render all particles at once using instanced rendering
  Particle::renderAll(projection);
}

Particle &ParticleSystem::createParticle() {
  // Create a new particle if we haven't reached the limit
  if (particles.size() < maxParticles) {
    particles.emplace_back();
    return particles.back();
  }

  // If we reached the limit, recycle an inactive particle
  for (auto &particle : particles) {
    if (!particle.isActive()) {
      return particle; // Return inactive particle without resetting its state
    }
  }

  // If all particles are active, cycle to the next one without resetting its properties
  size_t index = nextParticleIndex % particles.size();
  nextParticleIndex++;
  return particles[index]; // Return existing particle without resetting its state
}

void ParticleSystem::emitParticles(size_t count, const glm::vec2 &position, float radius,
                                   [[maybe_unused]] float lifetime, const glm::vec2 &velocity) {
  // Create random number generator for particle types
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<int> typeDist(0, 5); // 6 particle types (0-5)

  for (size_t i = 0; i < count; ++i) {
    // Create new particle
    if (particles.size() < maxParticles) {
      particles.emplace_back();
      Particle &particle = particles.back();

      particle.setActive(true);
      particle.setPos(position);
      particle.setRadius(radius);

      // Set new velocity
      float velX = velocity.x;
      float velY = velocity.y;
      particle.setVel(glm::vec2(velX, velY));

      // Set random type (0-5)
      int particleType = typeDist(gen);
      particle.setType(particleType);

      // Set color based on type (using simulation::COLORS or your own color mapping)
      particle.setColor(simulation::COLORS[particleType % simulation::COLORS.size()]);
    }
  }
}

float ParticleSystem::getInteractionStrength(int type1, int type2) {
  // Access the interaction matrix from the Particle class
  return Particle::getInteractionStrength(type1, type2);
}

void ParticleSystem::randomizeInteractions() {
  // Call the static method in Particle to randomize the interaction matrix
  Particle::randomizeInteractionMatrix();
}

void ParticleSystem::clear() {
  particles.clear();
  // Reset particle count in the static Particle class
  Particle::cleanupSharedResources();
  Particle::initializeSharedResources();
}

size_t ParticleSystem::getParticleCount() const { return particles.size(); }

size_t ParticleSystem::getActiveParticleCount() const {
  return std::count_if(particles.begin(), particles.end(),
                       [](const Particle &p) { return p.isActive(); });
}