#include <iostream>
#include "Common.h"
#include "Graphics/App.h"

int main() {
  try {
    App application("Orbital Simulation", WINDOW_WIDTH, WINDOW_HEIGHT);
    application.run();
  } catch (const std::exception &e) {
    std::cerr << "Fatal Error during application setup: " << e.what() << std::endl;
    return -1;
  } catch (...) {
    std::cerr << "Unknown fatal error during application setup." << std::endl;
    return -1;
  }
  return 0;
}
