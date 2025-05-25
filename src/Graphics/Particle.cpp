#include "Particle.h"
#include <iostream>
#include <omp.h>
#ifdef __ARM_NEON
#include <arm_neon.h>
#endif
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <random>
#include <vector>
#include "Graphics/Simulation.h"

// WebGPU static members
WGPUDevice Particle::device = nullptr;
WGPUBuffer Particle::vertexBuffer = nullptr;
WGPUBuffer Particle::indexBuffer = nullptr;
WGPUBuffer Particle::instanceBuffer = nullptr;
WGPURenderPipeline Particle::renderPipeline = nullptr;
std::unique_ptr<Shader> Particle::particleShader = nullptr;
std::vector<ParticleInstance> Particle::instanceData;
bool Particle::initialized = false;
size_t Particle::particleCount = 0;
const size_t Particle::MAX_PARTICLES = 1000000;

thread_local std::unordered_map<glm::vec3, int, ColorHash, ColorEqual> colorTypeCache;
alignas(16) std::vector<std::vector<float>> Particle::interactionMatrix;
alignas(4) int Particle::numParticleTypes = 6;                              // Default to 6 types
alignas(4) float Particle::frictionFactor = std::pow(0.5F, 0.02F / 0.040F); // Friction factor
alignas(4) const float Particle::BETA = 0.3F;                               // Repulsion parameter
float Particle::interactionRadius = 80.0F; // Default interaction radius

Particle::Particle()
    : position(0.0F), velocity(0.0F), acceleration(0.0F), radius(5.0F), color(1.0F), active(true) {

  if (!initialized) {
    // Note: WebGPU resources need to be initialized externally via initializeSharedResources
    // This is because we need the device and swap chain format
  }

  particleIndex = particleCount++;
  if (particleCount > MAX_PARTICLES) {
    // Handle error: too many particles
    active = false;
  }

  if (active && initialized) {
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

  if (active && initialized) {
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

    if (active && initialized) {
      updateInstanceData();
    }
  }
  return *this;
}

Particle::~Particle() { cleanup(); }

void Particle::initializeSharedResources(WGPUDevice dev, WGPUTextureFormat swapChainFormat) {
  if (initialized) {
    std::cout << "Particle::initializeSharedResources: Already initialized. Skipping." << std::endl;
    return;
  }
  device = dev;

  try {
    particleShader =
        std::make_unique<Shader>("../../../../src/Graphics/shaders/particle.vert.wgsl",
                                 "../../../../src/Graphics/shaders/particle.frag.wgsl", device);

    if (!particleShader || !particleShader->getVertexModule() ||
        !particleShader->getFragmentModule()) {
      throw std::runtime_error("Particle shader module creation failed or modules are null.");
    }
    if (!particleShader->getBindGroupLayout()) {
      throw std::runtime_error("Particle shader bind group layout is null.");
    }
    if (!particleShader->getUniformBindGroup()) {
      throw std::runtime_error("Particle shader uniform bind group is null.");
    }

    createVertexBuffer();
    createInstanceBuffer();
    createRenderPipeline(swapChainFormat);

    if (!renderPipeline) {
      throw std::runtime_error("Particle render pipeline is null after creation steps.");
    }

    // Initialize the matrix structure here. Randomization will be called separately.
    Particle::initInteractionMatrix(Particle::numParticleTypes); // Pass the static member

  } catch (const std::runtime_error &e) {
    std::cerr << "FATAL ERROR during Particle::initializeSharedResources: " << e.what()
              << std::endl;
    // ... (cleanup code from your previous version) ...
    particleShader.reset();
    if (vertexBuffer) {
      wgpuBufferRelease(vertexBuffer);
      vertexBuffer = nullptr;
    }
    if (indexBuffer) {
      wgpuBufferRelease(indexBuffer);
      indexBuffer = nullptr;
    }
    if (instanceBuffer) {
      wgpuBufferRelease(instanceBuffer);
      instanceBuffer = nullptr;
    }
    if (renderPipeline) {
      wgpuRenderPipelineRelease(renderPipeline);
      renderPipeline = nullptr;
    }
    device = nullptr;
    initialized = false;
    throw;
  }
  initialized = true;
}

void Particle::createVertexBuffer() {
  // Quad vertices (position + texCoord)
  ParticleVertex quadVertices[] = {
      {{-0.5F, 0.5F}, {0.0F, 1.0F}}, // Top-left
      {{0.5F, 0.5F}, {1.0F, 1.0F}},  // Top-right
      {{0.5F, -0.5F}, {1.0F, 0.0F}}, // Bottom-right
      {{-0.5F, -0.5F}, {0.0F, 0.0F}} // Bottom-left
  };

  WGPUBufferDescriptor vertexBufferDesc = {};
  vertexBufferDesc.label = "Particle Vertex Buffer";
  vertexBufferDesc.size = sizeof(quadVertices);
  vertexBufferDesc.usage = WGPUBufferUsage_Vertex;
  vertexBufferDesc.mappedAtCreation = 1; // Changed from 0 to 1
  vertexBuffer = wgpuDeviceCreateBuffer(device, &vertexBufferDesc);

  void *vertexData = wgpuBufferGetMappedRange(vertexBuffer, 0, sizeof(quadVertices));
  memcpy(vertexData, quadVertices, sizeof(quadVertices));
  wgpuBufferUnmap(vertexBuffer);

  // Index buffer for quad
  uint16_t indices[] = {0, 1, 2, 2, 3, 0};

  WGPUBufferDescriptor indexBufferDesc = {};
  indexBufferDesc.label = "Particle Index Buffer";
  indexBufferDesc.size = sizeof(indices);
  indexBufferDesc.usage = WGPUBufferUsage_Index;
  indexBufferDesc.mappedAtCreation = 1; // Changed from 0 to 1
  indexBuffer = wgpuDeviceCreateBuffer(device, &indexBufferDesc);

  void *indexData = wgpuBufferGetMappedRange(indexBuffer, 0, sizeof(indices));
  memcpy(indexData, indices, sizeof(indices));
  wgpuBufferUnmap(indexBuffer);
}
void Particle::createInstanceBuffer() {
  instanceData.resize(MAX_PARTICLES);

  WGPUBufferDescriptor instanceBufferDesc = {};
  instanceBufferDesc.label = "Particle Instance Buffer";
  instanceBufferDesc.size = MAX_PARTICLES * sizeof(ParticleInstance);
  instanceBufferDesc.usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst;
  instanceBufferDesc.mappedAtCreation = 0;
  instanceBuffer = wgpuDeviceCreateBuffer(device, &instanceBufferDesc);
}

// Particle.cpp

void Particle::createRenderPipeline(WGPUTextureFormat swapChainFormat) {
  // Vertex attributes (existing code is likely fine)
  WGPUVertexAttribute vertexAttributes[4] = {};
  // ... (setup vertexAttributes[0] through vertexAttributes[3]) ...
  // Position attribute (location 0)
  vertexAttributes[0].format = WGPUVertexFormat_Float32x2;
  vertexAttributes[0].offset = offsetof(ParticleVertex, position);
  vertexAttributes[0].shaderLocation = 0;

  // TexCoord attribute (location 1)
  vertexAttributes[1].format = WGPUVertexFormat_Float32x2;
  vertexAttributes[1].offset = offsetof(ParticleVertex, texCoord);
  vertexAttributes[1].shaderLocation = 1;

  // Instance worldPosition, radius, active (location 2)
  // Note: ParticleInstance has worldPosition (vec2), radius (float), active (float)
  // This is effectively a vec4.
  vertexAttributes[2].format = WGPUVertexFormat_Float32x4; // Correct for vec2 + float + float
  vertexAttributes[2].offset = offsetof(ParticleInstance, worldPosition); // Starts at worldPosition
  vertexAttributes[2].shaderLocation = 2;

  // Instance color + padding (location 3)
  // Note: ParticleInstance has color (vec3), padding (float)
  // This is effectively a vec4.
  vertexAttributes[3].format = WGPUVertexFormat_Float32x4;        // Correct for vec3 + float
  vertexAttributes[3].offset = offsetof(ParticleInstance, color); // Starts at color
  vertexAttributes[3].shaderLocation = 3;

  // Vertex buffer layouts (existing code is likely fine)
  WGPUVertexBufferLayout vertexBufferLayouts[2] = {};
  // Per-vertex data
  vertexBufferLayouts[0].arrayStride = sizeof(ParticleVertex);
  vertexBufferLayouts[0].stepMode = WGPUVertexStepMode_Vertex;
  vertexBufferLayouts[0].attributeCount = 2; // position, texCoord
  vertexBufferLayouts[0].attributes = &vertexAttributes[0];

  // Per-instance data
  vertexBufferLayouts[1].arrayStride = sizeof(ParticleInstance);
  vertexBufferLayouts[1].stepMode = WGPUVertexStepMode_Instance;
  vertexBufferLayouts[1].attributeCount = 2; // (worldPosition,radius,active), (color,padding)
  vertexBufferLayouts[1].attributes = &vertexAttributes[2]; // Start from attribute for location 2

  // Vertex state
  WGPUVertexState vertexState = {};
  vertexState.module = particleShader->getVertexModule();
  vertexState.entryPoint = "vs_main";
  vertexState.bufferCount = 2;
  vertexState.buffers = vertexBufferLayouts;

  // Fragment state (existing code is likely fine)
  WGPUBlendState blendState = {};
  blendState.color.srcFactor = WGPUBlendFactor_SrcAlpha;
  blendState.color.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
  blendState.color.operation = WGPUBlendOperation_Add;
  blendState.alpha.srcFactor = WGPUBlendFactor_One; // Often SrcAlpha or One for particles
  blendState.alpha.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
  blendState.alpha.operation = WGPUBlendOperation_Add;

  WGPUColorTargetState colorTarget = {};
  colorTarget.format = swapChainFormat;
  colorTarget.blend = &blendState;
  colorTarget.writeMask = WGPUColorWriteMask_All;

  WGPUFragmentState fragmentState = {};
  fragmentState.module = particleShader->getFragmentModule();
  fragmentState.entryPoint = "fs_main";
  fragmentState.targetCount = 1;
  fragmentState.targets = &colorTarget;

  // Pipeline descriptor
  WGPURenderPipelineDescriptor pipelineDesc = {};
  pipelineDesc.label = "Particle Render Pipeline";
  pipelineDesc.vertex = vertexState;
  pipelineDesc.fragment = &fragmentState;
  pipelineDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
  pipelineDesc.primitive.cullMode = WGPUCullMode_None; // Typical for 2D particles

  // --- FIX 1: Add Multisample State ---
  WGPUMultisampleState multisampleState = {};
  multisampleState.count = 1; // Must be 1 or more. 1 means no MSAA.
  multisampleState.mask = 0xFFFFFFFF;
  multisampleState.alphaToCoverageEnabled = 0;
  pipelineDesc.multisample = multisampleState;

  // --- FIX 2: Create and Set Pipeline Layout ---
  // Make sure your Shader class has a getter for its WGPUBindGroupLayout
  // e.g., WGPUBindGroupLayout getBindGroupLayout() const { return bindGroupLayout; }
  WGPUBindGroupLayout pBGL = particleShader->getBindGroupLayout();
  if (pBGL == nullptr) {
    std::cerr << "FATAL: Particle shader bind group layout is null!" << std::endl;
    // Handle error: throw, exit, or prevent pipeline creation
    return;
  }

  WGPUPipelineLayoutDescriptor pipelineLayoutDesc = {};
  pipelineLayoutDesc.label = "Particle Pipeline Layout";
  pipelineLayoutDesc.bindGroupLayoutCount = 1;
  pipelineLayoutDesc.bindGroupLayouts = &pBGL; // Pass address of the BGL handle

  WGPUPipelineLayout pl = wgpuDeviceCreatePipelineLayout(device, &pipelineLayoutDesc);
  if (pl == nullptr) {
    std::cerr << "FATAL: Failed to create particle pipeline layout!" << std::endl;
    // Handle error
    return;
  }
  pipelineDesc.layout = pl;

  // Create the render pipeline
  renderPipeline = wgpuDeviceCreateRenderPipeline(device, &pipelineDesc);

  // Release the pipeline layout once the pipeline is created
  if (pl != nullptr) {
    wgpuPipelineLayoutRelease(pl);
  }

  // --- FIX 3: Check for pipeline creation failure ---
  if (renderPipeline == nullptr) {
    std::cerr
        << "FATAL: Failed to create particle render pipeline! Check shader paths and WGSL validity."
        << std::endl;
    // This is critical. If renderPipeline is null, Particle::renderAll will crash.
    // Consider throwing an exception or exiting.
  }
  if (particleShader->getVertexModule() == nullptr ||
      particleShader->getFragmentModule() == nullptr) {
    std::cerr << "FATAL: Particle shader modules not loaded correctly. Check paths: "
              << "../../../../src/Graphics/shaders/particle.vert.wgsl and "
              << "../../../../src/Graphics/shaders/particle.frag.wgsl" << std::endl;
    // Ensure Shader class constructor or createShaderModule handles errors if files are not found
    // or WGSL is invalid.
  }
}

void Particle::cleanupSharedResources() {
  if (initialized) {
    if (renderPipeline != nullptr) {
      wgpuRenderPipelineRelease(renderPipeline);
    }
    if (instanceBuffer != nullptr) {
      wgpuBufferRelease(instanceBuffer);
    }
    if (indexBuffer != nullptr) {
      wgpuBufferRelease(indexBuffer);
    }
    if (vertexBuffer != nullptr) {
      wgpuBufferRelease(vertexBuffer);
    }

    renderPipeline = nullptr;
    instanceBuffer = nullptr;
    indexBuffer = nullptr;
    vertexBuffer = nullptr;

    particleShader.reset();
    instanceData.clear();
    interactionMatrix.clear();
    initialized = false;
    particleCount = 0;
    device = nullptr;
  }
}

void Particle::cleanup() {
  active = false;
  if (initialized) {
    updateInstanceData();
  }
}

void Particle::updateInstanceData() {
  if (particleIndex < MAX_PARTICLES && initialized) {
    instanceData[particleIndex].worldPosition = position;
    instanceData[particleIndex].radius = radius;
    instanceData[particleIndex].active = active ? 1.0F : 0.0F;
    instanceData[particleIndex].color = color;
    instanceData[particleIndex].padding = 0.0F;
  }
}

void Particle::updateAllInstanceData() {
  if (initialized && particleCount > 0) {
    size_t dataSize = particleCount * sizeof(ParticleInstance);
    wgpuQueueWriteBuffer(wgpuDeviceGetQueue(device), instanceBuffer, 0, instanceData.data(),
                         dataSize);
  }
}

// Particle.cpp
void Particle::renderAll(const glm::mat4 &projection, WGPURenderPassEncoder renderPass) {
  if (!initialized || particleCount == 0) {
    return;
  }
  if (!renderPipeline) {
    std::cerr << "Particle::renderAll: Skipping render, renderPipeline is null." << std::endl;
    return;
  }
  if (!particleShader) {
    std::cerr << "Particle::renderAll: Skipping render, particleShader is null." << std::endl;
    return;
  }
  WGPUBindGroup currentBindGroup = particleShader->getUniformBindGroup();
  if (!currentBindGroup) {
    std::cerr << "Particle::renderAll: Skipping render, particleShader's uniformBindGroup is null."
              << std::endl;
    return;
  }
  if (!vertexBuffer || !indexBuffer || !instanceBuffer) {
    std::cerr << "Particle::renderAll: Skipping render, one or more particle buffers are null."
              << std::endl;
    return;
  }

  updateAllInstanceData(); // This writes to instanceBuffer

  wgpuRenderPassEncoderSetPipeline(renderPass, renderPipeline);

  particleShader->setMat4("projection", projection);
  particleShader->updateUniforms(); // This writes to the shader's uniform buffers

  wgpuRenderPassEncoderSetBindGroup(renderPass, 0, currentBindGroup, 0, nullptr);

  wgpuRenderPassEncoderSetVertexBuffer(renderPass, 0, vertexBuffer, 0, WGPU_WHOLE_SIZE);
  wgpuRenderPassEncoderSetVertexBuffer(renderPass, 1, instanceBuffer, 0, WGPU_WHOLE_SIZE);
  wgpuRenderPassEncoderSetIndexBuffer(renderPass, indexBuffer, WGPUIndexFormat_Uint16, 0,
                                      WGPU_WHOLE_SIZE);
  wgpuRenderPassEncoderDrawIndexed(renderPass, 6, particleCount, 0, 0, 0);
}

void Particle::initInteractionMatrix(int num_types_to_init) { // Renamed param to avoid confusion
  // Update the static member if a new size is requested,
  // otherwise, it uses its current static value (e.g. default 6)
  Particle::numParticleTypes = num_types_to_init;

  if (Particle::numParticleTypes <= 0) {
    std::cerr
        << "Warning: initInteractionMatrix called with numParticleTypes <= 0. Matrix will be empty."
        << std::endl;
    interactionMatrix.clear(); // Ensure it's empty
    return;
  }

  std::cout << "DEBUG Particle: Initializing interaction matrix with " << Particle::numParticleTypes
            << " types." << std::endl;
  interactionMatrix.clear();                            // Clear previous state
  interactionMatrix.resize(Particle::numParticleTypes); // Resize outer vector
  for (int i = 0; i < Particle::numParticleTypes; ++i) {
    interactionMatrix[i].resize(Particle::numParticleTypes, 0.0F); // Resize inner vectors
  }
  std::cout << "DEBUG Particle: Interaction matrix structure created." << std::endl;
  // DO NOT call randomizeInteractionMatrix() here if it's called from ParticleSystem
}

void Particle::randomizeInteractionMatrix() {
  // Safety check: ensure matrix is actually sized.
  if (Particle::numParticleTypes <= 0 || interactionMatrix.empty() ||
      interactionMatrix.size() != static_cast<size_t>(Particle::numParticleTypes) ||
      interactionMatrix[0].size() != static_cast<size_t>(Particle::numParticleTypes)) {
    std::cerr << "Error: Particle::randomizeInteractionMatrix called but matrix is not properly "
                 "initialized to size "
              << Particle::numParticleTypes << "x" << Particle::numParticleTypes << "."
              << std::endl;
    std::cerr << "Matrix actual size: " << interactionMatrix.size() << "x"
              << (interactionMatrix.empty() ? 0 : interactionMatrix[0].size()) << std::endl;
    return;
  }
  std::cout << "DEBUG Particle: Randomizing interaction matrix..." << std::endl;

  std::random_device rd;
  std::vector<std::mt19937> gens;
  int max_threads = omp_get_max_threads();
  gens.reserve(max_threads);
  for (int t = 0; t < max_threads; ++t) {
    gens.emplace_back(rd() ^ (static_cast<uint64_t>(t) * 0x9e3779b97f4a7c15ULL));
  }

#pragma omp parallel for collapse(2) schedule(static)
  for (int i = 0; i < Particle::numParticleTypes; ++i) {
    for (int j = 0; j < Particle::numParticleTypes; ++j) {
      int thread_idx = omp_get_thread_num();
      // Basic safety for gens access, though ideally thread_idx should always be < gens.size()
      if (thread_idx >= static_cast<int>(gens.size())) {
        thread_idx = thread_idx % gens.size();
      }

      auto &gen = gens[thread_idx];
      std::uniform_real_distribution<float> dist(-1.0F, 1.0F);
      // This is the line that crashed:
      interactionMatrix[i][j] = dist(gen);
    }
  }
  std::cout << "DEBUG Particle: Interaction matrix randomized." << std::endl;
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
    if (initialized) {
      updateInstanceData();
    }
    timeAccumulator = 0.0F;
  }
}

void Particle::updateAll(std::vector<Particle> &particles, float deltaTime) {
#pragma omp parallel for schedule(static)
  for (size_t i = 0; i < particles.size(); ++i) {
    particles[i].update(deltaTime);
  }
}
