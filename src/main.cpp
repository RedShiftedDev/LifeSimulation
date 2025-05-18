#include <glad/glad.h>
#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_sdl3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>
#include <iostream>
#include "GUI/gui.h"
#include "Graphics/renderer.h"
#include "core/fps_counter.h"
#include "core/window.h"

int main() {
  try {
    Window window("Orbital Simulation", 1280, 720);

    if (gladLoadGLLoader(reinterpret_cast<GLADloadproc>(SDL_GL_GetProcAddress)) == 0) {
      std::cerr << "Failed to initialize GLAD\n";
      return -1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls

    ImGui::StyleColorsDark();

    ImGui_ImplSDL3_InitForOpenGL(window.getSDLWindow(), window.getGLContext());
    ImGui_ImplOpenGL3_Init("#version 410");

    // Initialize renderer and create sphere
    Renderer renderer;

    // Setup camera
    glm::mat4 projection = glm::perspective(glm::radians(45.0F), 1280.0F / 720.0F, 0.1f, 100.0f);
    glm::mat4 view = glm::lookAt(glm::vec3(0.0F, 0.0F, 5.0F), glm::vec3(0.0f, 0.0f, 0.0f),
                                 glm::vec3(0.0F, 1.0F, 0.0F));

    renderer.setProjectionMatrix(projection);
    renderer.setViewMatrix(view);

    glClearColor(0.1F, 0.1F, 0.15F, 1.0F);
    glEnable(GL_DEPTH_TEST);

    FpsCounter fpsCounter;

    while (!window.shouldClose()) {
      window.pollEvents();

      ImGui_ImplOpenGL3_NewFrame();
      ImGui_ImplSDL3_NewFrame();
      ImGui::NewFrame();

      gui::RenderGui();

      ImGui::Render();

      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

      // Render the scenes

      ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

      window.swapBuffers();

      float currentFps = fpsCounter.update();
      // Update window title with current FPS
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