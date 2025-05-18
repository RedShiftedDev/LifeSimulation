#pragma once
#include <SDL3/SDL.h>
#include <string>

class Window {
public:
  Window(const char *title, int width, int height);
  ~Window();

  void swapBuffers() const;
  bool shouldClose() const;

  void pollEvents();
  SDL_Window *getSDLWindow() const;
  SDL_GLContext getGLContext() const;

  void updateTitle(float fps);

private:
  SDL_Window *window = nullptr;
  SDL_GLContext glContext = nullptr;
  bool isRunning = true;
  std::string windowTitle;
};