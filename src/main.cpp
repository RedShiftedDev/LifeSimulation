#include <glad/glad.h>
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

    ParticleSystem particleSystem(1000000);

    // Initialize particle count
    simulation::particleCount = 0;

    // Main loop
    while (!window.shouldClose()) {
      auto currentFrameTime = std::chrono::high_resolution_clock::now();
      float rawDeltaTime = std::chrono::duration<float>(currentFrameTime - lastFrameTime).count();
      lastFrameTime = currentFrameTime;

      // Calculate deltaTime with simulation speed applied
      float deltaTime = std::min(rawDeltaTime * simulation::simulationSpeed, 0.05F);

      // Debug output
      // std::cout << "Raw Delta time: " << rawDeltaTime << ", Adjusted Delta time: " << deltaTime
      //           << ", Simulation speed: " << simulation::simulationSpeed << '\n';

      window.pollEvents();

      ImGui_ImplOpenGL3_NewFrame();
      ImGui_ImplSDL3_NewFrame();
      ImGui::NewFrame();

      gui::RenderGui(fpsCounter);

      if (simulation::shouldCreateParticles) {
        int newParticles = simulation::desiredParticleCount;
        for (int i = 0; i < newParticles; i++) {
          float randX = static_cast<float>((rand() % 1000) - 500);
          float randY = static_cast<float>((rand() % 1000) - 500);
          float randVX = static_cast<float>((rand() % 100) - 50);
          float randVY = static_cast<float>((rand() % 100) - 50);
          particleSystem.emitParticles(1, glm::vec2(randX, randY), 10.0F, 5.0F, glm::vec2(randVX, randVY));
        }
        simulation::particleCount = particleSystem.getParticleCount();
        simulation::shouldCreateParticles = false;
      }

      if (simulation::shouldClearParticles) {
        particleSystem.clear();
        simulation::particleCount = 0;
        simulation::desiredParticleCount = 0;
        glClear(GL_COLOR_BUFFER_BIT);
        simulation::shouldClearParticles = false;
      }

      // Update particle physics
      particleSystem.update(deltaTime);

      // Set and clear background color
      glClearColor(glBackgroundColour.r, glBackgroundColour.g, glBackgroundColour.b,
                   glBackgroundColour.a);
      glClear(GL_COLOR_BUFFER_BIT);

      ImGui::Render();

      // Render all particles
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