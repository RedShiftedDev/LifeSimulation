#pragma once
#include <cstddef>
#include <glm/glm.hpp>
#include "fps_counter.h"

namespace gui {

void RenderGui(const FpsCounter &fpsCounter);
void SetParticleCount(size_t count);
bool ShouldCreateParticle();
void ResetParticleCreation();
bool HasBounds();

} // namespace gui