#include "Particle.h"
#include <glad/glad.h>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include "Simulation.h"

Particle::Particle()
    : position(glm::vec2{0, 0}), velocity(glm::vec2{0, 0}), acceleration(glm::vec2{0, 0}),
      radius(2.5F), color(glm::vec3{1, 1, 1}), VAO(0), VBO(0), EBO(0) {
  init();
}

Particle::Particle(const Particle &other)
    : position(other.position), velocity(other.velocity), acceleration(other.acceleration),
      radius(other.radius), color(other.color), VAO(0), VBO(0), EBO(0) {
  init();
}

Particle &Particle::operator=(const Particle &other) {
  if (this != &other) {
    // Clean up existing resources
    cleanup();

    // Copy properties
    position = other.position;
    velocity = other.velocity;
    acceleration = other.acceleration;
    radius = other.radius;
    color = other.color;

    // Create new resources
    init();
  }
  return *this;
}

Particle::~Particle() { cleanup(); }

void Particle::init() {
  // Initialize shader
  shader = std::make_unique<Shader>(
      "/Users/redshifted/LifeSimulation/src/Graphics/shaders/shader2D.vert",
      "/Users/redshifted/LifeSimulation/src/Graphics/shaders/shader2D.frag");

  // Setup OpenGL buffers
  setupBuffers();
}

void Particle::cleanup() {
  if (VAO != 0) {
    glDeleteVertexArrays(1, &VAO);
    VAO = 0;
  }

  if (VBO != 0) {
    glDeleteBuffers(1, &VBO);
    VBO = 0;
  }

  if (EBO != 0) {
    glDeleteBuffers(1, &EBO);
    EBO = 0;
  }
}

void Particle::setupBuffers() {
  // Generate circle vertices
  const int segments = 32;
  std::vector<float> vertices;

  // Center point
  vertices.push_back(0.0F); // x
  vertices.push_back(0.0F); // y
  vertices.push_back(0.0F); // z

  // Points around the circle
  for (int i = 0; i <= segments; ++i) {
    float angle = 2.0F * M_PI * i / segments;
    float xPos = cosf(angle);
    float yPos = sinf(angle);

    vertices.push_back(xPos);
    vertices.push_back(yPos);
    vertices.push_back(0.0F);
  }

  // Generate indices for triangle fan
  std::vector<unsigned int> indices;
  for (int i = 0; i <= segments; ++i) {
    indices.push_back(0); // Center point
    indices.push_back(i + 1);
    indices.push_back(i + 2 > segments + 1 ? 1 : i + 2);
  }

  // Create and bind the VAO, VBO, and EBO
  glGenVertexArrays(1, &VAO);
  glGenBuffers(1, &VBO);
  glGenBuffers(1, &EBO);

  glBindVertexArray(VAO);

  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(),
               GL_STATIC_DRAW);

  // Specify vertex attributes
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);

  // Unbind buffers
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);
}

void Particle::render(const glm::mat4 &projection) {
  shader->use();
  shader->setVec3("color", color);

  // Create model matrix for positioning and scaling
  glm::mat4 model = glm::mat4(1.0F);
  model = glm::translate(model, glm::vec3(position.x, position.y, 0.0F));
  model = glm::scale(model, glm::vec3(radius, radius, 1.0F));

  shader->setMat4("model", model);
  shader->setMat4("projection", projection);

  // Draw circle
  glBindVertexArray(VAO);
  glDrawElements(GL_TRIANGLES, (32 * 3), GL_UNSIGNED_INT, 0);
  glBindVertexArray(0);
}

void Particle::update(float deltaTime) {
  // Update velocity based on acceleration
  velocity += acceleration * deltaTime;

  // Update position based on velocity
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
}