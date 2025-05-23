#include "Particle.h"
#include <omp.h>
#ifdef __ARM_NEON
#include <arm_neon.h>
#endif
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <random>
#include <vector>
#include "Graphics/Simulation.h"

unsigned int Particle::instanceVBO = 0;
unsigned int Particle::quadVAO = 0;
unsigned int Particle::quadVBO = 0;
std::unique_ptr<Shader> Particle::particleShader = nullptr;
std::vector<glm::vec4> Particle::instanceData;
bool Particle::initialized = false;
size_t Particle::particleCount = 0;
const size_t Particle::MAX_PARTICLES = 1000000;
thread_local std::unordered_map<glm::vec3, int, ColorHash, ColorEqual> colorTypeCache;
alignas(16) std::vector<std::vector<float>> Particle::interactionMatrix;
alignas(4) int Particle::numParticleTypes = 6;                              // Default to 6 types
alignas(4) float Particle::frictionFactor = std::pow(0.5F, 0.02F / 0.040F); // Friction factor
alignas(4) const float Particle::BETA = 0.3F;                               // Repulsion parameter

Particle::Particle()
    : position(0.0F), velocity(0.0F), acceleration(0.0F), radius(5.0F), color(1.0F), active(true) {

  if (!initialized) {
    initializeSharedResources();
  }

  particleIndex = particleCount++;
  if (particleCount > MAX_PARTICLES) {
    // Handle error: too many particles
    active = false;
  }

  if (active) {
    updateInstanceData();
  }
}

Particle::Particle(const Particle &other)
    : position(other.position), velocity(other.velocity), acceleration(other.acceleration),
      radius(other.radius), color(other.color), active(other.active) {

  particleIndex = particleCount++;
  if (particleCount > MAX_PARTICLES) {
    // Handle error: too many particles
    active = false;
  }

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

    if (active) {
      updateInstanceData();
    }
  }
  return *this;
}

Particle::~Particle() { cleanup(); }

void Particle::initializeSharedResources() {
  particleShader = std::make_unique<Shader>(
      "../../../../src/Graphics/shaders/particle.vert",
      "../../../../src/Graphics/shaders/particle.frag");

  float quadVertices[] = {-0.5F, 0.5F,  0.0F, 0.0F, 1.0F, 0.5F,  0.5F,  0.0F, 1.0F, 1.0F,
                          0.5F,  -0.5F, 0.0F, 1.0F, 0.0F, -0.5F, -0.5F, 0.0F, 0.0F, 0.0F};

  GLuint indices[] = {
      0, 1, 2, // first triangle
      2, 3, 0  // second triangle
  };
  glGenVertexArrays(1, &quadVAO);
  glGenBuffers(1, &quadVBO);
  GLuint EBO;
  glGenBuffers(1, &EBO);
  glBindVertexArray(quadVAO);
  glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)(3 * sizeof(float)));
  glEnableVertexAttribArray(1);
  instanceData.resize(MAX_PARTICLES * 2, glm::vec4(0.0F));
  glGenBuffers(1, &instanceVBO);
  glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
  glBufferData(GL_ARRAY_BUFFER, MAX_PARTICLES * 2 * sizeof(glm::vec4), instanceData.data(),
               GL_DYNAMIC_DRAW);
  glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 2 * sizeof(glm::vec4), (void *)0);
  glEnableVertexAttribArray(2);
  glVertexAttribDivisor(2, 1);
  glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, 2 * sizeof(glm::vec4),
                        (void *)(sizeof(glm::vec4)));
  glEnableVertexAttribArray(3);
  glVertexAttribDivisor(3, 1);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);
  initInteractionMatrix(numParticleTypes);
  initialized = true;
}

void Particle::cleanupSharedResources() {
  if (initialized) {
    glDeleteVertexArrays(1, &quadVAO);
    glDeleteBuffers(1, &quadVBO);
    glDeleteBuffers(1, &instanceVBO);
    particleShader.reset();
    instanceData.clear();
    interactionMatrix.clear();
    initialized = false;
    particleCount = 0;
  }
}

void Particle::cleanup() {
  active = false;
  updateInstanceData();
}

void Particle::updateInstanceData() {
  if (particleIndex < MAX_PARTICLES) {
    instanceData[particleIndex * 2] =
        glm::vec4(position.x, position.y, radius, active ? 1.0F : 0.0F);
    instanceData[(particleIndex * 2) + 1] = glm::vec4(color, 0.0F);
  }
}

void Particle::updateAllInstanceData() {
  if (initialized) {
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
    size_t dataSize = particleCount * 2 * sizeof(glm::vec4);
    glBufferSubData(GL_ARRAY_BUFFER, 0, dataSize, instanceData.data());
    glBindBuffer(GL_ARRAY_BUFFER, 0);
  }
}

void Particle::renderAll(const glm::mat4 &projection) {
  if (!initialized || particleCount == 0) {
    return;
  }
  updateAllInstanceData();
  particleShader->use();
  static GLint projectionLoc = -1;

  if (projectionLoc == -1) {
    projectionLoc = glGetUniformLocation(particleShader->getProgramID(), "projection");
  }

  if (projectionLoc != -1) {
    glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, &projection[0][0]);
  }
  glBindVertexArray(quadVAO);
  glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0, particleCount);
  glBindVertexArray(0);
}

void Particle::initInteractionMatrix(int numTypes) {
  numParticleTypes = numTypes;
  interactionMatrix.clear();
  interactionMatrix.reserve(numTypes);
  for (int i = 0; i < numTypes; ++i) {
    interactionMatrix.emplace_back(numTypes, 0.0F);
  }
  randomizeInteractionMatrix();
}

void Particle::randomizeInteractionMatrix() {
  std::random_device rd;
  std::vector<std::mt19937> gens;
  gens.reserve(numParticleTypes);
  for (int t = 0; t < omp_get_max_threads(); ++t) {
    gens.emplace_back(rd() ^ (t * 0x9e3779b97f4a7c15ULL));
  }

#pragma omp parallel for collapse(2) schedule(static)
  for (int i = 0; i < numParticleTypes; ++i) {
    for (int j = 0; j < numParticleTypes; ++j) {
      auto &gen = gens[omp_get_thread_num()];
      std::uniform_real_distribution<float> dist(-1.0F, 1.0F);
      interactionMatrix[i][j] = dist(gen);
    }
  }
}

float Particle::calculateForce(float r_norm, float a) {
#ifdef __ARM_NEON
  float32x2_t beta_vec = vdup_n_f32(BETA);
  float32x2_t one_vec = vdup_n_f32(1.0F);
  float32x2_t input_vec = vdup_n_f32(r_norm);
  float32x2_t a_vec = vdup_n_f32(a);
  float32x2_t two_vec = vdup_n_f32(2.0F);
  float32x2_t repulsion = vsub_f32(vdiv_f32(input_vec, beta_vec), one_vec);
  float32x2_t inner_term = vsub_f32(vsub_f32(vmul_f32(two_vec, input_vec), one_vec), beta_vec);
  float32x2_t abs_inner = vabs_f32(inner_term);
  float32x2_t one_minus_beta = vsub_f32(one_vec, beta_vec);
  float32x2_t attraction = vmul_f32(a_vec, vsub_f32(one_vec, vdiv_f32(abs_inner, one_minus_beta)));
  uint32x2_t case1 = vclt_f32(input_vec, beta_vec);
  uint32x2_t case2 = vand_u32(vcgt_f32(input_vec, beta_vec), vclt_f32(input_vec, one_vec));
  float32x2_t result = vbsl_f32(case1, repulsion, vbsl_f32(case2, attraction, vdup_n_f32(0.0F)));
  return vget_lane_f32(result, 0);
#else // Scalar implementation for non-ARM platforms
  if (r_norm < BETA) {
    return (r_norm / BETA) - 1.0f;
  } else if (r_norm < 1.0f) {
    float inner_term = 2.0f * r_norm - 1.0f - BETA;
    return a * (1.0f - (std::abs(inner_term) / (1.0f - BETA)));
  } else {
    return 0.0f;
  }
#endif
}

void Particle::update(float deltaTime) {
  if (!active) {
    return;
  }

#ifdef __ARM_NEON
  float32x2_t pos = vld1_f32(&position.x);
  float32x2_t vel = vld1_f32(&velocity.x);

  if (__builtin_expect(static_cast<long>(simulation::enableBounds), 1) != 0) {
    const float32x2_t bounds = {
        ((simulation::boundaryRight - simulation::boundaryLeft) / 2) - radius,
        ((simulation::boundaryBottom - simulation::boundaryTop) / 2) - radius};

    uint32x2_t over_bounds = vcgt_f32(pos, bounds);
    uint32x2_t under_bounds = vclt_f32(pos, vneg_f32(bounds));

    uint32x2_t out_of_bounds = vorr_u32(over_bounds, under_bounds);

    float32x2_t pos_corrected =
        vbsl_f32(over_bounds, bounds, vbsl_f32(under_bounds, vneg_f32(bounds), pos));

    const float32x2_t bounce_factor = vdup_n_f32(-0.9F);
    float32x2_t vel_corrected =
        vmul_f32(vel, vbsl_f32(out_of_bounds, bounce_factor, vdup_n_f32(1.0F)));

    pos = pos_corrected;
    vel = vel_corrected;
  }

  vel = vmul_n_f32(vel, frictionFactor);

#ifdef __ARM_FEATURE_FMA
  pos = vfma_n_f32(pos, vel, deltaTime);
#else
  pos = vadd_f32(pos, vmul_n_f32(vel, deltaTime));
#endif

  vst1_f32(&position.x, pos);
  vst1_f32(&velocity.x, vel);

#else // Scalar implementation for non-ARM platforms
  if (simulation::enableBounds) {
    const float boundX = ((simulation::boundaryRight - simulation::boundaryLeft) / 2) - radius;
    const float boundY = ((simulation::boundaryBottom - simulation::boundaryTop) / 2) - radius;

    if (position.x > boundX || position.x < -boundX) {
      position.x = (position.x > boundX) ? boundX : -boundX;
      velocity.x *= -0.9F;
    }

    if (position.y > boundY || position.y < -boundY) {
      position.y = (position.y > boundY) ? boundY : -boundY;
      velocity.y *= -0.9F;
    }
  }

  velocity *= frictionFactor;
  position += velocity * deltaTime;
#endif

  static float timeAccumulator = 0.0F;
  timeAccumulator += deltaTime;

  if (timeAccumulator >= 0.016666667F) {
    updateInstanceData();
    timeAccumulator = 0.0F;
  }
}

void Particle::updateAll(std::vector<Particle> &particles, float deltaTime) {
#pragma omp parallel for schedule(static)
  for (size_t i = 0; i < particles.size(); ++i) {
    particles[i].update(deltaTime);
  }
}
