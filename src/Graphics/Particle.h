#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <vector>
#include "Graphics/shader.h"

class Particle {
public:
  Particle();
  Particle(const Particle &other);
  Particle &operator=(const Particle &other);
  ~Particle();

  void init();                              // Initialize particle (mostly for compatibility)
  void cleanup();                           // Deactivate particle
  void render(const glm::mat4 &projection); // Individual render (not used in this implementation)
  void update(float deltaTime);             // Update particle state

  // Static methods for batch operations
  static void initializeSharedResources();            // Initialize shared OpenGL resources
  static void cleanupSharedResources();               // Clean up shared OpenGL resources
  static void renderAll(const glm::mat4 &projection); // Render all particles
  static void updateAllInstanceData();                // Update GPU buffer with all particle data

  // Interaction-based simulation methods
  static void initInteractionMatrix(int numTypes);
  static void randomizeInteractionMatrix();
  static float calculateForce(float distance, float interactionStrength);

  // setters
  void setPos(const glm::vec2 &pos) {
    this->position = pos;
    updateInstanceData();
  }

  void setVel(const glm::vec2 &vel) { this->velocity = vel; }

  void setAcc(const glm::vec2 &acc) { this->acceleration = acc; }

  void setRadius(const float radius) {
    this->radius = radius;
    updateInstanceData();
  }

  void setColor(const glm::vec3 &color) {
    this->color = color;
    updateInstanceData();
  }

  void setActive(bool active) {
    this->active = active;
    updateInstanceData();
  }

  void setType(int type) {
    this->particleType = type;
    updateInstanceData();
  }

  // getters
  [[nodiscard]] glm::vec2 getPos() const { return this->position; }
  [[nodiscard]] glm::vec2 getVel() const { return this->velocity; }
  [[nodiscard]] glm::vec2 getAcc() const { return this->acceleration; }
  [[nodiscard]] float getSize() const { return this->radius; }
  [[nodiscard]] glm::vec3 getColor() const { return this->color; }
  [[nodiscard]] bool isActive() const { return this->active; }
  [[nodiscard]] int getType() const { return this->particleType; }

  // Access interaction matrix values
  static float getInteractionStrength(int type1, int type2) {
    if (type1 >= 0 && type1 < numParticleTypes && type2 >= 0 && type2 < numParticleTypes) {
      return interactionMatrix[type1][type2];
    }
    return 0.0F;
  }

  static void setInteractionStrength(int type1, int type2, float strength) {
    if (type1 >= 0 && type1 < numParticleTypes && type2 >= 0 && type2 < numParticleTypes) {
      interactionMatrix[type1][type2] = strength;
    }
  }

  static int getNumParticleTypes() { return numParticleTypes; }

  static void setNumParticleTypes(int num) {
    numParticleTypes = num;
    initInteractionMatrix(numParticleTypes);
  }

  static float getInteractionRadius() { return interactionRadius; }

  static void setInteractionRadius(float radius) { interactionRadius = radius; }

  static float getFrictionFactor() { return frictionFactor; }

  static void setFrictionFactor(float factor) { frictionFactor = factor; }

private:
  glm::vec2 position;
  glm::vec2 velocity;
  glm::vec2 acceleration;
  float radius;
  glm::vec3 color;
  bool active;
  size_t particleIndex;
  int particleType; // Added for particle interaction types

  // Update the instance data for this particle in the shared buffer
  void updateInstanceData();

  // Shared resources for all particles
  static unsigned int quadVAO;
  static unsigned int quadVBO;
  static unsigned int instanceVBO;
  static std::unique_ptr<Shader> particleShader;
  static std::vector<glm::vec4> instanceData;
  static bool initialized;
  static size_t particleCount;
  static const size_t MAX_PARTICLES;

  // Interaction simulation parameters
  static std::vector<std::vector<float>> interactionMatrix;
  static int numParticleTypes;
  static float interactionRadius;
  static float frictionFactor;
  static const float BETA; // Repulsion parameter
};