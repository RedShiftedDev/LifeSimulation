#include <glad/glad.h>
#include <algorithm>
#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_sdl3.h>
#include <chrono>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>
#include <iostream>
#include "GUI/gui.h"
#include "Graphics/renderer.h"
#include "core/fps_counter.h"
#include "core/window.h"

void updateCirclePhysics(std::vector<Circle> &circles, float deltaTime, float windowWidth,
                         float windowHeight, float gravity, float bounceFactor, float friction) {
  for (auto &circle : circles) {
    // Apply gravity
    circle.velocityY += gravity * deltaTime;

    // Update position based on velocity
    circle.x += circle.velocityX * deltaTime;
    circle.y += circle.velocityY * deltaTime;

    // Apply friction
    circle.velocityX *= (1.0F - friction);
    circle.velocityY *= (1.0F - friction);

    // Handle collisions with window edges
    // Right wall
    if (circle.x + circle.radius > windowWidth) {
      circle.x = windowWidth - circle.radius;
      circle.velocityX *= -bounceFactor;
    }
    // Left wall
    if (circle.x - circle.radius < 0) {
      circle.x = circle.radius;
      circle.velocityX *= -bounceFactor;
    }
    // Bottom wall
    if (circle.y + circle.radius > windowHeight) {
      circle.y = windowHeight - circle.radius;
      circle.velocityY *= -bounceFactor;
    }
    // Top wall
    if (circle.y - circle.radius < 0) {
      circle.y = circle.radius;
      circle.velocityY *= -bounceFactor;
    }
  }
}

// Function to handle circle-to-circle collisions
void handleCircleCollisions(std::vector<Circle> &circles, float bounceFactor) {
  for (size_t i = 0; i < circles.size(); i++) {
    for (size_t j = i + 1; j < circles.size(); j++) {
      Circle &c1 = circles[i];
      Circle &c2 = circles[j];

      // Calculate distance between circles
      float dx = c2.x - c1.x;
      float dy = c2.y - c1.y;
      float distance = std::sqrt((dx * dx) + (dy * dy));

      // Check if circles are colliding
      float minDist = c1.radius + c2.radius;
      if (distance < minDist) {
        // Normalize collision vector
        float nx = dx / distance;
        float ny = dy / distance;

        // Calculate relative velocity
        float dvx = c2.velocityX - c1.velocityX;
        float dvy = c2.velocityY - c1.velocityY;

        // Calculate velocity along collision normal
        float velAlongNormal = (dvx * nx) + (dvy * ny);

        // Only separate if circles are moving toward each other
        if (velAlongNormal < 0) {
          // Calculate collision impulse
          float mass1 = c1.radius * c1.radius; // Mass proportional to area
          float mass2 = c2.radius * c2.radius;
          float impulse = (-(1 + bounceFactor) * velAlongNormal) / (1 / mass1 + 1 / mass2);

          // Apply impulse to velocities
          c1.velocityX -= (impulse / mass1) * nx;
          c1.velocityY -= (impulse / mass1) * ny;
          c2.velocityX += (impulse / mass2) * nx;
          c2.velocityY += (impulse / mass2) * ny;
        }

        // Separate circles to prevent sticking
        float overlap = minDist - distance;
        float separationX = (overlap * nx) * 0.5F;
        float separationY = (overlap * ny) * 0.5F;

        c1.x -= separationX;
        c1.y -= separationY;
        c2.x += separationX;
        c2.y += separationY;
      }
    }
  }
}

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
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    ImGui::StyleColorsDark();

    ImGui_ImplSDL3_InitForOpenGL(window.getSDLWindow(), window.getGLContext());
    ImGui_ImplOpenGL3_Init("#version 410");

    Renderer renderer;

    int windowWidth = 1280;
    int windowHeight = 720;
    glm::mat4 projection = glm::ortho(0.0F, static_cast<float>(windowWidth),
                                      static_cast<float>(windowHeight), 0.0F, -1.0F, 1.0F);
    renderer.setProjectionMatrix(projection);

    glClearColor(0.1F, 0.1F, 0.15F, 1.0F);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    FpsCounter fpsCounter;

    auto lastFrameTime = std::chrono::high_resolution_clock::now();

    // Main loop
    while (!window.shouldClose()) {
      auto currentFrameTime = std::chrono::high_resolution_clock::now();
      float deltaTime = std::chrono::duration<float>(currentFrameTime - lastFrameTime).count();
      lastFrameTime = currentFrameTime;

      deltaTime = std::min(deltaTime, 0.05F);

      window.pollEvents();

      float mouseX;
      float mouseY;
      Uint32 mouseState = SDL_GetMouseState(&mouseX, &mouseY);
      if ((mouseState & SDL_BUTTON_LMASK) != 0U) {
        if (!ImGui::GetIO().WantCaptureMouse) {
          static auto lastSpawnTime = std::chrono::high_resolution_clock::now();
          auto currentTime = std::chrono::high_resolution_clock::now();
          auto timeSinceLastSpawn =
              std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastSpawnTime)
                  .count();

          if (timeSinceLastSpawn > 100) {
            gui::AddCircle(mouseX, mouseY);
            lastSpawnTime = currentTime;
          }
        }
      }

      ImGui_ImplOpenGL3_NewFrame();
      ImGui_ImplSDL3_NewFrame();
      ImGui::NewFrame();

      gui::RenderGui();

      // Update circles physics
      auto &circles = const_cast<std::vector<Circle> &>(gui::GetCircles());
      updateCirclePhysics(circles, deltaTime, windowWidth, windowHeight, gui::GetGravity(),
                          gui::GetBounceFactor(), gui::GetFriction());

      // Handle circle-to-circle collisions
      handleCircleCollisions(circles, gui::GetBounceFactor());

      ImGui::Render();

      glClear(GL_COLOR_BUFFER_BIT);

      // Draw all circles
      for (const auto &circle : circles) {
        renderer.drawCircle(circle.x, circle.y, circle.radius, circle.color);
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