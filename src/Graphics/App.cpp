#include "App.h"
#include "Common.h"
#include "Graphics/ParticleSystem.h"
#include "Graphics/Simulation.h"
#include "Graphics/renderer.h"
#include "core/fps_counter.h"
#include "core/window.h"

#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_wgpu.h>
#include <glfw3webgpu.h>
#include <imgui.h>

#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <stdexcept>

App::App(const char *windowTitle, int windowWidth, int windowHeight)
    : m_paused(false), m_randomizeOnStart(true) {
  m_window = std::make_unique<Window>(windowTitle, windowWidth, windowHeight);
  m_fpsCounter = std::make_unique<FpsCounter>();
}

App::~App() { cleanup(); }

void App::run() {
  try {
    init();
    mainLoop();
  } catch (const std::exception &e) {
    cleanup();
  }
}

void App::init() {
  initWebGPU();
  initImGui();
  initApplicationLogic();
}

void App::initWebGPU() {
  WGPUInstanceDescriptor instanceDesc = {};
  m_instance = wgpuCreateInstance(&instanceDesc);
  if (m_instance == nullptr) {
    throw std::runtime_error("Failed to create WebGPU instance.");
  }

  m_window->createSurface(m_instance);
  m_surface = m_window->getWGPUSurface();
  if (m_surface == nullptr) {
    throw std::runtime_error("Failed to get WebGPU surface from window object.");
  }

  WGPURequestAdapterOptions adapterOpts = {};
  adapterOpts.compatibleSurface = m_surface;
  m_adapter = requestAdapter(m_instance, &adapterOpts); // Uses static member
  if (m_adapter == nullptr) {
    throw std::runtime_error("Failed to request WebGPU adapter.");
  }

  WGPUDeviceDescriptor deviceDesc = {};
  deviceDesc.label = "Primary Device";
  deviceDesc.defaultQueue.label = "Default Queue";
  m_device = requestDevice(m_adapter, &deviceDesc); // Uses static member
  if (m_device == nullptr) {
    throw std::runtime_error("Failed to request WebGPU device.");
  }
  wgpuDeviceSetUncapturedErrorCallback(m_device, onDeviceError, nullptr); // Uses static member
  m_queue = wgpuDeviceGetQueue(m_device);

  m_surfaceFormat = wgpuSurfaceGetPreferredFormat(m_surface, m_adapter);
  if (m_surfaceFormat == WGPUTextureFormat_Undefined) {
    m_surfaceFormat = WGPUTextureFormat_BGRA8Unorm;
  }

  int initialWidth;
  int initialHeight;
  m_window->getFramebufferSize(&initialWidth, &initialHeight);

  m_surfaceConfig.device = m_device;
  m_surfaceConfig.format = m_surfaceFormat;
  m_surfaceConfig.usage = WGPUTextureUsage_RenderAttachment;
  // A more robust fallback for initialWidth/Height if window isn't fully ready or headless
  m_surfaceConfig.width = (initialWidth > 0) ? initialWidth : 800;
  m_surfaceConfig.height = (initialHeight > 0) ? initialHeight : 600;
  m_surfaceConfig.presentMode = WGPUPresentMode_Fifo;
  m_surfaceConfig.alphaMode = WGPUCompositeAlphaMode_Opaque;
  wgpuSurfaceConfigure(m_surface, &m_surfaceConfig);
}

void App::initImGui() {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

  ImGui::StyleColorsDark();

  ImGui_ImplGlfw_InitForOther(m_window->getGLFWWindow(), true);
  ImGui_ImplWGPU_InitInfo init_info = {};
  init_info.Device = m_device;
  init_info.NumFramesInFlight = 3;
  init_info.RenderTargetFormat = m_surfaceFormat;
  init_info.DepthStencilFormat = WGPUTextureFormat_Undefined;
  ImGui_ImplWGPU_Init(&init_info);
}

void App::initApplicationLogic() {
  m_renderer2D = std::make_unique<Renderer>(m_device, m_surfaceFormat);
  m_projectionMatrix = calculateProjectionMatrix(m_surfaceConfig.width, m_surfaceConfig.height);
  m_renderer2D->setProjectionMatrix(m_projectionMatrix);

  m_particleSystem = std::make_unique<ParticleSystem>(1000000);
  ParticleSystem::initializeWebGPU(m_device, m_surfaceFormat);

  if (m_randomizeOnStart) {
    ParticleSystem::randomizeInteractions();
  }
}

void App::mainLoop() {
  auto lastFrameTime = std::chrono::high_resolution_clock::now();

  while (!m_window->shouldClose()) {
    auto currentFrameTime = std::chrono::high_resolution_clock::now();
    float rawDeltaTime = std::chrono::duration<float>(currentFrameTime - lastFrameTime).count();
    lastFrameTime = currentFrameTime;

    float deltaTime = std::min(rawDeltaTime * simulation::simulationSpeed, 0.05F);

    m_window->pollEvents();
    if (m_window->wasResized()) {
      handleResize();
    }

    update(deltaTime); // gui::RenderGui is called inside update()
    render();

    float currentFps = m_fpsCounter->update();
    m_window->updateTitle(currentFps);
  }
}

void App::update(float deltaTime) {
  ImGui_ImplWGPU_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  gui::RenderGui(*m_fpsCounter);

  if (!m_paused) {
    m_particleSystem->update(deltaTime);
  }
}

void App::render() {
  WGPUSurfaceTexture surfaceTexture;
  wgpuSurfaceGetCurrentTexture(m_surface, &surfaceTexture);

  if (surfaceTexture.status != WGPUSurfaceGetCurrentTextureStatus_Success) {
    if (surfaceTexture.status == WGPUSurfaceGetCurrentTextureStatus_Lost ||
        surfaceTexture.status == WGPUSurfaceGetCurrentTextureStatus_Outdated) {
      int currentWidth;
      int currentHeight;
      m_window->getFramebufferSize(&currentWidth, &currentHeight);
      if (currentWidth > 0 && currentHeight > 0) {
        m_surfaceConfig.width = currentWidth;
        m_surfaceConfig.height = currentHeight;
      }

      wgpuSurfaceConfigure(m_surface, &m_surfaceConfig);
    }
    if (surfaceTexture.texture != nullptr) {
      wgpuTextureRelease(surfaceTexture.texture);
    }
    return;
  }

  WGPUTextureViewDescriptor viewDesc = {};
  viewDesc.format = m_surfaceConfig.format;
  viewDesc.dimension = WGPUTextureViewDimension_2D;
  viewDesc.mipLevelCount = 1;
  viewDesc.arrayLayerCount = 1;
  viewDesc.aspect = WGPUTextureAspect_All;
  WGPUTextureView currentTextureView = wgpuTextureCreateView(surfaceTexture.texture, &viewDesc);
  if (currentTextureView == nullptr) {
    if (surfaceTexture.texture != nullptr) {
      wgpuTextureRelease(surfaceTexture.texture);
    }
    return;
  }

  WGPUCommandEncoderDescriptor encoderDesc = {};
  encoderDesc.label = "Main Command Encoder";
  WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(m_device, &encoderDesc);

  WGPURenderPassColorAttachment colorAttachment = {};
  colorAttachment.view = currentTextureView;
  colorAttachment.loadOp = WGPULoadOp_Clear;
  colorAttachment.storeOp = WGPUStoreOp_Store;
  colorAttachment.clearValue = {::BackgroundColour.r, ::BackgroundColour.g, ::BackgroundColour.b,
                                ::BackgroundColour.a};

  WGPURenderPassDescriptor renderPassDesc = {};
  renderPassDesc.label = "Main Render Pass";
  renderPassDesc.colorAttachmentCount = 1;
  renderPassDesc.colorAttachments = &colorAttachment;

  WGPURenderPassEncoder renderPass = wgpuCommandEncoderBeginRenderPass(encoder, &renderPassDesc);

  wgpuRenderPassEncoderSetViewport(renderPass, 0.0F, 0.0F,
                                   static_cast<float>(m_surfaceConfig.width),
                                   static_cast<float>(m_surfaceConfig.height), 0.0F, 1.0F);
  wgpuRenderPassEncoderSetScissorRect(renderPass, 0, 0, m_surfaceConfig.width,
                                      m_surfaceConfig.height);

  ParticleSystem::render(m_projectionMatrix, renderPass);

  m_renderer2D->beginRender(renderPass);
  // m_renderer2D->drawRect(0, 0, 50, 50, {1, 0, 0});
  m_renderer2D->endRender();

  ImGui::Render();
  ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), renderPass);

  wgpuRenderPassEncoderEnd(renderPass);
  wgpuRenderPassEncoderRelease(renderPass);

  WGPUCommandBufferDescriptor cmdBufDesc = {};
  cmdBufDesc.label = "Main Command Buffer";
  WGPUCommandBuffer commandBuffer = wgpuCommandEncoderFinish(encoder, &cmdBufDesc);
  wgpuCommandEncoderRelease(encoder);

  wgpuQueueSubmit(m_queue, 1, &commandBuffer);
  wgpuCommandBufferRelease(commandBuffer);

  wgpuSurfacePresent(m_surface);

  wgpuTextureViewRelease(currentTextureView);
  if (surfaceTexture.texture != nullptr) {
    wgpuTextureRelease(surfaceTexture.texture);
  }
}

void App::handleResize() {
  int newWidth;
  int newHeight;
  m_window->getFramebufferSize(&newWidth, &newHeight);
  if (newWidth > 0 && newHeight > 0) {
    m_surfaceConfig.width = newWidth;
    m_surfaceConfig.height = newHeight;
    wgpuSurfaceConfigure(m_surface, &m_surfaceConfig);
    m_projectionMatrix = calculateProjectionMatrix(newWidth, newHeight);
    if (m_renderer2D) {
      m_renderer2D->setProjectionMatrix(m_projectionMatrix);
    }
  }
  m_window->resetResizedFlag();
}

glm::mat4 App::calculateProjectionMatrix(int windowWidth, int windowHeight) {
  const float referenceWidth = 1280.0F;
  const float referenceHeight = 720.0F;
  const float targetAspectRatio = referenceWidth / referenceHeight;

  float projectionWidth = referenceWidth;
  float projectionHeight = referenceHeight;

  if (windowWidth > 0 && windowHeight > 0) {
    float currentAspectRatio = static_cast<float>(windowWidth) / static_cast<float>(windowHeight);
    if (currentAspectRatio > targetAspectRatio) {
      projectionWidth = projectionHeight * currentAspectRatio;
    } else {
      projectionHeight = projectionWidth / currentAspectRatio;
    }
  }
  return glm::ortho(-projectionWidth / 2.0F, projectionWidth / 2.0F, -projectionHeight / 2.0F,
                    projectionHeight / 2.0F, -1.0F, 1.0F);
}

void App::emitParticlesAtPosition(const glm::vec2 &position, int count, int type) {
  if (!m_particleSystem) {
    return;
  }
  for (int i = 0; i < count; ++i) {
    Particle &p = m_particleSystem->createParticle();
    p.setPos(position + glm::vec2(static_cast<float>((rand() % 20) - 10),
                                  static_cast<float>((rand() % 20) - 10)));
    p.setVel({0.0F, 0.0F});
    p.setRadius(5.0F);
    if (type != -1) {
      p.setType(type);
    } else {
      p.setType(rand() % Particle::getNumParticleTypes());
    }
    p.setActive(true);
  }
}

void App::cleanup() {
  if (ImGui::GetCurrentContext() != nullptr) {
    ImGui_ImplWGPU_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
  }

  m_particleSystem.reset();
  m_renderer2D.reset();

  if (m_queue != nullptr) {
    wgpuQueueRelease(m_queue);
  }
  m_queue = nullptr;
  if (m_device != nullptr) {
    wgpuDeviceRelease(m_device);
  }
  m_device = nullptr;
  if (m_adapter != nullptr) {
    wgpuAdapterRelease(m_adapter);
  }
  m_adapter = nullptr;
  // m_surface is released by m_window's destructor
  if (m_instance != nullptr) {
    wgpuInstanceRelease(m_instance);
  }
  m_instance = nullptr;
}

// --- Static Helper Functions Implementation ---
// These are part of the App class but are static
void App::onDeviceError(WGPUErrorType type, char const *message, [[maybe_unused]] void *userdata) {
  std::cerr << "Uncaptured WebGPU device error: type " << static_cast<int>(type);
  if (message != nullptr) {
    std::cerr << " (" << message << ")";
  }
  std::cerr << std::endl;
}

WGPUAdapter App::requestAdapter(WGPUInstance instance, WGPURequestAdapterOptions const *options) {
  // ... (implementation as previously provided) ...
  struct UserData {
    WGPUAdapter adapter = nullptr;
    bool requestEnded = false;
  };
  UserData userData;
  auto onAdapterRequestEnded = [](WGPURequestAdapterStatus status, WGPUAdapter adapter,
                                  char const *message, void *pUserData) {
    UserData &data = *reinterpret_cast<UserData *>(pUserData);
    if (status == WGPURequestAdapterStatus_Success) {
      data.adapter = adapter;
    } else {
      std::cerr << "Could not get WebGPU adapter: " << message << std::endl;
    }
    data.requestEnded = true;
  };
  wgpuInstanceRequestAdapter(instance, options, onAdapterRequestEnded, &userData);
  while (!userData.requestEnded) {
  } // Simplified busy wait
  return userData.adapter;
}

WGPUDevice App::requestDevice(WGPUAdapter adapter, WGPUDeviceDescriptor const *descriptor) {
  // ... (implementation as previously provided) ...
  struct UserData {
    WGPUDevice device = nullptr;
    bool requestEnded = false;
  };
  UserData userData;
  auto onDeviceRequestEnded = [](WGPURequestDeviceStatus status, WGPUDevice device,
                                 char const *message, void *pUserData) {
    UserData &data = *reinterpret_cast<UserData *>(pUserData);
    if (status == WGPURequestDeviceStatus_Success) {
      data.device = device;
    } else {
      std::cerr << "Could not get WebGPU device: " << message << std::endl;
    }
    data.requestEnded = true;
  };
  wgpuAdapterRequestDevice(adapter, descriptor, onDeviceRequestEnded, &userData);
  while (!userData.requestEnded) {
  } // Simplified busy wait
  return userData.device;
}
