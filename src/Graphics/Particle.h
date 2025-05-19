#pragma once

#include <glm/glm.hpp>
#include "Graphics/shader.h"

class Particle {
public:
  Particle();
  Particle(const Particle &other);
  Particle &operator=(const Particle &other);
  ~Particle();

  void init();                              // Initialize GL resources
  void cleanup();                           // Clean up GL resources
  void render(const glm::mat4 &projection); // Render the particle
  void update(float deltaTime);             // Update particle state

  // setters
  void setPos(const glm::vec2 &pos) { this->position = pos; }
  void setVel(const glm::vec2 &vel) { this->velocity = vel; }
  void setAcc(const glm::vec2 &acc) { this->acceleration = acc; }
  void setRadius(const float radius) { this->radius = radius; }
  void setColor(const glm::vec3 &color) { this->color = color; }

  // getters
  [[nodiscard]] glm::vec2 getPos() const { return this->position; }
  [[nodiscard]] glm::vec2 getVel() const { return this->velocity; }
  [[nodiscard]] glm::vec2 getAcc() const { return this->acceleration; }
  [[nodiscard]] float getSize() const { return this->radius; }
  [[nodiscard]] glm::vec3 getColor() const { return this->color; }

private:
  glm::vec2 position;
  glm::vec2 velocity;
  glm::vec2 acceleration;
  float radius;
  glm::vec3 color;

  // GL resources
  unsigned int VAO;
  unsigned int VBO;
  unsigned int EBO;
  std::unique_ptr<Shader> shader;

  // Helper methods
  void setupBuffers();
};
