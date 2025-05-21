#include "renderer.h"
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

Renderer::Renderer() { init(); }

void Renderer::init() {
  shader2D = std::make_unique<Shader>(
      "../../../../src/Graphics/shaders/shader2D.vert",
      "../../../../src/Graphics/shaders/shader2D.frag");

  setupRectBuffer();
  setupCircleBuffer();
  setupLineBuffer();
}

void Renderer::setupRectBuffer() {
  float vertices[] = {
      0.5F,  0.5F,  0.0F, // top right
      0.5F,  -0.5F, 0.0F, // bottom right
      -0.5F, -0.5F, 0.0F, // bottom left
      -0.5F, 0.5F,  0.0F  // top left
  };
  unsigned int indices[] = {
      0, 1, 3, // first triangle
      1, 2, 3  // second triangle
  };

  unsigned int EBO;
  glGenVertexArrays(1, &rectVAO);
  glGenBuffers(1, &rectVBO);
  glGenBuffers(1, &EBO);

  glBindVertexArray(rectVAO);

  glBindBuffer(GL_ARRAY_BUFFER, rectVBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);

  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);
}

void Renderer::setupCircleBuffer() {
  glGenVertexArrays(1, &circleVAO);
  glGenBuffers(1, &circleVBO);
}

void Renderer::setupLineBuffer() {
  float vertices[] = {
      0.0F, 0.0F, 0.0F, // start point
      1.0F, 0.0F, 0.0F  // end point
  };

  glGenVertexArrays(1, &lineVAO);
  glGenBuffers(1, &lineVBO);

  glBindVertexArray(lineVAO);

  glBindBuffer(GL_ARRAY_BUFFER, lineVBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);

  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);
}

void Renderer::render() {
  // This is a placeholder - rendering will be done by individual draw calls
}

void Renderer::drawRect(float x, float y, float width, float height, const glm::vec3 &color) {
  shader2D->use();
  shader2D->setVec3("color", color);

  // Create model matrix for positioning and scaling
  glm::mat4 model = glm::mat4(1.0F);
  model = glm::translate(
      model, glm::vec3(x + (width / 2), y + (height / 2), 0.0F)); // Position at center of rectangle
  model = glm::scale(model, glm::vec3(width, height, 1.0F));      // Scale to width and height

  shader2D->setMat4("model", model);
  shader2D->setMat4("projection", projectionMatrix);

  glBindVertexArray(rectVAO);
  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
  glBindVertexArray(0);
}

void Renderer::drawCircle(float x, float y, float radius, const glm::vec3 &color, int segments) {
  shader2D->use();
  shader2D->setVec3("color", color);

  // Generate vertices for the circle
  std::vector<float> vertices;
  vertices.push_back(0.0F); // Center point x
  vertices.push_back(0.0F); // Center point y
  vertices.push_back(0.0F); // Center point z

  for (int i = 0; i <= segments; ++i) {
    float angle = 2.0F * M_PI * i / segments;
    float xPos = cosf(angle);
    float yPos = sinf(angle);

    vertices.push_back(xPos);
    vertices.push_back(yPos);
    vertices.push_back(0.0F);
  }

  // Update buffer with new vertices
  glBindVertexArray(circleVAO);
  glBindBuffer(GL_ARRAY_BUFFER, circleVBO);
  glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_DYNAMIC_DRAW);

  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);

  // Create model matrix for positioning and scaling
  glm::mat4 model = glm::mat4(1.0F);
  model = glm::translate(model, glm::vec3(x, y, 0.0F));
  model = glm::scale(model, glm::vec3(radius, radius, 1.0F));

  shader2D->setMat4("model", model);
  shader2D->setMat4("projection", projectionMatrix);

  // Draw circle as triangle fan
  glDrawArrays(GL_TRIANGLE_FAN, 0, segments + 2);
  glBindVertexArray(0);
}

void Renderer::drawLine(float x1, float y1, float x2, float y2, const glm::vec3 &color,
                        float thickness) {
  shader2D->use();
  shader2D->setVec3("color", color);

  // Calculate direction vector
  float dx = x2 - x1;
  float dy = y2 - y1;
  float length = sqrtf((dx * dx) + (dy * dy));

  // Update line vertices
  float vertices[] = {0.0F, 0.0F, 0.0F, length, 0.0F, 0.0F};

  glBindVertexArray(lineVAO);
  glBindBuffer(GL_ARRAY_BUFFER, lineVBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

  // Create model matrix for positioning and rotation
  glm::mat4 model = glm::mat4(1.0F);
  model = glm::translate(model, glm::vec3(x1, y1, 0.0F));

  // Calculate rotation angle
  float angle = atan2f(dy, dx);
  model = glm::rotate(model, angle, glm::vec3(0.0F, 0.0F, 1.0F));

  shader2D->setMat4("model", model);
  shader2D->setMat4("projection", projectionMatrix);

  // Set line width
  glLineWidth(thickness);
  glDrawArrays(GL_LINES, 0, 2);
  glBindVertexArray(0);

  // Reset line width to default
  glLineWidth(1.0F);
}