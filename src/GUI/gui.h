#pragma once
#include <glm/glm.hpp>
#include <vector>

struct Circle {
  float x;
  float y;
  float radius;
  glm::vec3 color;
  float velocityX;
  float velocityY;
};

namespace gui {
void AddCircle(float x, float y);

void RenderGui();
void RandomizeColors();

const std::vector<Circle> &GetCircles();
void SetGravity(float value);
float GetGravity();
void SetBounceFactor(float value);
float GetBounceFactor();
void SetFriction(float value);
float GetFriction();
} // namespace gui