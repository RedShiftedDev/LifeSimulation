#pragma once
#include <chrono>

class FpsCounter {
public:
  FpsCounter();
  float update();
  float getFps() const;

private:
  unsigned int frameCount;
  std::chrono::high_resolution_clock::time_point lastTime;
  float fps;
};
