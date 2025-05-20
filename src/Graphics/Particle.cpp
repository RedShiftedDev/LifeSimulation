#include "Particle.h"
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include "Graphics/Simulation.h"

// Static shader and buffer objects for all particles
unsigned int Particle::instanceVBO = 0;
unsigned int Particle::quadVAO = 0;
unsigned int Particle::quadVBO = 0;
std::unique_ptr<Shader> Particle::particleShader = nullptr;
std::vector<glm::vec4> Particle::instanceData;
bool Particle::initialized = false;
size_t Particle::particleCount = 0;
const size_t Particle::MAX_PARTICLES = 1000000; // 1M particles

Particle::Particle()
    : position(0.0F), velocity(0.0F), acceleration(0.0F), radius(5.0F), color(1.0F), active(true) {

  if (!initialized) {
    initializeSharedResources();
  }

  // Register this particle
  particleIndex = particleCount++;
  if (particleCount > MAX_PARTICLES) {
    // Handle error: too many particles
    active = false;
  }

  // Initialize instance data for this particle
  if (active) {
    updateInstanceData();
  }
}

Particle::Particle(const Particle &other)
    : position(other.position), velocity(other.velocity), acceleration(other.acceleration),
      radius(other.radius), color(other.color), active(other.active) {

  // Register this particle
  particleIndex = particleCount++;
  if (particleCount > MAX_PARTICLES) {
    // Handle error: too many particles
    active = false;
  }

  // Initialize instance data for this particle
  if (active) {
    updateInstanceData();
  }
}

Particle &Particle::operator=(const Particle &other) {
  if (this != &other) {
    position = other.position;
    velocity = other.velocity;
    acceleration = other.acceleration;
    radius = other.radius;
    color = other.color;
    active = other.active;

    // Update instance data
    if (active) {
      updateInstanceData();
    }
  }
  return *this;
}

Particle::~Particle() {
  // Individual particles don't need to clean up shared resources
  // That should be handled by a separate cleanup method when the application exits
}

void Particle::initializeSharedResources() {
  // Initialize shader
  particleShader = std::make_unique<Shader>(
      "/Users/redshifted/codes/LifeSimulation/src/Graphics/shaders/particle.vert",
      "/Users/redshifted/codes/LifeSimulation/src/Graphics/shaders/particle.frag");

  // Create a basic quad for all particles
  float quadVertices[] = {// positions        // texture coords
                          -0.5F, 0.5F,  0.0F, 0.0F, 1.0F, 0.5F,  0.5F,  0.0F, 1.0F, 1.0F,
                          0.5F,  -0.5F, 0.0F, 1.0F, 0.0F, -0.5F, -0.5F, 0.0F, 0.0F, 0.0F};

  unsigned int indices[] = {
      0, 1, 2, // first triangle
      2, 3, 0  // second triangle
  };

  // Setup vertex array object
  glGenVertexArrays(1, &quadVAO);
  glGenBuffers(1, &quadVBO);
  unsigned int EBO;
  glGenBuffers(1, &EBO);

  glBindVertexArray(quadVAO);

  // Quad vertices
  glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

  // Element buffer
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

  // Position attribute
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);

  // Texture coordinate attribute
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)(3 * sizeof(float)));
  glEnableVertexAttribArray(1);

  // Instance data buffer (position, size, color)
  // Format: vec4(posX, posY, radius, active), vec4(r, g, b, padding)
  instanceData.resize(MAX_PARTICLES * 2, glm::vec4(0.0F));

  glGenBuffers(1, &instanceVBO);
  glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
  glBufferData(GL_ARRAY_BUFFER, MAX_PARTICLES * 2 * sizeof(glm::vec4), instanceData.data(),
               GL_DYNAMIC_DRAW);

  // Instance attributes - position and size (vec4)
  glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 2 * sizeof(glm::vec4), (void *)0);
  glEnableVertexAttribArray(2);
  glVertexAttribDivisor(2, 1); // Tell OpenGL this is an instanced attribute

  // Instance attributes - color (vec4)
  glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, 2 * sizeof(glm::vec4),
                        (void *)(sizeof(glm::vec4)));
  glEnableVertexAttribArray(3);
  glVertexAttribDivisor(3, 1); // Tell OpenGL this is an instanced attribute

  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);

  initialized = true;
}

void Particle::cleanupSharedResources() {
  if (initialized) {
    glDeleteVertexArrays(1, &quadVAO);
    glDeleteBuffers(1, &quadVBO);
    glDeleteBuffers(1, &instanceVBO);
    particleShader.reset();
    instanceData.clear();
    initialized = false;
    particleCount = 0;
  }
}

void Particle::init() {
  // Individual particles don't need separate initialization
  // Everything is handled by the constructor and shared resources
}

void Particle::cleanup() {
  active = false;
  updateInstanceData();
}

void Particle::updateInstanceData() {
  if (particleIndex < MAX_PARTICLES) {
    // Update position and size
    instanceData[particleIndex * 2] =
        glm::vec4(position.x, position.y, radius, active ? 1.0F : 0.0F);
    // Update color (with padding)
    instanceData[(particleIndex * 2) + 1] = glm::vec4(color, 0.0F);
  }
}

void Particle::updateAllInstanceData() {
  if (initialized) {
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, particleCount * 2 * sizeof(glm::vec4), instanceData.data());
    glBindBuffer(GL_ARRAY_BUFFER, 0);
  }
}

void Particle::render(const glm::mat4 &projection) {
  // Individual particles don't render themselves
  // Use renderAll() instead
}

void Particle::renderAll(const glm::mat4 &projection) {
  if (!initialized || particleCount == 0) {
    return;
  }

  // Update all particle data in the buffer
  updateAllInstanceData();

  // Activate shader and set uniforms
  particleShader->use();
  particleShader->setMat4("projection", projection);

  // Render all particles with instanced rendering
  glBindVertexArray(quadVAO);
  glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0, particleCount);
  glBindVertexArray(0);
}

void Particle::update(float deltaTime) {
  if (!active) {
    return;
  }

  // Update physics
  velocity += acceleration * deltaTime;
  position += velocity * deltaTime;

  if (simulation::enableBounds) {
    const float boundsX = ((simulation::boundaryRight - simulation::boundaryLeft) / 2) - radius;
    const float boundsY = ((simulation::boundaryBottom - simulation::boundaryTop) / 2) - radius;
    // X-axis bounds checking
    if (position.x > boundsX) {
      position.x = boundsX;
      velocity.x *= -0.9F; // Bounce with some damping
    } else if (position.x < -boundsX) {
      position.x = -boundsX;
      velocity.x *= -0.9F;
    }
    // Y-axis bounds checking
    if (position.y > boundsY) {
      position.y = boundsY;
      velocity.y *= -0.9F;
    } else if (position.y < -boundsY) {
      position.y = -boundsY;
      velocity.y *= -0.9F;
    }
  }

  // Update instance data
  updateInstanceData();
}