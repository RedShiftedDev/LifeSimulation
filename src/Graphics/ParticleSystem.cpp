#include "ParticleSystem.h"
#include <algorithm>
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

      // Set color
      particle.setColor(simulation::COLORS[rand() % simulation::COLORS.size()]);
    }
  }
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