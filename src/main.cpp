#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_sdl3.h>
#include <chrono>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>
#include <iostream>
#include <random>
#include "Common.h"
#include "GUI/gui.h"
#include "Graphics/ParticleSystem.h"
#include "Graphics/Simulation.h"
#include "Graphics/renderer.h"
#include "core/fps_counter.h"
#include "core/window.h"

// Constants
const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;
const int NUM_PARTICLE_TYPES = 6;
const int PARTICLES_PER_EMIT = 100;
const float PARTICLE_RADIUS = 3.0f;
const float SIMULATION_AREA_SIZE = 600.0f; // Simulation area size

// Global variables
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;
float deltaTime = 0.0f;
float lastFrame = 0.0f;
bool paused = false;
bool showDebugInfo = true;
bool randomizeOnStart = true;
float emitRadius = 50.0f;

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

    // ParticleSystem particleSystem(1000000);
    particleSystem = std::make_unique<ParticleSystem>(1000000); // Allow up to 1 million particles

    // Initialize particle count
    // simulation::particleCount = 0;

    // Randomize interaction matrix on start
    if (randomizeOnStart) {
      particleSystem->randomizeInteractions();
    }

    // Create initial particles
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> posDist(-SIMULATION_AREA_SIZE * 0.8f,
                                                  SIMULATION_AREA_SIZE * 0.8f);

    // Create clusters of each type
    for (int type = 0; type < NUM_PARTICLE_TYPES; ++type) {
      // Create a cluster center
      glm::vec2 clusterCenter(posDist(gen), posDist(gen));

      // Distribute particles around cluster center
      std::uniform_real_distribution<float> offsetDist(-50.0f, 50.0f);

      for (int i = 0; i < 500; ++i) {
        glm::vec2 position = clusterCenter + glm::vec2(offsetDist(gen), offsetDist(gen));
        emitParticlesAtPosition(position, 1, type);
      }
    }

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

      // if (simulation::shouldCreateParticles) {
      //   int newParticles = simulation::desiredParticleCount;
      //   for (int i = 0; i < newParticles; i++) {
      //     float randX = static_cast<float>((rand() % 1000) - 500);
      //     float randY = static_cast<float>((rand() % 1000) - 500);
      //     float randVX = static_cast<float>((rand() % 100) - 50);
      //     float randVY = static_cast<float>((rand() % 100) - 50);
      //     particleSystem.emitParticles(1, glm::vec2(randX, randY), 10.0F, 5.0F,
      //                                  glm::vec2(randVX, randVY));
      //   }
      //   simulation::particleCount = particleSystem.getParticleCount();
      //   simulation::shouldCreateParticles = false;
      // }

      // if (simulation::shouldClearParticles) {
      //   particleSystem.clear();
      //   simulation::particleCount = 0;
      //   simulation::desiredParticleCount = 0;
      //   glClear(GL_COLOR_BUFFER_BIT);
      //   simulation::shouldClearParticles = false;
      // }

      if (!paused) {
        particleSystem->update(deltaTime);
      }

      // Update particle physics
      // particleSystem.update(deltaTime);

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

void emitParticlesAtPosition(const glm::vec2 &position, int count, int type) {
  // Create random generator
  static std::random_device rd;
  static std::mt19937 gen(rd());

  // Generate random velocities
  std::uniform_real_distribution<float> velDist(-10.0f, 10.0f);
  glm::vec2 velocity(velDist(gen), velDist(gen));

  // If type is specified, create particles of that type, otherwise random
  if (type >= 0 && type < NUM_PARTICLE_TYPES) {
    // Create particles with specific type
    for (int i = 0; i < count; ++i) {
      Particle &p = particleSystem->createParticle();
      p.setActive(true);
      p.setPos(position);
      p.setRadius(PARTICLE_RADIUS);
      p.setVel(glm::vec2(velDist(gen), velDist(gen)));
      p.setType(type);
      p.setColor(simulation::COLORS[type]);
    }
  } else {
    // Create random particles using the emitParticles function
    particleSystem->emitParticles(count, position, PARTICLE_RADIUS, 0.0f, velocity);
  }
}