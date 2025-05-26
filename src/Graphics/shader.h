// shader.h
#pragma once
#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>
#include <glm/glm.hpp>
#include <string>
#include <string_view>
#include <webgpu/webgpu.h>

class Shader {
public:
  // Constructor now takes shader names instead of file paths
  Shader(const std::string &vertexShaderName, const std::string &fragmentShaderName,
         WGPUDevice device);
  ~Shader();

  WGPUShaderModule getVertexModule() const { return vertexModule; }
  WGPUShaderModule getFragmentModule() const { return fragmentModule; }

  void reload();

  // Uniform setting functions
  void setBool(std::string_view name, bool v);
  void setInt(std::string_view name, int v);
  void setFloat(std::string_view name, float v);
  void setVec3(std::string_view name, const glm::vec3 &v);
  void setMat4(std::string_view name, const glm::mat4 &m);

  // WebGPU-specific functions
  WGPUBindGroup getUniformBindGroup() const { return uniformBindGroup; }
  WGPUBindGroupLayout getBindGroupLayout() const { return bindGroupLayout; }
  void updateUniforms();
  WGPUBindGroupLayout &getBindGroupLayoutRef();

private:
  WGPUDevice device;
  WGPUShaderModule vertexModule;
  WGPUShaderModule fragmentModule;
  WGPUBuffer transformBuffer; // For matrices
  WGPUBuffer materialBuffer;  // For colors and other material properties
  WGPUBindGroup uniformBindGroup;
  WGPUBindGroupLayout bindGroupLayout;

  // Store shader names instead of paths
  std::string vertexShaderName, fragmentShaderName;

  // Separate uniform data structures for better alignment
  struct TransformUniforms {
    glm::mat4 model;
    glm::mat4 projection;
    float padding[32]; // Extra space for future use
  } transformData;

  struct MaterialUniforms {
    glm::vec3 color;
    float alpha;
    glm::vec3 padding1;
    float padding2;
    float extraFloats[12]; // Extra space for future material properties
  } materialData;

  WGPUShaderModule createShaderModule(const std::string &shaderName);
  void createUniformBuffers();
  void createBindGroup();
  static std::string loadShaderSource(const std::string &shaderName);
};
