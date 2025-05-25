#pragma once

#include <mutex>
#include <vector>
#include <webgpu/webgpu.h>
#include "Particle.h"

class ParticleSystem {
public:
  explicit ParticleSystem(size_t maxParticles = 1000000);
  ~ParticleSystem();

  void update(float deltaTime);
  static void render(const glm::mat4 &projection, WGPURenderPassEncoder renderPass);

  // WebGPU initialization - call this after WebGPU device is created
  static void initializeWebGPU(WGPUDevice device, WGPUTextureFormat swapChainFormat);

  // Force calculation for particle interactions
  void calculateInteractionForces(float deltaTime);
  void simplifiedForceCalculation(float deltaTime);
  static float getInteractionStrength(int type1, int type2);
  static void randomizeInteractions();

  // Particle creation
  Particle &createParticle();

  // System management
  void clear();
  size_t getParticleCount() const;
  size_t getActiveParticleCount() const;

  // Configuration
  void setAutoRemoveInactive(bool value) { autoRemoveInactive = value; }
  bool getAutoRemoveInactive() const { return autoRemoveInactive; }

private:
  std::vector<Particle> particles;
  size_t maxParticles;
  size_t nextParticleIndex = 0;
  bool autoRemoveInactive = true;
  const float R_MAX = 60.0F;
  const float invRMax = 1.0F / R_MAX;
  const float gridCellSize = R_MAX;
  const float R_MAX_SQR = R_MAX * R_MAX;
  static std::vector<glm::vec2> previousForces;
  static std::mutex previousForcesMutex;

  void populateSpatialGrid(std::vector<std::vector<size_t>> &grid,
                           std::vector<size_t> &activeParticles, std::vector<int> &cellX,
                           std::vector<int> &cellY, int gridWidth, int gridHeight);
  void computeInteractionForcesOMP(const std::vector<std::vector<size_t>> &grid,
                                   const std::vector<size_t> &activeParticles,
                                   const std::vector<int> &cellX, const std::vector<int> &cellY,
                                   std::vector<glm::vec2> &forceBuffer, int gridWidth,
                                   int gridHeight);
  void applyForcesOMP(const std::vector<size_t> &activeParticles,
                      const std::vector<glm::vec2> &forceBuffer, float deltaTime);
  bool shouldComputeForce(size_t i, size_t j, const std::vector<Particle> &particles,
                          const glm::vec2 &dist, float &invDist, float &normDist,
                          float &interaction, float &forceMag, int type_i) const;
};
