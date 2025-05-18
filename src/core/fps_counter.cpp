#include "fps_counter.h"

FpsCounter::FpsCounter()
    : frameCount(0), lastTime(std::chrono::high_resolution_clock::now()), fps(0.0F) {}

float FpsCounter::update() {
  frameCount++;
  auto currentTime = std::chrono::high_resolution_clock::now();
  float elapsed = std::chrono::duration<float>(currentTime - lastTime).count();

  if (elapsed >= 1.0F) {
    fps = static_cast<float>(frameCount) / elapsed;
    frameCount = 0;
    lastTime = currentTime;
  }

  return fps;
}

float FpsCounter::getFps() const { return fps; }