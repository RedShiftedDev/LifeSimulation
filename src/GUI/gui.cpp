#include "GUI/gui.h"
#include <algorithm>
#include <imgui.h>
#include "../Graphics/Simulation.h"
#include "Common.h"
#include "core/system_utils.h"

namespace gui {

// void PerformanceWindow(const FpsCounter &fpsCounter) {
//   // Performance metrics section
//   if (ImGui::CollapsingHeader("Performance Metrics", ImGuiTreeNodeFlags_DefaultOpen)) {
//     ImGui::Columns(2, "perfMetrics");

//     // FPS and Frame Time
//     ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4F, 0.8F, 1.0F, 1.0F));
//     ImGui::Text("FPS: %.1f", fpsCounter.getFps());
//     ImGui::PopStyleColor();

//     ImGui::NextColumn();
//     ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7F, 0.5F, 1.0F, 1.0F));
//     ImGui::Text("Frame Time: %.2f ms", 1000.0F / fpsCounter.getFps());
//     ImGui::PopStyleColor();

//     // Memory Usage and Entity Count
//     ImGui::NextColumn();
//     ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5F, 0.9F, 0.5F, 1.0F));
//     ImGui::Text("Memory: %.1f MB", SystemUtils::getApplicationMemoryUsage());
//     ImGui::PopStyleColor();

//     ImGui::NextColumn();
//     ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0F, 0.8F, 0.4F, 1.0F));
//     ImGui::Text("Entities: %zu", simulation::particleCount);
//     ImGui::PopStyleColor();

//     ImGui::Columns(1);

//     // FPS Graph
//     static float values[90] = {};
//     static int values_offset = 0;
//     static float refresh_time = 0.0F;

//     float current_time = ImGui::GetTime();
//     if (current_time - refresh_time >= 0.1F) {
//       values[values_offset] = fpsCounter.getFps();
//       values_offset = (values_offset + 1) % IM_ARRAYSIZE(values);
//       refresh_time = current_time;
//     }

//     float average = 0.0F;
//     for (float value : values) {
//       average += value;
//     }
//     average /= static_cast<float>(IM_ARRAYSIZE(values));

//     // Graph styling
//     ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(0.4F, 0.8F, 1.0F, 1.0F));
//     ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.1F, 0.12F, 0.15F, 0.9F));
//     ImGui::PushStyleColor(ImGuiCol_PlotLinesHovered, ImVec4(0.7F, 0.5F, 1.0F, 1.0F));

//     char overlay[32];
//     snprintf(overlay, sizeof(overlay), "Avg %.1f FPS", average);
//     ImGui::PlotLines("##FPS", values, IM_ARRAYSIZE(values), values_offset, overlay, 0.0F, 120.0F,
//                      ImVec2(0, 80.0F));

//     ImGui::PopStyleColor(3);
//   }
// }

void PerformanceWindow(const FpsCounter &fpsCounter) {
  // Performance metrics section
  if (ImGui::CollapsingHeader("Performance Metrics", ImGuiTreeNodeFlags_DefaultOpen)) {
    // Get current CPU and GPU metrics
    float cpuUsage = SystemUtils::getApplicationCPUUsage();
    float memoryUsage = SystemUtils::getApplicationMemoryUsage();

    // Create two columns for metrics display
    ImGui::Columns(2, "perfMetrics");

    // FPS and Frame Time
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4F, 0.8F, 1.0F, 1.0F));
    ImGui::Text("FPS: %.1f", fpsCounter.getFps());
    ImGui::PopStyleColor();

    ImGui::NextColumn();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7F, 0.5F, 1.0F, 1.0F));
    ImGui::Text("Frame Time: %.2f ms", 1000.0F / fpsCounter.getFps());
    ImGui::PopStyleColor();

    // Memory Usage and Entity Count
    ImGui::NextColumn();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5F, 0.9F, 0.5F, 1.0F));
    ImGui::Text("Memory: %.1f MB", memoryUsage);
    ImGui::PopStyleColor();

    ImGui::NextColumn();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0F, 0.8F, 0.4F, 1.0F));
    ImGui::Text("Entities: %zu", simulation::particleCount);
    ImGui::PopStyleColor();

    // CPU Usage App
    ImGui::NextColumn();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9F, 0.5F, 0.5F, 1.0F));
    ImGui::Text("CPU App: %.1f%%", cpuUsage);
    ImGui::PopStyleColor();

    ImGui::Columns(1);

    // Tabbed graphs section
    static int currentGraphTab = 0;
    const float graphHeight = 120.0F;

    // Style for tabs
    ImGui::PushStyleColor(ImGuiCol_Tab, ImVec4(0.15F, 0.15F, 0.2F, 1.0F));
    ImGui::PushStyleColor(ImGuiCol_TabHovered, ImVec4(0.3F, 0.3F, 0.4F, 1.0F));
    ImGui::PushStyleColor(ImGuiCol_TabActive, ImVec4(0.2F, 0.2F, 0.3F, 1.0F));

    if (ImGui::BeginTabBar("PerformanceGraphs")) {
      // FPS Graph Tab
      if (ImGui::BeginTabItem("FPS")) {
        currentGraphTab = 0;
        ImGui::EndTabItem();
      }

      // CPU Usage Graph Tab
      if (ImGui::BeginTabItem("CPU")) {
        currentGraphTab = 1;
        ImGui::EndTabItem();
      }

      ImGui::EndTabBar();
    }

    ImGui::PopStyleColor(3); // Pop tab styles

    // Graph data arrays - updated in a ring buffer pattern
    const int dataPoints = 90;
    static float fpsValues[dataPoints] = {};
    static float appCpuValues[dataPoints] = {};

    static int values_offset = 0;
    static float refresh_time = 0.0F;
    static const float updateInterval = 0.1F; // Update every 100ms

    // Update data points periodically
    float current_time = ImGui::GetTime();
    if (current_time - refresh_time >= updateInterval) {
      // Update all metrics
      fpsValues[values_offset] = fpsCounter.getFps();
      appCpuValues[values_offset] = cpuUsage;
      // Move to next position in circular buffer
      values_offset = (values_offset + 1) % dataPoints;
      refresh_time = current_time;
    }

    // Calculate averages for each metric
    float avgFps = 0.0F;
    float avgAppCpu = 0.0F;

    for (int i = 0; i < dataPoints; i++) {
      avgFps += fpsValues[i];
      avgAppCpu += appCpuValues[i];
    }

    avgFps /= static_cast<float>(dataPoints);
    avgAppCpu /= static_cast<float>(dataPoints);

    // Setup common graph styling
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.1F, 0.12F, 0.15F, 0.9F));

    // Now render the appropriate graph based on current tab
    char overlay[64];
    switch (currentGraphTab) {
    case 0: // FPS Graph
      ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(0.4F, 0.8F, 1.0F, 1.0F));
      ImGui::PushStyleColor(ImGuiCol_PlotLinesHovered, ImVec4(0.7F, 0.5F, 1.0F, 1.0F));
      snprintf(overlay, sizeof(overlay), "Avg %.1f FPS", avgFps);
      ImGui::PlotLines("##FPS", fpsValues, dataPoints, values_offset, overlay, 0.0F, 120.0F,
                       ImVec2(-1, graphHeight));
      ImGui::PopStyleColor(2);
      break;

    case 1: // CPU Graph (shows both app and system usage)
      // App CPU graph
      ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(0.9F, 0.5F, 0.5F, 1.0F));
      ImGui::PushStyleColor(ImGuiCol_PlotLinesHovered, ImVec4(1.0F, 0.7F, 0.7F, 1.0F));
      snprintf(overlay, sizeof(overlay), "App CPU: Avg %.1f%%", avgAppCpu);
      ImGui::PlotLines("##AppCPU", appCpuValues, dataPoints, values_offset, overlay, 0.0F, 100.0F,
                       ImVec2(-1, graphHeight));
      ImGui::PopStyleColor(2);
      break;
    }

    ImGui::PopStyleColor(); // Pop common style (FrameBg)

    // Add tooltips to provide more detailed information
    if (ImGui::IsItemHovered() && ImGui::BeginTooltip()) {
      switch (currentGraphTab) {
      case 0:
        ImGui::Text("Current FPS: %.1f", fpsCounter.getFps());
        ImGui::Text("Frame Time: %.2f ms", 1000.0F / fpsCounter.getFps());
        ImGui::Text("Average FPS: %.1f", avgFps);
        break;

      case 1:
        ImGui::Text("Application CPU: %.1f%%", cpuUsage);
        break;
      }
      ImGui::EndTooltip();
    }
  }
}

void RenderGui(const FpsCounter &fpsCounter) {
  // Set window styling
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0F);
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0F);
  ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.07F, 0.07F, 0.09F, 0.94F));
  ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.4F, 0.4F, 0.8F, 0.5F));
  ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.3F, 0.3F, 0.7F, 0.5F));

  ImGui::Begin("Simulation Controls", nullptr,
               ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar);

  // Title
  ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
  ImGui::TextColored(ImVec4(0.4F, 0.8F, 1.0F, 1.0F), "Particle System");
  ImGui::PopFont();
  ImGui::Spacing();

  // Speed Control with gradient
  ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.1F, 0.1F, 0.2F, 1.0F));
  ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.3F, 0.7F, 0.9F, 1.0F));
  ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(0.4F, 0.8F, 1.0F, 1.0F));
  if (ImGui::DragFloat("Simulation Speed", &simulation::simulationSpeed, 0.1F, 0.1F, 25.0F,
                       "%.1fx")) {
    simulation::simulationSpeed = std::max(simulation::simulationSpeed, 0.1F);
  }
  ImGui::PopStyleColor(3);

  // Background Color Control
  ImGui::Spacing();
  ImGui::TextColored(ImVec4(0.4F, 0.8F, 1.0F, 1.0F), "Background Color");
  ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.1F, 0.1F, 0.2F, 1.0F));
  float bgColor[4] = {glBackgroundColour.r, glBackgroundColour.g, glBackgroundColour.b,
                      glBackgroundColour.a};
  if (ImGui::ColorEdit4("##BackgroundColor", bgColor)) {
    glBackgroundColour = glm::vec4(bgColor[0], bgColor[1], bgColor[2], bgColor[3]);
  }
  ImGui::PopStyleColor();

  // Particle Control Section
  ImGui::Spacing();
  ImGui::TextColored(ImVec4(0.4F, 0.8F, 1.0F, 1.0F), "Particle Controls");
  ImGui::Separator();

  // Particle count input
  ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.1F, 0.1F, 0.2F, 1.0F));
  ImGui::DragInt("Desired Particle Count", &simulation::desiredParticleCount, 1, 0, 100000);
  ImGui::PopStyleColor();

  // Create and Clear buttons
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2F, 0.7F, 0.2F, 1.0F));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3F, 0.8F, 0.3F, 1.0F));
  if (ImGui::Button("Create Particles", ImVec2(ImGui::GetContentRegionAvail().x * 0.5F, 0))) {
    simulation::shouldCreateParticles = true;
  }
  ImGui::PopStyleColor(2);

  ImGui::SameLine();

  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7F, 0.2F, 0.2F, 1.0F));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8F, 0.3F, 0.3F, 1.0F));
  if (ImGui::Button("Clear Particles", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
    simulation::shouldClearParticles = true;
  }
  ImGui::PopStyleColor(2);

  // Display current particle count
  ImGui::TextColored(ImVec4(0.9F, 0.9F, 0.9F, 1.0F), "Active Particles:");
  ImGui::SameLine();
  ImGui::TextColored(ImVec4(0.4F, 0.8F, 1.0F, 1.0F), "%zu", simulation::particleCount);
  // Collision Settings
  ImGui::Spacing();
  if (ImGui::CollapsingHeader("Collision Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::Indent(10.0F);

    // Boundary toggle with custom checkbox
    ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0.4F, 0.8F, 1.0F, 1.0F));
    ImGui::Checkbox("Enable Boundaries", &simulation::enableBounds);
    ImGui::PopStyleColor();

    if (simulation::enableBounds) {
      ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.1F, 0.1F, 0.2F, 1.0F));
      ImGui::DragFloat("Left Boundary", &simulation::boundaryLeft, 1.0F, -WINDOW_WIDTH / 2.0F,
                       WINDOW_WIDTH / 2.0F);
      ImGui::DragFloat("Right Boundary", &simulation::boundaryRight, 1.0F, -WINDOW_WIDTH / 2.0F,
                       WINDOW_WIDTH / 2.0F);
      ImGui::DragFloat("Top Boundary", &simulation::boundaryTop, 1.0F, -WINDOW_HEIGHT / 2.0F,
                       WINDOW_HEIGHT / 2.0F);
      ImGui::DragFloat("Bottom Boundary", &simulation::boundaryBottom, 1.0F, -WINDOW_HEIGHT / 2.0F,
                       WINDOW_HEIGHT / 2.0F);
      ImGui::PopStyleColor();
    }
    ImGui::Unindent(10.0F);
  }

  // Performance section
  ImGui::Separator();
  PerformanceWindow(fpsCounter);

  ImGui::End();

  // Pop all style modifications
  ImGui::PopStyleColor(3);
  ImGui::PopStyleVar(2);
}

bool ShouldCreateParticle() {
  bool result = simulation::createParticle;
  if (result) {
    simulation::createParticle = false;
  }
  return result;
  simulation::particleCount++;
}

void ResetParticleCreation() { simulation::createParticle = false; }

bool HasBounds() { return simulation::enableBounds; }

} // namespace gui
