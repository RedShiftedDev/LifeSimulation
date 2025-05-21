#include "ParticleSystem.h"
#include <algorithm>
#include <arm_neon.h>
#include <cmath>
#include <dispatch/dispatch.h>
#include <omp.h>
#include "Graphics/Simulation.h"

std::vector<glm::vec2> ParticleSystem::previousForces;
std::mutex ParticleSystem::previousForcesMutex;

ParticleSystem::ParticleSystem(size_t maxParticles) : maxParticles(maxParticles) {
  particles.reserve(maxParticles);
  Particle::initializeSharedResources();
}

ParticleSystem::~ParticleSystem() {
  particles.clear();
  Particle::cleanupSharedResources();
}

void ParticleSystem::update(float deltaTime) {
  const size_t PARTICLE_THRESHOLD = 100;

  if (particles.size() > PARTICLE_THRESHOLD) {
    calculateInteractionForces(deltaTime);
  } else if (!particles.empty()) {
    simplifiedForceCalculation(deltaTime);
  }

  const size_t BATCH_SIZE = 1024;

  if (particles.size() > BATCH_SIZE) {
#pragma omp parallel for schedule(static)
    for (int i = 0; i < static_cast<int>(particles.size()); ++i) {
      particles[i].update(deltaTime);
    }
  } else {
    for (auto &particle : particles) {
      particle.update(deltaTime);
    }
  }

  if (autoRemoveInactive) {
    particles.erase(std::remove_if(particles.begin(), particles.end(),
                                   [](const Particle &p) { return !p.isActive(); }),
                    particles.end());
  }
}

void ParticleSystem::simplifiedForceCalculation(float deltaTime) {
  size_t n = particles.size();

  struct ParticleData {
    glm::vec2 position;
    int type;
    bool active;
  };

  alignas(16) std::vector<ParticleData> particleData(n);
  alignas(16) std::vector<glm::vec2> forces(n, glm::vec2(0.0F));

  for (size_t i = 0; i < n; ++i) {
    particleData[i].position = particles[i].getPos();
    particleData[i].type = particles[i].getType();
    particleData[i].active = particles[i].isActive();
  }

#pragma omp parallel for schedule(static)
  for (int ii = 0; ii < static_cast<int>(n); ++ii) {
    if (!particleData[ii].active) {
      continue;
    }

    glm::vec2 totalForce(0.0F);
    const glm::vec2 pos_i = particleData[ii].position;
    const int type_i = particleData[ii].type;

    for (size_t jj = 0; jj < n; ++jj) {
      if (ii == static_cast<int>(jj) || !particleData[jj].active) {
        continue;
      }

      const glm::vec2 pos_j = particleData[jj].position;
      glm::vec2 distVec = pos_j - pos_i;

      float distSqr = glm::dot(distVec, distVec);
      if (distSqr < 2.5F || distSqr >= R_MAX_SQR) {
        continue;
      }

      float invDist = 1.0F / std::sqrt(distSqr);
      float distance = distSqr * invDist;
      float interaction = Particle::getInteractionStrength(type_i, particleData[jj].type);
      float normDist = distance * invRMax;
      float forceMag = Particle::calculateForce(normDist, interaction);

      totalForce += distVec * (forceMag * invDist);
    }

    forces[ii] = totalForce * R_MAX;
  }

#pragma omp parallel for schedule(static)
  for (int ii = 0; ii < static_cast<int>(n); ++ii) {
    if (!particleData[ii].active) {
      continue;
    }
    glm::vec2 newVel = particles[ii].getVel() + forces[ii] * deltaTime;
    particles[ii].setVel(newVel);
  }
}

void ParticleSystem::calculateInteractionForces(float deltaTime) {
  const int gridWidth =
      std::ceil((simulation::boundaryRight - simulation::boundaryLeft) / gridCellSize);
  const int gridHeight =
      std::ceil((simulation::boundaryBottom - simulation::boundaryTop) / gridCellSize);

  std::vector<std::vector<size_t>> grid(gridWidth * gridHeight);
  std::vector<size_t> activeParticles;
  std::vector<int> cellX(particles.size());
  std::vector<int> cellY(particles.size());

  populateSpatialGrid(grid, activeParticles, cellX, cellY, gridWidth, gridHeight);

  std::vector<glm::vec2> forceBuffer(particles.size(), glm::vec2(0.0f));

  computeInteractionForcesOMP(grid, activeParticles, cellX, cellY, forceBuffer, gridWidth,
                              gridHeight);
  applyForcesOMP(activeParticles, forceBuffer, deltaTime);
}

void ParticleSystem::populateSpatialGrid(std::vector<std::vector<size_t>> &grid,
                                         std::vector<size_t> &activeParticles,
                                         std::vector<int> &cellX, std::vector<int> &cellY,
                                         int gridWidth, int gridHeight) {

  for (size_t i = 0; i < particles.size(); ++i) {
    if (!particles[i].isActive()) {
      continue;
    }

    activeParticles.push_back(i);

    const glm::vec2 &pos = particles[i].getPos();
    int x = std::clamp(static_cast<int>((pos.x - simulation::boundaryLeft) / gridCellSize), 0,
                       gridWidth - 1);
    int y = std::clamp(static_cast<int>((pos.y - simulation::boundaryTop) / gridCellSize), 0,
                       gridHeight - 1);

    cellX[i] = x;
    cellY[i] = y;
    grid[(y * gridWidth) + x].push_back(i);
  }
}

void ParticleSystem::computeInteractionForcesOMP(const std::vector<std::vector<size_t>> &grid,
                                                 const std::vector<size_t> &activeParticles,
                                                 const std::vector<int> &cellX,
                                                 const std::vector<int> &cellY,
                                                 std::vector<glm::vec2> &forceBuffer, int gridWidth,
                                                 int gridHeight) {

#pragma omp parallel for schedule(static)
  for (size_t idx = 0; idx < activeParticles.size(); ++idx) {
    size_t i = activeParticles[idx];
    glm::vec2 totalForce(0.0F);
    const glm::vec2 &pos_i = particles[i].getPos();
    int type_i = particles[i].getType();
    int x = cellX[i];
    int y = cellY[i];

    for (int dy = -1; dy <= 1; ++dy) {
      for (int dx = -1; dx <= 1; ++dx) {
        int nx = x + dx;
        int ny = y + dy;
        if (nx < 0 || ny < 0 || nx >= gridWidth || ny >= gridHeight) {
          continue;
        }
        const std::vector<unsigned long> &cell = grid[(ny * gridWidth) + nx];
        for (size_t j : cell) {
          glm::vec2 dist = particles[j].getPos() - pos_i;
          float invDist;
          float normDist;
          float interaction;
          float forceMag;

          if (!shouldComputeForce(i, j, particles, dist, invDist, normDist, interaction, forceMag,
                                  type_i)) {
            continue;
          }

          totalForce += dist * (forceMag * invDist);
        }
      }
    }
    forceBuffer[i] = totalForce * R_MAX;
  }
}

bool ParticleSystem::shouldComputeForce(size_t i, size_t j, const std::vector<Particle> &particles,
                                        const glm::vec2 &dist, float &invDist, float &normDist,
                                        float &interaction, float &forceMag, int type_i) const {
  if (i == j || !particles[j].isActive()) {
    return false;
  }

  float distSqr = glm::dot(dist, dist);
  if (distSqr < 2.5F || distSqr >= R_MAX_SQR) {
    return false;
  }

  invDist = 1.0F / std::sqrt(distSqr);
  float distance = distSqr * invDist;
  normDist = distance * invRMax;

  interaction = getInteractionStrength(type_i, particles[j].getType());
  forceMag = Particle::calculateForce(normDist, interaction);

  return true;
}

void ParticleSystem::applyForcesOMP(const std::vector<size_t> &activeParticles,
                                    const std::vector<glm::vec2> &forceBuffer, float deltaTime) {
#pragma omp parallel for schedule(static)
  for (size_t idx = 0; idx < activeParticles.size(); ++idx) {
    size_t i = activeParticles[idx];
    particles[i].setVel(particles[i].getVel() + forceBuffer[i] * deltaTime);
  }
}

void ParticleSystem::render(const glm::mat4 &projection) { Particle::renderAll(projection); }

Particle &ParticleSystem::createParticle() {
  if (particles.size() < maxParticles) {
    if (particles.capacity() == particles.size()) {
      size_t newCapacity = particles.capacity() * 2;
      newCapacity = std::min(newCapacity, maxParticles);
      particles.reserve(newCapacity);
    }
    particles.emplace_back();
    return particles.back();
  }

  static std::vector<size_t> inactiveIndices;

  static uint64_t lastRebuildTime = 0;
  uint64_t currentTime = dispatch_time(0, 0);

  if (inactiveIndices.empty() || currentTime - lastRebuildTime > 1000000000) { // ~1 second
    inactiveIndices.clear();
    for (size_t i = 0; i < particles.size(); i++) {
      if (!particles[i].isActive()) {
        inactiveIndices.push_back(i);
      }
    }
    lastRebuildTime = currentTime;
  }

  if (!inactiveIndices.empty()) {
    size_t index = inactiveIndices.back();
    inactiveIndices.pop_back();
    particles[index].setActive(true);
    return particles[index];
  }

  size_t index = nextParticleIndex % particles.size();
  nextParticleIndex++;
  return particles[index];
}

float ParticleSystem::getInteractionStrength(int type1, int type2) {
  return Particle::getInteractionStrength(type1, type2);
}

void ParticleSystem::randomizeInteractions() { Particle::randomizeInteractionMatrix(); }

void ParticleSystem::clear() {
  particles.clear();
  Particle::cleanupSharedResources();
  Particle::initializeSharedResources();
}

size_t ParticleSystem::getParticleCount() const { return particles.size(); }

size_t ParticleSystem::getActiveParticleCount() const {
  return std::count_if(particles.begin(), particles.end(),
                       [](const Particle &p) { return p.isActive(); });
}