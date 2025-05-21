
```cs
#version 410 core

in vec2 TexCoord;
in vec4 Color;
in float Active;
out vec4 FragColor;

uniform float uTime;
uniform sampler2D uNoiseTexture;  // Noise texture for organic patterns
uniform vec3 uAccentColor;        // Secondary color for harmonious blending

void main() {
    // Discard inactive particles
    if (Active < 0.5) 
        discard;
    
    // Calculate distance from center
    vec2 center = vec2(0.5, 0.5);
    float dist = length(TexCoord - center) * 2.0;
    
    // Create organic noise patterns
    vec2 noiseCoord = TexCoord * 3.0 + vec2(uTime * 0.1, uTime * 0.08);
    float noise = texture(uNoiseTexture, noiseCoord).r * 0.2;
    
    // Create fractal patterns
    float fractal = 0.0;
    float amplitude = 1.0;
    float frequency = 1.0;
    for (int i = 0; i < 4; i++) {
        fractal += amplitude * texture(uNoiseTexture, noiseCoord * frequency).r;
        amplitude *= 0.5;
        frequency *= 2.0;
    }
    fractal = fractal * 0.15;
    
    // Dynamic pulsation
    float pulseSpeed = 0.8 + 0.2 * sin(uTime * 0.3);
    float pulseMagnitude = 0.15;
    float pulse = 1.0 + pulseMagnitude * sin(uTime * pulseSpeed);
    
    // Create a more interesting shape with noise-based distortion
    float distortedDist = dist - noise * 0.4 - fractal * 0.3;
    
    // Create light ray effects emanating from center
    float rays = 0.5 + 0.5 * sin(atan(TexCoord.y - 0.5, TexCoord.x - 0.5) * 8.0 + uTime * 2.0);
    rays *= smoothstep(1.0, 0.0, dist) * 0.15;
    
    // Color manipulation for beautiful gradients
    vec3 baseColor = Color.rgb;
    vec3 accentColor = uAccentColor;
    
    // Create internal glow with vibrant center
    float innerGlow = smoothstep(1.0, 0.0, dist * 2.0);
    vec3 glowColor = mix(baseColor * 1.6, accentColor, 0.5) * innerGlow;
    
    // Create outer ethereal glow
    float outerGlow = exp(-dist * 3.0) * 0.6;
    
    // Blend colors using the distorted distance
    float colorMix = smoothstep(0.0, 0.8, distortedDist);
    vec3 gradientColor = mix(baseColor * 1.3, accentColor * 0.7, colorMix);
    
    // Apply vignette effect for depth
    float vignette = smoothstep(1.4 * pulse, 0.4 * pulse, distortedDist);
    
    // Combine all effects
    vec3 finalColor = gradientColor;
    finalColor += glowColor;
    finalColor += outerGlow * accentColor;
    finalColor += rays * accentColor;
    finalColor *= vignette;
    
    // Apply subtle color aberration
    float aberration = 0.03;
    vec3 rgbOffset = vec3(
        smoothstep(0.8 * pulse - aberration, 1.0 * pulse - aberration, distortedDist),
        smoothstep(0.8 * pulse, 1.0 * pulse, distortedDist),
        smoothstep(0.8 * pulse + aberration, 1.0 * pulse + aberration, distortedDist)
    );
    finalColor = mix(finalColor, finalColor * rgbOffset, 0.3);
    
    // Edge softness with dynamic pulse
    float alpha = smoothstep(1.0 * pulse, 0.8 * pulse, distortedDist);
    alpha = mix(alpha, alpha * (0.9 + 0.1 * sin(uTime * 3.0 + dist * 5.0)), 0.3);
    
    // Discard pixels outside the main shape
    if (distortedDist > 1.0 * pulse) 
        discard;
    
    // Output final color with all effects applied
    FragColor = vec4(finalColor, alpha * Color.a);
}
```

```c++
void ParticleSystem::calculateInteractionForces(float deltaTime) {
  const float R_MAX = 100.0F;
  const float gridCellSize = R_MAX;
  const float invRMax = 1.0F / R_MAX;
  const float R_MAX_SQR = R_MAX * R_MAX;

  // Add damping factor to reduce numerical instability
  const float DAMPING = 0.97F;
  // Minimum distance to prevent extreme forces
  const float MIN_DIST_SQR = 4.0F;
  // Maximum force magnitude to prevent excessive acceleration
  const float MAX_FORCE = 1000.0F;
  const float SMOOTH_FACTOR = 0.3F; // Adjust this value to control smoothing (0.0-1.0)

  // 1. Pre-compute grid dimensions based on simulation boundaries
  const int gridWidth =
      std::ceil((simulation::boundaryRight - simulation::boundaryLeft) / gridCellSize);
  const int gridHeight =
      std::ceil((simulation::boundaryBottom - simulation::boundaryTop) / gridCellSize);

  // 2. SOA (Structure of Arrays) approach for better cache locality and SIMD
  struct ParticleSOA {
    std::vector<glm::vec2> positions;
    std::vector<glm::vec2> velocities;
    std::vector<int> types;
    std::vector<bool> active;
    std::vector<glm::vec2> forces;
  } particleData;

  // Pre-allocate with exact sizes needed
  const size_t numParticles = particles.size();
  particleData.positions.resize(numParticles);
  particleData.velocities.resize(numParticles);
  particleData.types.resize(numParticles);
  particleData.active.resize(numParticles);
  particleData.forces.resize(numParticles, glm::vec2(0.0F));

  // 3. Convert AOS to SOA for better cache coherence and SIMD potential
  std::vector<size_t> activeParticles;
  activeParticles.reserve(numParticles);

  for (size_t i = 0; i < numParticles; ++i) {
    particleData.positions[i] = particles[i].getPos();
    particleData.velocities[i] = particles[i].getVel();
    particleData.types[i] = particles[i].getType();
    particleData.active[i] = particles[i].isActive();

    if (particleData.active[i]) {
      activeParticles.push_back(i);
    }
  }

  // 4. Grid optimization with fixed-size vectors for predictable memory
  const size_t estimatedParticlesPerCell =
      std::max(size_t(8), numParticles / (gridWidth * gridHeight));
  std::vector<std::vector<size_t>> grid(gridWidth * gridHeight);
  for (auto &cell : grid) {
    cell.reserve(estimatedParticlesPerCell);
  }

  // Use direct indexing and avoid per-particle allocations
  std::vector<int> particleCellIndices(numParticles);

  // 5. Populate grid with particles - vectorizable loop
  for (size_t idx = 0; idx < activeParticles.size(); ++idx) {
    const size_t i = activeParticles[idx];
    const glm::vec2 &pos = particleData.positions[i];

    const int cellX = std::clamp(
        static_cast<int>((pos.x - simulation::boundaryLeft) / gridCellSize), 0, gridWidth - 1);
    const int cellY = std::clamp(static_cast<int>((pos.y - simulation::boundaryTop) /
    gridCellSize),
                                 0, gridHeight - 1);

    const int cellIndex = (cellY * gridWidth) + cellX;
    particleCellIndices[i] = cellIndex;
    grid[cellIndex].push_back(i);
  }

  // 6. Optimize thread management - use thread pool to avoid creation/destruction overhead
  const size_t numThreads = std::min<size_t>(
      std::thread::hardware_concurrency(), activeParticles.empty() ? 1UL :
      activeParticles.size());
  std::vector<std::thread> threads;
  threads.reserve(numThreads);

  // Load balance dynamically based on active particles
  const size_t chunkSize = std::max(size_t(1), activeParticles.size() / numThreads);

  // 7. Fast lookup table for interaction strengths (if types are bounded)
  constexpr int MAX_PARTICLE_TYPES = 10; // Adjust based on your actual max types
  static float interactionLUT[MAX_PARTICLE_TYPES][MAX_PARTICLE_TYPES] = {{0}};
  static bool lutInitialized = false;

  if (!lutInitialized) {
    for (int i = 0; i < MAX_PARTICLE_TYPES; ++i) {
      for (int j = 0; j < MAX_PARTICLE_TYPES; ++j) {
        interactionLUT[i][j] = getInteractionStrength(i, j);
      }
    }
    lutInitialized = true;
  }

  // 8. Use aligned memory for SIMD operations
  alignas(32) std::vector<float> distBuffer(32);

  auto processParticleChunk = [&](size_t startIdx, size_t endIdx) {
    // 9. Process particles in small batches for better cache behavior
    const size_t BATCH_SIZE = 32; // Fits L1 cache line

    for (size_t batchStart = startIdx; batchStart < endIdx; batchStart += BATCH_SIZE) {
      const size_t batchEnd = std::min(batchStart + BATCH_SIZE, endIdx);

      for (size_t idx = batchStart; idx < batchEnd; ++idx) {
        if (idx >= activeParticles.size())
          break;

        const size_t i = activeParticles[idx];
        glm::vec2 totalForce(0.0F);
        const glm::vec2 &pos_i = particleData.positions[i];
        const int type_i = particleData.types[i];
        const int cellIndex = particleCellIndices[i];

        // Convert linear index back to 2D for neighborhood search
        const int cellY = cellIndex / gridWidth;
        const int cellX = cellIndex % gridWidth;

        // 10. Neighbor cell lookup with reduced branching
        for (int dy = -1; dy <= 1; ++dy) {
          const int ny = cellY + dy;
          if (ny < 0 || ny >= gridHeight)
            continue;

          for (int dx = -1; dx <= 1; ++dx) {
            const int nx = cellX + dx;
            if (nx < 0 || nx >= gridWidth)
              continue;

            const auto &cellParticles = grid[ny * gridWidth + nx];

            // 11. Batch process particles for vectorization
            for (size_t j_idx = 0; j_idx < cellParticles.size(); j_idx++) {
              const size_t j = cellParticles[j_idx];

              if (i == j || !particleData.active[j])
                continue;

              const glm::vec2 &pos_j = particleData.positions[j];
              const glm::vec2 distVec = pos_j - pos_i;
              const float distSqr = glm::dot(distVec, distVec);

              // Improved distance thresholding
              if (distSqr < MIN_DIST_SQR || distSqr >= R_MAX_SQR)
                continue;

              // More stable inverse square root
              float invDist = 1.0F / (std::sqrt(distSqr) + 0.1F); // Add small epsilon
              float distance = distSqr * invDist;

              const int type_j = particleData.types[j];
              const float interaction = (type_i < MAX_PARTICLE_TYPES && type_j <
              MAX_PARTICLE_TYPES)
                                            ? interactionLUT[type_i][type_j]
                                            : getInteractionStrength(type_i, type_j);

              const float normDist = distance * invRMax;
              float forceMag = Particle::calculateForce(normDist, interaction);

              // Clamp force magnitude
              forceMag = std::min(forceMag, MAX_FORCE);

              totalForce += distVec * (forceMag * invDist);
            }
          }
        }

        // Safe force smoothing with bounds checking
        if (i < previousForces.size()) {
          glm::vec2 smoothedForce = glm::mix(previousForces[i], totalForce * R_MAX,
          SMOOTH_FACTOR); particleData.forces[i] = smoothedForce; previousForces[i] =
          smoothedForce;
        } else {
          particleData.forces[i] = totalForce * R_MAX;
        }
      }
    }
  };

  // 16. Launch optimized threads with better work distribution
  const size_t minParticlesPerThread = 32;
  const size_t actualThreads =
      std::min(numThreads, std::max(1UL, activeParticles.size() / minParticlesPerThread));

  if (actualThreads > 1) {
    const size_t optimalChunkSize = (activeParticles.size() + actualThreads - 1) / actualThreads;

    for (size_t t = 0; t < actualThreads; ++t) {
      size_t start = t * optimalChunkSize;
      size_t end = std::min((t + 1) * optimalChunkSize, activeParticles.size());
      threads.emplace_back(processParticleChunk, start, end);
    }

    // Join threads
    for (auto &thread : threads) {
      thread.join();
    }
  } else {
    // For small particle counts, avoid thread overhead
    processParticleChunk(0, activeParticles.size());
  }

  // 17. Apply forces in a vectorizable manner
  threads.clear();

  auto applyForces = [&](size_t startIdx, size_t endIdx) {
    for (size_t idx = startIdx; idx < endIdx && idx < activeParticles.size(); ++idx) {
      const size_t i = activeParticles[idx];

      // Apply damping to current velocity
      particleData.velocities[i] *= DAMPING;

      // Add force contribution
      glm::vec2 deltaV = particleData.forces[i] * deltaTime;

      // Clamp velocity changes
      const float MAX_DELTA_V = 100.0F;
      float lengthSqr = glm::dot(deltaV, deltaV);
      if (lengthSqr > MAX_DELTA_V * MAX_DELTA_V) {
        deltaV *= MAX_DELTA_V / std::sqrt(lengthSqr);
      }

      particleData.velocities[i] += deltaV;
    }
  };

  // 18. Reuse the thread distribution logic
  if (actualThreads > 1) {
    const size_t optimalChunkSize = (activeParticles.size() + actualThreads - 1) / actualThreads;

    for (size_t t = 0; t < actualThreads; ++t) {
      size_t start = t * optimalChunkSize;
      size_t end = std::min((t + 1) * optimalChunkSize, activeParticles.size());
      threads.emplace_back(applyForces, start, end);
    }

    // Join threads
    for (auto &thread : threads) {
      thread.join();
    }
  } else {
    // For small particle counts, avoid thread overhead
    applyForces(0, activeParticles.size());
  }

  // 19. Copy back results to AOS structure in a vectorizable loop
  for (size_t idx = 0; idx < activeParticles.size(); ++idx) {
    const size_t i = activeParticles[idx];
    particles[i].setVel(particleData.velocities[i]);
  }
}
```