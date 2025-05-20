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
  const float R_MAX = 200.0f; // Maximum interaction radius

  // Calculate forces between all particles
  for (size_t i = 0; i < particles.size(); ++i) {
    if (!particles[i].isActive())
      continue;

    glm::vec2 totalForce(0.0f, 0.0f);
    const glm::vec2 pos_i = particles[i].getPos();
    const int type_i = particles[i].getType();

    for (size_t j = 0; j < particles.size(); ++j) {
      if (i == j || !particles[j].isActive())
        continue;

      const glm::vec2 pos_j = particles[j].getPos();
      const int type_j = particles[j].getType();

      // Calculate distance vector
      glm::vec2 distVec = pos_j - pos_i;
      float distance = glm::length(distVec);

      // Only process if within interaction radius and not too close
      if (distance > 1e-6f && distance < R_MAX) {
        // Get interaction strength from matrix
        float interaction = getInteractionStrength(type_i, type_j);

        // Calculate force magnitude
        float forceMag = Particle::calculateForce(distance / R_MAX, interaction);

        // Calculate force direction (normalized)
        glm::vec2 forceDir = distVec / distance;

        // Add to total force
        totalForce += forceDir * forceMag;
      }
    }

    // Scale the total force and apply to particle velocity
    glm::vec2 scaledForce = totalForce * R_MAX;
    glm::vec2 currentVel = particles[i].getVel();
    currentVel += scaledForce * deltaTime;

    // Update particle velocity
    particles[i].setVel(currentVel);
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