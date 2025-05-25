// shader.cpp
#include "shader.h"
#include <cstring> // For std::memset
#include <fstream>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <sstream>
#include <stdexcept> // For std::runtime_error

Shader::Shader(std::filesystem::path vertexPath, std::filesystem::path fragmentPath,
               WGPUDevice device)
    : device(device), vertexModule(nullptr), fragmentModule(nullptr), transformBuffer(nullptr),
      materialBuffer(nullptr), // Initialize to nullptr
      uniformBindGroup(nullptr), bindGroupLayout(nullptr), vertexPath(std::move(vertexPath)),
      fragmentPath(std::move(fragmentPath)) // Initialize to nullptr
{

  // Initialize uniform data to zero or default values
  std::memset(&transformData, 0, sizeof(transformData));
  std::memset(&materialData, 0, sizeof(materialData));

  // Set default values
  transformData.model = glm::mat4(1.0f);
  transformData.projection = glm::mat4(1.0f);
  materialData.color = glm::vec3(1.0f);
  materialData.alpha = 1.0f;

  // Create shader modules
  this->vertexModule = createShaderModule(this->vertexPath);
  this->fragmentModule = createShaderModule(this->fragmentPath);

  // --- CRITICAL: Check if shader modules were created successfully ---
  if (this->vertexModule == nullptr || this->fragmentModule == nullptr) {
    std::string errorMsg = "Shader Creation Failed: ";
    if (this->vertexModule == nullptr) {
      errorMsg += "Could not create vertex shader module from " + this->vertexPath.string() + ". ";
    }
    if (this->fragmentModule == nullptr) {
      errorMsg +=
          "Could not create fragment shader module from " + this->fragmentPath.string() + ". ";
    }
    // Release any partially created modules before throwing
    if (this->vertexModule != nullptr) {
      wgpuShaderModuleRelease(this->vertexModule);
      this->vertexModule = nullptr;
    }
    if (this->fragmentModule != nullptr) {
      wgpuShaderModuleRelease(this->fragmentModule);
      this->fragmentModule = nullptr;
    }
    throw std::runtime_error(errorMsg);
  }
  // --- END CRITICAL CHECK ---

  // Create uniform buffers and bind group
  // These functions already throw on failure, which is good.
  createUniformBuffers();
  createBindGroup();
}

Shader::~Shader() {
  if (uniformBindGroup != nullptr) { // Release bind group first
    wgpuBindGroupRelease(uniformBindGroup);
  }
  if (bindGroupLayout != nullptr) { // Then layout
    wgpuBindGroupLayoutRelease(bindGroupLayout);
  }
  if (transformBuffer != nullptr) {
    wgpuBufferRelease(transformBuffer);
  }
  if (materialBuffer != nullptr) {
    wgpuBufferRelease(materialBuffer);
  }
  if (vertexModule != nullptr) {
    wgpuShaderModuleRelease(vertexModule);
  }
  if (fragmentModule != nullptr) {
    wgpuShaderModuleRelease(fragmentModule);
  }
}

void Shader::reload() {
  // Release old resources (bind group and layout might need to be recreated if shaders change
  // significantly) For simplicity, we are only reloading modules here. If bindings change, more is
  // needed.
  if (vertexModule != nullptr) {
    wgpuShaderModuleRelease(vertexModule);
    vertexModule = nullptr;
  }
  if (fragmentModule != nullptr) {
    wgpuShaderModuleRelease(fragmentModule);
    fragmentModule = nullptr;
  }

  // Recreate modules
  vertexModule = createShaderModule(vertexPath);
  fragmentModule = createShaderModule(fragmentPath);

  if (vertexModule == nullptr || fragmentModule == nullptr) {
    std::cerr << "Error: Shader reload failed. Modules could not be recreated." << std::endl;
    // Potentially revert to old modules or handle error state
    // For now, just log. The next render call might fail.
  }
  // Note: If shader bindings change, you'd need to recreate bindGroupLayout and uniformBindGroup.
  // And any pipelines using this shader would need to be recreated.
}

void Shader::setBool(std::string_view name, bool value) { setFloat(name, value ? 1.0F : 0.0F); }

void Shader::setInt(std::string_view name, int value) { setFloat(name, static_cast<float>(value)); }

void Shader::setFloat(std::string_view name, float value) {
  if (name == "alpha") {
    materialData.alpha = value;
  }
  // Add more float uniforms as needed
}

void Shader::setVec3(std::string_view name, const glm::vec3 &value) {
  if (name == "color") {
    materialData.color = value;
  }
  // Add more vec3 uniforms as needed
}

void Shader::setMat4(std::string_view name, const glm::mat4 &mat) {
  if (name == "model") {
    transformData.model = mat;
  } else if (name == "projection") {
    transformData.projection = mat;
  }
  // Add more matrix uniforms as needed
}

void Shader::updateUniforms() {
  if (transformBuffer == nullptr || materialBuffer == nullptr || device == nullptr) {
    std::cerr << "Shader::updateUniforms called with null buffer or device." << std::endl;
    return;
  }
  WGPUQueue queue = wgpuDeviceGetQueue(device); // Get queue here or pass it in
  if (queue == nullptr) {
    std::cerr << "Shader::updateUniforms: Failed to get device queue." << std::endl;
    return;
  }

  wgpuQueueWriteBuffer(queue, transformBuffer, 0, &transformData, sizeof(transformData));
  wgpuQueueWriteBuffer(queue, materialBuffer, 0, &materialData, sizeof(materialData));
  // No need to release queue if obtained via wgpuDeviceGetQueue directly like this
}

WGPUShaderModule Shader::createShaderModule(const std::filesystem::path &path) {
  std::string source;
  try {
    source = loadShaderSource(path);
  } catch (const std::runtime_error &e) {
    std::cerr << "Error loading shader source for " << path << ": " << e.what() << std::endl;
    return nullptr;
  }

  if (source.empty()) {
    std::cerr << "Error: Shader source file is empty: " << path << std::endl;
    return nullptr;
  }

  WGPUShaderModuleWGSLDescriptor wgslDesc = {};
  wgslDesc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
  wgslDesc.code = source.c_str();

  std::string labelStr = path.filename().string(); // Ensure labelStr lives long enough
  WGPUShaderModuleDescriptor moduleDesc = {};
  moduleDesc.nextInChain = &wgslDesc.chain;
  moduleDesc.label = labelStr.c_str();

  WGPUShaderModule module = wgpuDeviceCreateShaderModule(device, &moduleDesc);
  if (module == nullptr) {
    std::cerr << "Error: wgpuDeviceCreateShaderModule failed for: " << path << std::endl;
    // Additional error information might be available via device error callbacks
  }
  return module;
}

void Shader::createUniformBuffers() {
  WGPUBufferDescriptor transformBufferDesc = {};
  transformBufferDesc.label = "Transform Uniform Buffer";
  transformBufferDesc.size = sizeof(transformData);
  transformBufferDesc.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;
  transformBufferDesc.mappedAtCreation = false; // Standard practice unless pre-filling

  transformBuffer = wgpuDeviceCreateBuffer(device, &transformBufferDesc);
  if (transformBuffer == nullptr) {
    throw std::runtime_error("Failed to create transform uniform buffer");
  }

  WGPUBufferDescriptor materialBufferDesc = {};
  materialBufferDesc.label = "Material Uniform Buffer";
  materialBufferDesc.size = sizeof(materialData);
  materialBufferDesc.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;
  materialBufferDesc.mappedAtCreation = false;

  materialBuffer = wgpuDeviceCreateBuffer(device, &materialBufferDesc);
  if (materialBuffer == nullptr) {
    // Clean up previously created buffer before throwing
    wgpuBufferRelease(transformBuffer);
    transformBuffer = nullptr;
    throw std::runtime_error("Failed to create material uniform buffer");
  }
}

void Shader::createBindGroup() {
  WGPUBindGroupLayoutEntry bindingLayouts[2] = {};

  bindingLayouts[0].binding = 0;
  bindingLayouts[0].visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
  bindingLayouts[0].buffer.type = WGPUBufferBindingType_Uniform;
  bindingLayouts[0].buffer.hasDynamicOffset = false; // Default
  bindingLayouts[0].buffer.minBindingSize = sizeof(transformData);

  bindingLayouts[1].binding = 1;
  bindingLayouts[1].visibility = WGPUShaderStage_Fragment; // Only fragment for material typically
  bindingLayouts[1].buffer.type = WGPUBufferBindingType_Uniform;
  bindingLayouts[1].buffer.hasDynamicOffset = false; // Default
  bindingLayouts[1].buffer.minBindingSize = sizeof(materialData);

  WGPUBindGroupLayoutDescriptor layoutDesc = {};
  layoutDesc.label = "Shader Bind Group Layout";
  layoutDesc.entryCount = 2;
  layoutDesc.entries = bindingLayouts;

  bindGroupLayout = wgpuDeviceCreateBindGroupLayout(device, &layoutDesc);
  if (bindGroupLayout == nullptr) {
    throw std::runtime_error("Failed to create bind group layout");
  }

  WGPUBindGroupEntry bindings[2] = {};
  bindings[0].binding = 0;
  bindings[0].buffer = transformBuffer;
  bindings[0].offset = 0;
  bindings[0].size = sizeof(transformData);

  bindings[1].binding = 1;
  bindings[1].buffer = materialBuffer;
  bindings[1].offset = 0;
  bindings[1].size = sizeof(materialData);

  WGPUBindGroupDescriptor bindGroupDesc = {};
  bindGroupDesc.label = "Shader Bind Group";
  bindGroupDesc.layout = bindGroupLayout;
  bindGroupDesc.entryCount = 2;
  bindGroupDesc.entries = bindings;

  uniformBindGroup = wgpuDeviceCreateBindGroup(device, &bindGroupDesc);
  if (uniformBindGroup == nullptr) {
    // Clean up previously created layout before throwing
    wgpuBindGroupLayoutRelease(bindGroupLayout);
    bindGroupLayout = nullptr;
    throw std::runtime_error("Failed to create bind group");
  }
}

// Existing non-const getter (can be kept if modification is intended, though rare for BGL)
WGPUBindGroupLayout &Shader::getBindGroupLayoutRef() { return bindGroupLayout; }

// New const getter as recommended for Particle::createRenderPipeline
// This is defined in shader.h already, but ensure it's consistent:
// WGPUBindGroupLayout Shader::getBindGroupLayout() const { return bindGroupLayout; }
// (Implementation would be in the .cpp if not inline in .h)

std::string Shader::loadShaderSource(const std::filesystem::path &path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    // Changed to throw, so createShaderModule can catch it.
    throw std::runtime_error("Failed to open shader file: " + path.string());
  }
  std::stringstream stream;
  stream << file.rdbuf();
  return stream.str();
}
