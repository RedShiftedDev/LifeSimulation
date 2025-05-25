// window.h
#pragma once
#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>
#include <string>
#include <webgpu/webgpu.h>

class Window {
public:
  Window(const char *title, int width, int height);
  ~Window();

  void swapBuffers() const;
  bool shouldClose() const;

  void pollEvents();
  WGPUSurface getWGPUSurface() const;
  GLFWwindow *getGLFWWindow() const;

  void updateTitle(float fps);

  // New methods for proper WebGPU integration
  void createSurface(WGPUInstance instance);
  void getFramebufferSize(int *width, int *height) const;
  void getWindowSize(int *width, int *height) const;
  bool wasResized() const { return resized; }
  void resetResizedFlag() { resized = false; }

private:
  GLFWwindow *window = nullptr;
  WGPUSurface surface = nullptr;
  bool isRunning = true;
  bool resized = false;
  std::string windowTitle;

  // Static callback for window resize
  static void framebufferSizeCallback(GLFWwindow *window, int width, int height);
};
