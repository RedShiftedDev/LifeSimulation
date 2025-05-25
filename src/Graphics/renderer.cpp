#include "renderer.h"
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <stdexcept>

Renderer::Renderer(WGPUDevice device, WGPUTextureFormat swapChainFormat)
    : device(device), swapChainFormat(swapChainFormat), currentRenderPass(nullptr) {
  init();
}

Renderer::~Renderer() {
  if (rectVertexBuffer != nullptr) {
    wgpuBufferRelease(rectVertexBuffer);
  }
  if (rectIndexBuffer != nullptr) {
    wgpuBufferRelease(rectIndexBuffer);
  }
  if (circleVertexBuffer != nullptr) {
    wgpuBufferRelease(circleVertexBuffer);
  }
  if (lineVertexBuffer != nullptr) {
    wgpuBufferRelease(lineVertexBuffer);
  }

  if (rectPipeline != nullptr) {
    wgpuRenderPipelineRelease(rectPipeline);
  }
  if (circlePipeline != nullptr) {
    wgpuRenderPipelineRelease(circlePipeline);
  }
  if (linePipeline != nullptr) {
    wgpuRenderPipelineRelease(linePipeline);
  }
}

void Renderer::init() {
  shader2D =
      std::make_unique<Shader>("../../../../src/Graphics/shaders/shader2D.vert.wgsl",
                               "../../../../src/Graphics/shaders/shader2D.frag.wgsl", device);

  setupRectBuffer();
  setupCircleBuffer();
  setupLineBuffer();
  createRenderPipelines();
}

void Renderer::setupRectBuffer() {
  float vertices[] = {
      0.5F,  0.5F,  0.0F, // top right
      0.5F,  -0.5F, 0.0F, // bottom right
      -0.5F, -0.5F, 0.0F, // bottom left
      -0.5F, 0.5F,  0.0F  // top left
  };
  uint16_t indices[] = {
      0, 1, 3, // first triangle
      1, 2, 3  // second triangle
  };

  WGPUBufferDescriptor vertexBufferDesc = {};
  vertexBufferDesc.label = "Rectangle Vertex Buffer";
  vertexBufferDesc.size = sizeof(vertices);
  vertexBufferDesc.usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst;
  vertexBufferDesc.mappedAtCreation = 0;

  rectVertexBuffer = wgpuDeviceCreateBuffer(device, &vertexBufferDesc);
  if (rectVertexBuffer == nullptr) {
    throw std::runtime_error("Failed to create rectangle vertex buffer");
  }
  wgpuQueueWriteBuffer(wgpuDeviceGetQueue(device), rectVertexBuffer, 0, vertices, sizeof(vertices));

  WGPUBufferDescriptor indexBufferDesc = {};
  indexBufferDesc.label = "Rectangle Index Buffer";
  indexBufferDesc.size = sizeof(indices);
  indexBufferDesc.usage = WGPUBufferUsage_Index | WGPUBufferUsage_CopyDst;
  indexBufferDesc.mappedAtCreation = 0;

  rectIndexBuffer = wgpuDeviceCreateBuffer(device, &indexBufferDesc);
  if (rectIndexBuffer == nullptr) {
    throw std::runtime_error("Failed to create rectangle index buffer");
  }
  wgpuQueueWriteBuffer(wgpuDeviceGetQueue(device), rectIndexBuffer, 0, indices, sizeof(indices));
}

void Renderer::setupCircleBuffer() {
  size_t maxVertices = (32 + 2) * 3;

  WGPUBufferDescriptor bufferDesc = {};
  bufferDesc.label = "Circle Vertex Buffer"; // Added label
  bufferDesc.size = maxVertices * sizeof(float);
  bufferDesc.usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst;
  bufferDesc.mappedAtCreation = 0; // Correct, not mapped at creation

  circleVertexBuffer = wgpuDeviceCreateBuffer(device, &bufferDesc);
  if (circleVertexBuffer == nullptr) { // Added null check
    throw std::runtime_error("Failed to create circle vertex buffer");
  }
}

void Renderer::setupLineBuffer() {
  // Initial vertices are written here, but it's updated per draw call.
  // So, using wgpuQueueWriteBuffer for the initial state is consistent.
  float vertices[] = {
      0.0F, 0.0F, 0.0F, // start point
      1.0F, 0.0F, 0.0F  // end point
  };

  WGPUBufferDescriptor bufferDesc = {};
  bufferDesc.label = "Line Vertex Buffer"; // Added label
  bufferDesc.size = sizeof(vertices);      // Initial size for 2 points
  // Max size if lines can be longer? For now, this is fine, updated per draw.
  // If you intended this buffer to hold arbitrary line lengths without re-creation,
  // it would need to be larger. But current drawLine updates with specific length.
  bufferDesc.usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst;
  bufferDesc.mappedAtCreation = 0; // Correct

  lineVertexBuffer = wgpuDeviceCreateBuffer(device, &bufferDesc);
  if (!lineVertexBuffer) { // Added null check
    throw std::runtime_error("Failed to create line vertex buffer");
  }
  // Initialize with default line (0,0) to (1,0)
  wgpuQueueWriteBuffer(wgpuDeviceGetQueue(device), lineVertexBuffer, 0, vertices, sizeof(vertices));
}

void Renderer::createRenderPipelines() {
  rectPipeline = createRenderPipeline(WGPUPrimitiveTopology_TriangleList, "Rectangle Pipeline");
  circlePipeline = createRenderPipeline(WGPUPrimitiveTopology_TriangleStrip, "Circle Pipeline");
  linePipeline = createRenderPipeline(WGPUPrimitiveTopology_LineList, "Line Pipeline");
}

WGPURenderPipeline Renderer::createRenderPipeline(WGPUPrimitiveTopology topology,
                                                  const char *label) {
  // Vertex buffer layout
  WGPUVertexAttribute vertexAttribute = {};
  vertexAttribute.format = WGPUVertexFormat_Float32x3;
  vertexAttribute.offset = 0;
  vertexAttribute.shaderLocation = 0;

  WGPUVertexBufferLayout vertexBufferLayout = {};
  vertexBufferLayout.arrayStride = 3 * sizeof(float);
  vertexBufferLayout.stepMode = WGPUVertexStepMode_Vertex;
  vertexBufferLayout.attributeCount = 1;
  vertexBufferLayout.attributes = &vertexAttribute;

  // Vertex state
  WGPUVertexState vertexState = {};
  vertexState.module = shader2D->getVertexModule();
  vertexState.entryPoint = "vs_main";
  vertexState.bufferCount = 1;
  vertexState.buffers = &vertexBufferLayout;

  // Fragment state
  WGPUBlendState blendState = {};
  blendState.color.srcFactor = WGPUBlendFactor_SrcAlpha;
  blendState.color.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
  blendState.color.operation = WGPUBlendOperation_Add;
  blendState.alpha.srcFactor = WGPUBlendFactor_One;
  blendState.alpha.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
  blendState.alpha.operation = WGPUBlendOperation_Add;

  WGPUColorTargetState colorTarget = {};
  colorTarget.format = swapChainFormat;
  colorTarget.blend = &blendState;
  colorTarget.writeMask = WGPUColorWriteMask_All;

  WGPUFragmentState fragmentState = {};
  fragmentState.module = shader2D->getFragmentModule();
  fragmentState.entryPoint = "fs_main";
  fragmentState.targetCount = 1;
  fragmentState.targets = &colorTarget;

  // Primitive state
  WGPUPrimitiveState primitiveState = {};
  primitiveState.topology = topology;
  primitiveState.stripIndexFormat = WGPUIndexFormat_Undefined;
  primitiveState.frontFace = WGPUFrontFace_CCW;
  primitiveState.cullMode = WGPUCullMode_None;

  // Pipeline layout
  WGPUPipelineLayoutDescriptor layoutDesc = {};
  layoutDesc.bindGroupLayoutCount = 1;
  layoutDesc.bindGroupLayouts = &shader2D->getBindGroupLayoutRef();

  WGPUPipelineLayout pipelineLayout = wgpuDeviceCreatePipelineLayout(device, &layoutDesc);

  // Render pipeline
  WGPURenderPipelineDescriptor pipelineDesc = {};
  pipelineDesc.label = label;
  pipelineDesc.layout = pipelineLayout;
  pipelineDesc.vertex = vertexState;
  pipelineDesc.primitive = primitiveState;
  pipelineDesc.fragment = &fragmentState;

  WGPUMultisampleState multisampleState = {};
  multisampleState.count = 1;                  // Specify 1 for no multisampling
  multisampleState.mask = 0xFFFFFFFF;          // Default mask
  multisampleState.alphaToCoverageEnabled = 0; // Default
  pipelineDesc.multisample = multisampleState;

  WGPURenderPipeline pipeline = wgpuDeviceCreateRenderPipeline(device, &pipelineDesc);

  wgpuPipelineLayoutRelease(pipelineLayout);

  if (pipeline == nullptr) {
    throw std::runtime_error("Failed to create render pipeline");
  }

  return pipeline;
}

void Renderer::beginRender(WGPURenderPassEncoder renderPass) { currentRenderPass = renderPass; }

void Renderer::endRender() { currentRenderPass = nullptr; }

void Renderer::drawRect(float x, float y, float width, float height, const glm::vec3 &color) {
  if (currentRenderPass == nullptr) {
    return;
  }

  // Set uniforms
  shader2D->setVec3("color", color);

  // Create model matrix for positioning and scaling
  glm::mat4 model = glm::mat4(1.0F);
  model = glm::translate(model, glm::vec3(x + (width / 2), y + (height / 2), 0.0F));
  model = glm::scale(model, glm::vec3(width, height, 1.0F));

  shader2D->setMat4("model", model);
  shader2D->setMat4("projection", projectionMatrix);
  shader2D->updateUniforms();

  // Set pipeline and bind resources
  wgpuRenderPassEncoderSetPipeline(currentRenderPass, rectPipeline);
  wgpuRenderPassEncoderSetBindGroup(currentRenderPass, 0, shader2D->getUniformBindGroup(), 0,
                                    nullptr);
  wgpuRenderPassEncoderSetVertexBuffer(currentRenderPass, 0, rectVertexBuffer, 0, WGPU_WHOLE_SIZE);
  wgpuRenderPassEncoderSetIndexBuffer(currentRenderPass, rectIndexBuffer, WGPUIndexFormat_Uint16, 0,
                                      WGPU_WHOLE_SIZE);

  // Draw
  wgpuRenderPassEncoderDrawIndexed(currentRenderPass, 6, 1, 0, 0, 0);
}

void Renderer::drawCircle(float x, float y, float radius, const glm::vec3 &color, int segments) {
  if (currentRenderPass == nullptr) {
    return;
  }

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
  wgpuQueueWriteBuffer(wgpuDeviceGetQueue(device), circleVertexBuffer, 0, vertices.data(),
                       vertices.size() * sizeof(float));

  // Set uniforms
  shader2D->setVec3("color", color);

  // Create model matrix for positioning and scaling
  glm::mat4 model = glm::mat4(1.0F);
  model = glm::translate(model, glm::vec3(x, y, 0.0F));
  model = glm::scale(model, glm::vec3(radius, radius, 1.0F));

  shader2D->setMat4("model", model);
  shader2D->setMat4("projection", projectionMatrix);
  shader2D->updateUniforms();

  // Set pipeline and bind resources
  wgpuRenderPassEncoderSetPipeline(currentRenderPass, circlePipeline);
  wgpuRenderPassEncoderSetBindGroup(currentRenderPass, 0, shader2D->getUniformBindGroup(), 0,
                                    nullptr);
  wgpuRenderPassEncoderSetVertexBuffer(currentRenderPass, 0, circleVertexBuffer, 0,
                                       WGPU_WHOLE_SIZE);

  // Draw circle as triangle fan (using triangle strip in WebGPU)
  wgpuRenderPassEncoderDraw(currentRenderPass, segments + 2, 1, 0, 0);
}

void Renderer::drawLine(float x1, float y1, float x2, float y2, const glm::vec3 &color,
                        [[maybe_unused]] float thickness) {
  if (currentRenderPass == nullptr) {
    return;
  }

  // Calculate direction vector
  float dx = x2 - x1;
  float dy = y2 - y1;
  float length = sqrtf((dx * dx) + (dy * dy));

  // Update line vertices
  float vertices[] = {0.0F, 0.0F, 0.0F, length, 0.0F, 0.0F};
  wgpuQueueWriteBuffer(wgpuDeviceGetQueue(device), lineVertexBuffer, 0, vertices, sizeof(vertices));

  // Set uniforms
  shader2D->setVec3("color", color);

  // Create model matrix for positioning and rotation
  glm::mat4 model = glm::mat4(1.0F);
  model = glm::translate(model, glm::vec3(x1, y1, 0.0F));

  // Calculate rotation angle
  float angle = atan2f(dy, dx);
  model = glm::rotate(model, angle, glm::vec3(0.0F, 0.0F, 1.0F));

  shader2D->setMat4("model", model);
  shader2D->setMat4("projection", projectionMatrix);
  shader2D->updateUniforms();

  // Set pipeline and bind resources
  wgpuRenderPassEncoderSetPipeline(currentRenderPass, linePipeline);
  wgpuRenderPassEncoderSetBindGroup(currentRenderPass, 0, shader2D->getUniformBindGroup(), 0,
                                    nullptr);
  wgpuRenderPassEncoderSetVertexBuffer(currentRenderPass, 0, lineVertexBuffer, 0, WGPU_WHOLE_SIZE);

  // Draw line (Note: WebGPU doesn't support line thickness like OpenGL)
  wgpuRenderPassEncoderDraw(currentRenderPass, 2, 1, 0, 0);
}
