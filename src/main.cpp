#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_sdl3.h>
#include <chrono>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>
#include <iostream>
#include "Common.h"
#include "GUI/gui.h"
#include "Graphics/ParticleSystem.h"
#include "Graphics/Simulation.h"
#include "Graphics/renderer.h"
#include "core/fps_counter.h"
#include "core/window.h"

// Constants
const int NUM_PARTICLE_TYPES = 4;
const int PARTICLES_PER_EMIT = 100;
const float PARTICLE_RADIUS = 4.0F;

// Global variables
bool paused = false;
bool randomizeOnStart = true;

void emitParticlesAtPosition(const glm::vec2 &position, int count, int type = -1);

std::unique_ptr<ParticleSystem> particleSystem;

int main() {
  try {
    Window window("Orbital Simulation", WINDOW_WIDTH, WINDOW_HEIGHT);

    if (gladLoadGLLoader(reinterpret_cast<GLADloadproc>(SDL_GL_GetProcAddress)) == 0) {
      std::cerr << "Failed to initialize GLAD\n";
      return -1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    ImGui::StyleColorsDark();

    ImGui_ImplSDL3_InitForOpenGL(window.getSDLWindow(), window.getGLContext());
    ImGui_ImplOpenGL3_Init("#version 410");

    Renderer renderer;
    const float referenceWidth = 1280.0F;
    const float referenceHeight = 720.0F;
    const float aspectRatio = referenceWidth / referenceHeight;

    int windowWidth = WINDOW_WIDTH;
    int windowHeight = WINDOW_HEIGHT;

    float projectionWidth = referenceWidth;
    float projectionHeight = referenceHeight;

    float currentAspectRatio = static_cast<float>(windowWidth) / static_cast<float>(windowHeight);
    if (currentAspectRatio > aspectRatio) {
      projectionWidth = projectionHeight * currentAspectRatio;
    } else {
      projectionHeight = projectionWidth / currentAspectRatio;
    }

    glm::mat4 projection =
        glm::ortho(-projectionWidth / 2.0F, projectionWidth / 2.0F, -projectionHeight / 2.0F,
                   projectionHeight / 2.0F, -1.0F, 1.0F);
    renderer.setProjectionMatrix(projection);

    glClearColor(glBackgroundColour.r, glBackgroundColour.g, glBackgroundColour.b,
                 glBackgroundColour.a);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    FpsCounter fpsCounter;

    auto lastFrameTime = std::chrono::high_resolution_clock::now();

    particleSystem = std::make_unique<ParticleSystem>(1000000); // Allow up to 1 million particles

    if (randomizeOnStart) {
      ParticleSystem::randomizeInteractions();
    }

    // Main loop
    while (!window.shouldClose()) {
      auto currentFrameTime = std::chrono::high_resolution_clock::now();
      float rawDeltaTime = std::chrono::duration<float>(currentFrameTime - lastFrameTime).count();
      lastFrameTime = currentFrameTime;

      float deltaTime = std::min(rawDeltaTime * simulation::simulationSpeed, 0.05F);

      window.pollEvents();

      ImGui_ImplOpenGL3_NewFrame();
      ImGui_ImplSDL3_NewFrame();
      ImGui::NewFrame();

      gui::RenderGui(fpsCounter);

      if (!paused) {
        particleSystem->update(deltaTime);
      }

      // Set and clear background color
      glClearColor(glBackgroundColour.r, glBackgroundColour.g, glBackgroundColour.b,
                   glBackgroundColour.a);
      glClear(GL_COLOR_BUFFER_BIT);

      ImGui::Render();

      ParticleSystem::render(projection);

      ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
      window.swapBuffers();

      float currentFps = fpsCounter.update();
      window.updateTitle(currentFps);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    return 0;
  } catch (const std::exception &e) {
    std::cerr << "Exception: " << e.what() << "\n";
    return -1;
  }
}