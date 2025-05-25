#pragma once // Header guard

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <memory>
#include <webgpu/webgpu.h>

class Window;
class ParticleSystem;
class Renderer;
class FpsCounter;

namespace gui {
void RenderGui(const FpsCounter &fpsCounter);
extern glm::vec4 BackgroundColour;
} // namespace gui

namespace simulation {
extern float simulationSpeed;
}

class App {
public:
  App(const char *windowTitle, int windowWidth, int windowHeight);
  ~App();

  void run();

private:
  void init();
  void initWebGPU();
  void initImGui();
  void initApplicationLogic();

  void mainLoop();
  void update(float deltaTime);
  void render();
  void cleanup();

  void handleResize();
  static glm::mat4 calculateProjectionMatrix(int windowWidth, int windowHeight);
  void emitParticlesAtPosition(const glm::vec2 &position, int count, int type = -1);

  std::unique_ptr<Window> m_window;
  std::unique_ptr<ParticleSystem> m_particleSystem;
  std::unique_ptr<Renderer> m_renderer2D;
  std::unique_ptr<FpsCounter> m_fpsCounter;

  // WebGPU Resources
  WGPUInstance m_instance = nullptr;
  WGPUSurface m_surface = nullptr;
  WGPUAdapter m_adapter = nullptr;
  WGPUDevice m_device = nullptr;
  WGPUQueue m_queue = nullptr;
  WGPUSurfaceConfiguration m_surfaceConfig = {};
  WGPUTextureFormat m_surfaceFormat = WGPUTextureFormat_Undefined;

  glm::mat4 m_projectionMatrix{1.0F};

  bool m_paused = false;
  bool m_randomizeOnStart = true;

  static void onDeviceError(WGPUErrorType type, char const *message, void *userdata);
  static WGPUAdapter requestAdapter(WGPUInstance instance,
                                    WGPURequestAdapterOptions const *options);
  static WGPUDevice requestDevice(WGPUAdapter adapter, WGPUDeviceDescriptor const *descriptor);
};
