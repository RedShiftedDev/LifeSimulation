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
#include "Graphics/Particle.h"
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

    std::vector<Particle> particles;

    // Add one initial particle
    Particle initialParticle;
    initialParticle.setPos(glm::vec2(0.0F, 0.0F));
    initialParticle.setVel(glm::vec2(100.0F, 120.0F));
    initialParticle.setAcc(glm::vec2(0.0F, -0.01F));
    particles.push_back(initialParticle);

    // Initialize particle count
    simulation::particleCount = 1;

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

      // Handle particle spawning
      float mouseX;
      float mouseY;
      Uint32 mouseState = SDL_GetMouseState(&mouseX, &mouseY);

      // GUI-based particle creation
      if (gui::ShouldCreateParticle() && !ImGui::GetIO().WantCaptureMouse) {
        // Here you'd create a new particle based on mouse position
        // For now we'll just increment the counter
        simulation::particleCount++;
      }

      ImGui_ImplOpenGL3_NewFrame();
      ImGui_ImplSDL3_NewFrame();
      ImGui::NewFrame();

      gui::RenderGui(fpsCounter);

      if (gui::ShouldCreateParticle()) {
        Particle newParticle;
        float randX = static_cast<float>((rand() % 600) - 300);
        float randY = static_cast<float>((rand() % 600) - 300);
        newParticle.setPos(glm::vec2(randX, randY));
        float randVX = static_cast<float>((rand() % 100) - 50) * 10.0F;
        float randVY = static_cast<float>((rand() % 100) - 50) * 10.0F;
        newParticle.setVel(glm::vec2(randVX, randVY));
        newParticle.setAcc(glm::vec2(0.0F, -0.01F));
        float r = static_cast<float>(rand() % 255) / 255.0F;
        float g = static_cast<float>(rand() % 255) / 255.0F;
        float b = static_cast<float>(rand() % 255) / 255.0F;
        newParticle.setColor(glm::vec3(r, g, b));
        particles.push_back(newParticle);
        simulation::particleCount = particles.size();
      }

      // Update particle physics
      for (auto &particle : particles) {
        particle.update(deltaTime);
      }

      // Set and clear background color
      glClearColor(glBackgroundColour.r, glBackgroundColour.g, glBackgroundColour.b,
                   glBackgroundColour.a);
      glClear(GL_COLOR_BUFFER_BIT);

      ImGui::Render();

      // Render all particles
      for (auto &particle : particles) {
        particle.render(projection);
      }

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