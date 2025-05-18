#include "GUI/gui.h"
#include <algorithm>
#include <imgui.h>
#include <random>

namespace gui {

// Simulation parameters
std::vector<Circle> circles;
float gravity = 9.8F;
float bounceFactor = 0.7f;
float friction = 0.02f;
int maxCircles = 100;
float circleMinRadius = 10.0f;
float circleMaxRadius = 50.0f;
float windowWidth = 1280.0f;
float windowHeight = 720.0f;

// Random number generation
std::random_device rd;
std::mt19937 gen(rd());
std::uniform_real_distribution<float> colorDist(0.0f, 1.0f);
std::uniform_real_distribution<float> radiusDist(circleMinRadius, circleMaxRadius);
std::uniform_real_distribution<float> velocityDist(-100.0f, 100.0f);

// Helper function to generate random color
glm::vec3 getRandomColor() { return glm::vec3(colorDist(gen), colorDist(gen), colorDist(gen)); }

void AddCircle(float x, float y) {
  // Limit the total number of circles
  if (circles.size() >= static_cast<size_t>(maxCircles)) {
    return;
  }

  float radius = radiusDist(gen);

  // Make sure circle is within window bounds
  x = std::clamp(x, radius, windowWidth - radius);
  y = std::clamp(y, radius, windowHeight - radius);

  Circle newCircle;
  newCircle.x = x;
  newCircle.y = y;
  newCircle.radius = radius;
  newCircle.color = getRandomColor();
  newCircle.velocityX = velocityDist(gen);
  newCircle.velocityY = velocityDist(gen);

  circles.push_back(newCircle);
}

void RandomizeColors() {
  for (auto &circle : circles) {
    circle.color = getRandomColor();
  }
}

// Manual circle spawning at center with user-defined parameters
bool spawnManualCircle = false;
float manualRadius = 30.0f;
float manualVelocityX = 0.0f;
float manualVelocityY = 0.0f;
ImVec4 circleColor = ImVec4(0.2f, 0.5f, 0.8f, 1.0f); // Default to blue

void RenderGui() {
  ImGui::Begin("Simulation Controls");

  // === Physics parameters section ===
  if (ImGui::CollapsingHeader("Physics Parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::SliderFloat("Gravity", &gravity, 0.0f, 50.0f, "%.1f");
    ImGui::SliderFloat("Bounce Factor", &bounceFactor, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("Friction", &friction, 0.0f, 0.1f, "%.3f");
  }

  // === Circle spawning section ===
  if (ImGui::CollapsingHeader("Circle Spawning", ImGuiTreeNodeFlags_DefaultOpen)) {
    if (ImGui::Button("Spawn Random Circle")) {
      float x = windowWidth / 2.0f;
      float y = windowHeight / 2.0f;
      AddCircle(x, y);
    }

    ImGui::SameLine();

    if (ImGui::Button("Clear All Circles")) {
      circles.clear();
    }

    // Manual circle parameters
    ImGui::Checkbox("Custom Circle Settings", &spawnManualCircle);

    if (spawnManualCircle) {
      ImGui::SliderFloat("Radius", &manualRadius, 10.0f, 100.0f, "%.1f");
      ImGui::SliderFloat("X Velocity", &manualVelocityX, -200.0f, 200.0f, "%.1f");
      ImGui::SliderFloat("Y Velocity", &manualVelocityY, -200.0f, 200.0f, "%.1f");
      ImGui::ColorEdit3("Circle Color", (float *)&circleColor);

      if (ImGui::Button("Spawn Custom Circle")) {
        if (circles.size() < static_cast<size_t>(maxCircles)) {
          Circle newCircle;
          newCircle.x = windowWidth / 2.0f;
          newCircle.y = windowHeight / 2.0f;
          newCircle.radius = manualRadius;
          newCircle.color = glm::vec3(circleColor.x, circleColor.y, circleColor.z);
          newCircle.velocityX = manualVelocityX;
          newCircle.velocityY = manualVelocityY;

          circles.push_back(newCircle);
        }
      }
    }

    ImGui::SliderInt("Max Circles", &maxCircles, 1, 500);

    ImGui::Text("Current circle count: %zu", circles.size());
  }

  // === Appearance section ===
  if (ImGui::CollapsingHeader("Appearance", ImGuiTreeNodeFlags_DefaultOpen)) {
    if (ImGui::Button("Randomize All Colors")) {
      RandomizeColors();
    }

    ImGui::SliderFloat("Min Circle Size", &circleMinRadius, 5.0f, 50.0f);
    ImGui::SliderFloat("Max Circle Size", &circleMaxRadius, circleMinRadius, 100.0f);

    // Update the radius distribution when min/max changes
    radiusDist = std::uniform_real_distribution<float>(circleMinRadius, circleMaxRadius);
  }

  // === Explosion effect ===
  if (ImGui::CollapsingHeader("Special Effects")) {
    if (ImGui::Button("Explosion!")) {
      // Apply explosion force to all circles
      float explosionPower = 500.0f;
      float explosionX = windowWidth / 2.0f;
      float explosionY = windowHeight / 2.0f;

      for (auto &circle : circles) {
        // Calculate direction from explosion center
        float dx = circle.x - explosionX;
        float dy = circle.y - explosionY;
        float distance = std::sqrt(dx * dx + dy * dy);

        // Avoid division by zero
        if (distance < 0.1f)
          distance = 0.1f;

        // Apply force inversely proportional to distance
        float force = explosionPower / distance;
        circle.velocityX += (dx / distance) * force;
        circle.velocityY += (dy / distance) * force;
      }
    }

    ImGui::SameLine();

    if (ImGui::Button("Implode")) {
      // Apply implosion force to all circles
      for (auto &circle : circles) {
        // Direction toward center
        float dx = (windowWidth / 2.0f) - circle.x;
        float dy = (windowHeight / 2.0f) - circle.y;
        float magnitude = std::sqrt(dx * dx + dy * dy);

        // Avoid division by zero
        if (magnitude < 0.1f)
          magnitude = 0.1f;

        // Normalize and apply force
        circle.velocityX += (dx / magnitude) * 100.0f;
        circle.velocityY += (dy / magnitude) * 100.0f;
      }
    }
  }

  // Debug/Status section
  ImGui::Separator();
  ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate,
              ImGui::GetIO().Framerate);

  ImGui::End();
}

// Accessor functions
const std::vector<Circle> &GetCircles() { return circles; }

void SetGravity(float value) { gravity = value; }

float GetGravity() { return gravity; }

void SetBounceFactor(float value) { bounceFactor = value; }

float GetBounceFactor() { return bounceFactor; }

void SetFriction(float value) { friction = value; }

float GetFriction() { return friction; }

} // namespace gui