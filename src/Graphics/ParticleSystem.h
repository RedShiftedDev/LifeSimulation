#pragma once

#include <vector>
#include "Particle.h"

class ParticleSystem {
public:
  // Constructor with maximum particle count
  explicit ParticleSystem(size_t maxParticles = 1000000);
  ~ParticleSystem();

  // Main system functions
  void update(float deltaTime);
  static void render(const glm::mat4 &projection);

  // Particle creation and emission
  Particle &createParticle();
  void emitParticles(size_t count, const glm::vec2 &position, float radius,
                     [[maybe_unused]] float lifetime, const glm::vec2 &velocity);

  // System management
  void clear();
  size_t getParticleCount() const;
  size_t getActiveParticleCount() const;

  // Configuration
  void setAutoRemoveInactive(bool value) { autoRemoveInactive = value; }
  bool getAutoRemoveInactive() const { return autoRemoveInactive; }

private:
  std::vector<Particle> particles;
  size_t maxParticles;
  size_t nextParticleIndex = 0;
  bool autoRemoveInactive = true;
};