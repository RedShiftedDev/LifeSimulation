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

  // getters
  [[nodiscard]] glm::vec2 getPos() const { return this->position; }
  [[nodiscard]] glm::vec2 getVel() const { return this->velocity; }
  [[nodiscard]] glm::vec2 getAcc() const { return this->acceleration; }
  [[nodiscard]] float getSize() const { return this->radius; }
  [[nodiscard]] glm::vec3 getColor() const { return this->color; }
  [[nodiscard]] bool isActive() const { return this->active; }

private:
  glm::vec2 position;
  glm::vec2 velocity;
  glm::vec2 acceleration;
  float radius;
  glm::vec3 color;
  bool active;
  size_t particleIndex;

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
};