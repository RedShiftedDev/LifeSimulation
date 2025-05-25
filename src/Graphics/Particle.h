#pragma once

#include <glm/glm.hpp>
#include <limits>
#include <memory>
#include <unordered_map>
#include <vector>
#include <webgpu/webgpu.h>
#include "Graphics/Simulation.h"
#include "Graphics/shader.h"

struct InteractionMatrixCache {
  float values[16][16];
  int size;
};

static thread_local InteractionMatrixCache interactionCache = {{{0}}, 0};

struct ColorHash {
  std::size_t operator()(const glm::vec3 &color) const {
    std::size_t h1 = std::hash<float>{}(color.r);
    std::size_t h2 = std::hash<float>{}(color.g);
    std::size_t h3 = std::hash<float>{}(color.b);
    return h1 ^ (h2 << 1) ^ (h3 << 2);
  }
};

struct ColorEqual {
  bool operator()(const glm::vec3 &a, const glm::vec3 &b) const {
    const float EPSILON = 0.0001F;
    return std::abs(a.r - b.r) < EPSILON && std::abs(a.g - b.g) < EPSILON &&
           std::abs(a.b - b.b) < EPSILON;
  }
};

// Vertex structure for particle quad
struct ParticleVertex {
  glm::vec2 position; // Quad vertex position
  glm::vec2 texCoord; // Texture coordinates
};

// Instance data structure for each particle
struct ParticleInstance {
  glm::vec2 worldPosition; // World position of particle
  float radius;            // Particle radius
  float active;            // Active flag (0.0 or 1.0)
  glm::vec3 color;         // Particle color
  float padding;           // Padding for alignment
};

class Particle {
public:
  Particle();
  Particle(const Particle &other);
  Particle &operator=(const Particle &other);
  ~Particle();

  void cleanup();
  void update(float deltaTime);
  static void updateAll(std::vector<Particle> &particles, float deltaTime);

  // WebGPU-specific initialization
  static void initializeSharedResources(WGPUDevice device, WGPUTextureFormat swapChainFormat);
  static void cleanupSharedResources();
  static void renderAll(const glm::mat4 &projection, WGPURenderPassEncoder renderPass);
  static void updateAllInstanceData();

  static void initInteractionMatrix(int numTypes);
  static void randomizeInteractionMatrix();
  static float calculateForce(float r_norm, float a);

  // setters
  void setPos(const glm::vec2 &pos) {
    this->position = pos;
    updateInstanceData();
  }

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

  void setVel(const glm::vec2 &vel) { this->velocity = vel; }
  void setAcc(const glm::vec2 &acc) { this->acceleration = acc; }

  // getters
  [[nodiscard]] glm::vec2 getPos() const { return this->position; }
  [[nodiscard]] glm::vec2 getVel() const { return this->velocity; }
  [[nodiscard]] glm::vec2 getAcc() const { return this->acceleration; }
  [[nodiscard]] float getSize() const { return this->radius; }
  [[nodiscard]] glm::vec3 getColor() const { return this->color; }
  [[nodiscard]] bool isActive() const { return this->active; }

  [[nodiscard]] int getType() const {
    static thread_local std::unordered_map<glm::vec3, int, ColorHash> colorTypeCache;
    auto it = colorTypeCache.find(color);
    if (it != colorTypeCache.end()) {
      return it->second;
    }
    int closestColorIndex = 0;
    float minDistance = std::numeric_limits<float>::max();

    for (int i = 0; static_cast<size_t>(i) < simulation::COLORS.size() && i < numParticleTypes;
         ++i) {
      const auto &refColor = simulation::COLORS[i];
      float dx = color.r - refColor.r;
      float dy = color.g - refColor.g;
      float dz = color.b - refColor.b;
      float distanceSquared = (dx * dx) + (dy * dy) + (dz * dz);

      if (distanceSquared < minDistance) {
        minDistance = distanceSquared;
        closestColorIndex = i;
      }

      if (minDistance < 0.0001F) {
        break;
      }
    }

    if (colorTypeCache.size() > 100) {
      colorTypeCache.clear();
    }
    colorTypeCache[color] = closestColorIndex;

    return closestColorIndex;
  }

  void setType(int type) {
    if (type >= 0 && type < numParticleTypes &&
        static_cast<size_t>(type) < simulation::COLORS.size()) {
      this->color = simulation::COLORS[type];
      updateInstanceData();
    }
  }

  static float getInteractionStrength(int type1, int type2) {
    if (interactionCache.size != numParticleTypes) {
      interactionCache.size = numParticleTypes;
      for (int i = 0; i < numParticleTypes && i < 16; i++) {
        for (int j = 0; j < numParticleTypes && j < 16; j++) {
          interactionCache.values[i][j] = interactionMatrix[i][j];
        }
      }
    }

    if (type1 >= 0 && type1 < numParticleTypes && type1 < 16 && type2 >= 0 &&
        type2 < numParticleTypes && type2 < 16) {
      return interactionCache.values[type1][type2];
    }
    return 0.0F;
  }

  static void setInteractionStrength(int type1, int type2, float strength) {
    if (type1 >= 0 && type1 < numParticleTypes && type2 >= 0 && type2 < numParticleTypes) {
      interactionMatrix[type1][type2] = strength;
    }
  }

  static void setNumParticleTypes(int num) {
    numParticleTypes = num;
    initInteractionMatrix(numParticleTypes);
  }

  static int getNumParticleTypes() { return numParticleTypes; }
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

  void updateInstanceData();

  // WebGPU resources (replacing OpenGL equivalents)
  static WGPUDevice device;
  static WGPUBuffer vertexBuffer;   // Quad vertices (replaces quadVAO/quadVBO)
  static WGPUBuffer indexBuffer;    // Quad indices
  static WGPUBuffer instanceBuffer; // Instance data (replaces instanceVBO)
  static WGPURenderPipeline renderPipeline;
  static std::unique_ptr<Shader> particleShader;
  static std::vector<ParticleInstance> instanceData;
  static bool initialized;
  static size_t particleCount;
  static const size_t MAX_PARTICLES;

  static std::vector<std::vector<float>> interactionMatrix;
  static int numParticleTypes;
  static float interactionRadius;
  static float frictionFactor;
  static const float BETA;

  // WebGPU helper methods
  static void createVertexBuffer();
  static void createInstanceBuffer();
  static void createRenderPipeline(WGPUTextureFormat swapChainFormat);
};
