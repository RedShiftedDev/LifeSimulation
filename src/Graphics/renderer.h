#pragma once
#include <glm/glm.hpp>
#include <memory>
#include <webgpu/webgpu.h>
#include "shader.h"

class Renderer {
public:
  Renderer(WGPUDevice device, WGPUTextureFormat swapChainFormat);
  ~Renderer();

  void init();
  void beginRender(WGPURenderPassEncoder renderPass);
  void endRender();

  void setProjectionMatrix(const glm::mat4 &projection) { projectionMatrix = projection; }

  void drawRect(float x, float y, float width, float height, const glm::vec3 &color);
  void drawCircle(float x, float y, float radius, const glm::vec3 &color, int segments = 32);
  void drawLine(float x1, float y1, float x2, float y2, const glm::vec3 &color,
                float thickness = 1.0F);

private:
  WGPUDevice device;
  WGPUTextureFormat swapChainFormat;
  WGPURenderPassEncoder currentRenderPass;

  std::unique_ptr<Shader> shader2D;

  // WebGPU resources for rectangle
  WGPUBuffer rectVertexBuffer;
  WGPUBuffer rectIndexBuffer;
  WGPURenderPipeline rectPipeline;

  // WebGPU resources for circle
  WGPUBuffer circleVertexBuffer;
  WGPURenderPipeline circlePipeline;

  // WebGPU resources for line
  WGPUBuffer lineVertexBuffer;
  WGPURenderPipeline linePipeline;

  glm::mat4 projectionMatrix{1.0F};

  // Helper methods to set up buffers and pipelines
  void setupRectBuffer();
  void setupCircleBuffer();
  void setupLineBuffer();
  void createRenderPipelines();

  WGPURenderPipeline createRenderPipeline(WGPUPrimitiveTopology topology, const char *label);
};
