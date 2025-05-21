#pragma once
#include <glm/glm.hpp>
#include <memory>
#include "shader.h"

class Renderer {
public:
  Renderer();
  void init();
  void render();

  void setProjectionMatrix(const glm::mat4 &projection) { projectionMatrix = projection; }

  void drawRect(float x, float y, float width, float height, const glm::vec3 &color);
  void drawCircle(float x, float y, float radius, const glm::vec3 &color, int segments = 32);
  void drawLine(float x1, float y1, float x2, float y2, const glm::vec3 &color,
                float thickness = 1.0F);

private:
  std::unique_ptr<Shader> shader2D;
  unsigned int rectVAO, rectVBO;
  unsigned int circleVAO, circleVBO;
  unsigned int lineVAO, lineVBO;
  glm::mat4 projectionMatrix{1.0F};

  // Helper methods to set up buffers
  void setupRectBuffer();
  void setupCircleBuffer();
  void setupLineBuffer();
};