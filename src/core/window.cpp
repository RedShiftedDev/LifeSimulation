#include "window.h"
#include <backends/imgui_impl_sdl3.h>
#include <stdexcept>
#include <string>

Window::Window(const char *title, const int width, const int height) {
  windowTitle = title;

  if (static_cast<int>(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) < 0) {
    throw std::runtime_error("Failed to initialize SDL");
  }

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

  window =
      SDL_CreateWindow(windowTitle.c_str(), width, height,
                       SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
  if (window == nullptr) {
    throw std::runtime_error("Failed to create SDL window");
  }

  glContext = SDL_GL_CreateContext(window);
  if (glContext == nullptr) {
    throw std::runtime_error("Failed to create OpenGL context");
  }

  SDL_GL_MakeCurrent(window, glContext);
  SDL_GL_SetSwapInterval(1);
}

Window::~Window() {
  if (glContext != nullptr) {
    SDL_GL_DestroyContext(glContext);
  }
  if (window != nullptr) {
    SDL_DestroyWindow(window);
  }
  SDL_Quit();
}

void Window::swapBuffers() const { SDL_GL_SwapWindow(window); }

void Window::pollEvents() {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    ImGui_ImplSDL3_ProcessEvent(&event);
    if (event.type == SDL_EVENT_QUIT) {
      isRunning = false;
    }
  }
}

bool Window::shouldClose() const { return !isRunning; }

SDL_Window *Window::getSDLWindow() const { return window; }

SDL_GLContext Window::getGLContext() const { return glContext; }

void Window::updateTitle(float fps) {
  std::string title = windowTitle + " | FPS: " + std::to_string(static_cast<int>(fps));
  SDL_SetWindowTitle(window, title.c_str());
}