// window.cpp
#include "window.h"
#include <iostream>
#include <stdexcept>
#include <string>

Window::Window(const char *title, const int width, const int height) {
  windowTitle = title;

  if (glfwInit() == GLFW_FALSE) {
    throw std::runtime_error("Failed to initialize GLFW");
  }

  // Set GLFW hints for WebGPU (no OpenGL context needed)
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

  window = glfwCreateWindow(width, height, windowTitle.c_str(), nullptr, nullptr);
  if (window == nullptr) {
    glfwTerminate();
    throw std::runtime_error("Failed to create GLFW window");
  }

  // Set user pointer for callbacks
  glfwSetWindowUserPointer(window, this);

  // Set resize callback
  glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
}

Window::~Window() {
  if (surface != nullptr) {
    wgpuSurfaceRelease(surface);
  }
  if (window != nullptr) {
    glfwDestroyWindow(window);
  }
  glfwTerminate();
}

void Window::createSurface(WGPUInstance instance) {
  if (surface != nullptr) {
    wgpuSurfaceRelease(surface);
  }

  surface = glfwGetWGPUSurface(instance, window);
  if (surface == nullptr) {
    throw std::runtime_error("Failed to create WebGPU surface");
  }
}

void Window::getFramebufferSize(int *width, int *height) const {
  glfwGetFramebufferSize(window, width, height);
}

void Window::getWindowSize(int *width, int *height) const {
  glfwGetWindowSize(window, width, height);
}

void Window::framebufferSizeCallback(GLFWwindow *window, int width, int height) {
  Window *windowObj = static_cast<Window *>(glfwGetWindowUserPointer(window));
  if (windowObj != nullptr) {
    windowObj->resized = true;
    std::cout << "Window resized to: " << width << "x" << height << std::endl;
  }
}

void Window::swapBuffers() const {
  // WebGPU handles presentation through swap chain
  // This method is kept for compatibility but doesn't need to do anything
}

void Window::pollEvents() {
  glfwPollEvents();
  if (glfwWindowShouldClose(window) != 0) {
    isRunning = false;
  }
}

bool Window::shouldClose() const { return !isRunning; }

GLFWwindow *Window::getGLFWWindow() const { return window; }

WGPUSurface Window::getWGPUSurface() const { return surface; }

void Window::updateTitle(float fps) {
  std::string title = windowTitle + " | FPS: " + std::to_string(static_cast<int>(fps));
  glfwSetWindowTitle(window, title.c_str());
}
