#ifndef OPTIMIZED_QUADTREE_H
#define OPTIMIZED_QUADTREE_H

#include <algorithm>
#include <array>
#include <cmath>
#include <glm/glm.hpp>
#include <memory>
#include <numeric>
#include <omp.h>
#include <simd/simd.h>
#include <vector>

// Forward declarations
struct Particle;
struct QuadtreeAABB;
struct QuadtreeNode;
class MemoryPool;

// Memory alignment for SIMD operations (16-byte for M2's NEON)
constexpr ssize_t ALIGNMENT = 16;

// Custom aligned allocator for SIMD
template <typename T, ssize_t Alignment> class aligned_allocator {
public:
  using value_type = T;
  using pointer = T *;
  using const_pointer = const T *;
  using reference = T &;
  using const_reference = const T &;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using propagate_on_container_move_assignment = std::true_type;
  using is_always_equal = std::true_type;

  template <typename U> struct rebind {
    using other = aligned_allocator<U, Alignment>;
  };

  aligned_allocator() noexcept = default;
  template <typename U>
  aligned_allocator(const aligned_allocator<U, Alignment> & /*unused*/) noexcept {}

  pointer allocate(size_type n) {
    if (n > std::numeric_limits<size_type>::max() / sizeof(T)) {
      throw std::bad_array_new_length();
    }
    if (n == 0) {
      return nullptr;
    }

    // Calculate total size needed including alignment padding
    size_type size = n * sizeof(T);
    void *ptr = nullptr;

#if defined(_MSC_VER)
    ptr = _aligned_malloc(size, Alignment);
    if (!ptr)
      throw std::bad_alloc();
#else
    if (posix_memalign(&ptr, Alignment, size) != 0) {
      throw std::bad_alloc();
    }
#endif

    return static_cast<pointer>(ptr);
  }

  void deallocate(pointer p, size_type /*unused*/) noexcept {
#if defined(_MSC_VER)
    _aligned_free(p);
#else
    free(p);
#endif
  }

  // Perfect forwarding for constructing objects
  template <typename U, typename... Args> void construct(U *p, Args &&...args) {
    ::new (static_cast<void *>(p)) U(std::forward<Args>(args)...);
  }

  template <typename U> void destroy(U *p) noexcept { p->~U(); }

  // Comparison operators
  bool operator==(const aligned_allocator & /*unused*/) const noexcept { return true; }
  bool operator!=(const aligned_allocator & /*unused*/) const noexcept { return false; }

  // Get alignment
  static constexpr size_t alignment() noexcept { return Alignment; }
};

// Structure-of-Arrays (SoA) layout for particles
struct ParticleSystem {
  // Aligned vectors for SIMD operations
  std::vector<float, aligned_allocator<float, ALIGNMENT>> posX;
  std::vector<float, aligned_allocator<float, ALIGNMENT>> posY;
  std::vector<float, aligned_allocator<float, ALIGNMENT>> velX;
  std::vector<float, aligned_allocator<float, ALIGNMENT>> velY;
  std::vector<float, aligned_allocator<float, ALIGNMENT>> accX;
  std::vector<float, aligned_allocator<float, ALIGNMENT>> accY;
  std::vector<float, aligned_allocator<float, ALIGNMENT>> mass;
  size_t count = 0;

  // Add a new particle
  ssize_t addParticle(const glm::vec2 &position, float particleMass) {
    if (count >= posX.size()) {
      // Grow arrays if needed
      size_t newCapacity = std::max(size_t(64), count * 2);
      posX.resize(newCapacity);
      posY.resize(newCapacity);
      velX.resize(newCapacity);
      velY.resize(newCapacity);
      accX.resize(newCapacity);
      accY.resize(newCapacity);
      mass.resize(newCapacity);
    }

    size_t idx = count++;
    posX[idx] = position.x;
    posY[idx] = position.y;
    velX[idx] = 0.0F;
    velY[idx] = 0.0F;
    accX[idx] = 0.0F;
    accY[idx] = 0.0F;
    mass[idx] = particleMass;
    return idx;
  }

  // Get particle as a structure (for compatibility)
  Particle getParticle(size_t index) const;

  // Update particles with leapfrog integration
  void updateLeapfrog(float dt) {
    float *vx = velX.data();
    float *vy = velY.data();
    float *ax = accX.data();
    float *ay = accY.data();
    float *px = posX.data();
    float *py = posY.data();

// First half of velocity update (v += 0.5*a*dt)
#pragma omp parallel for simd aligned(vx, vy, ax, ay : ALIGNMENT)
    for (size_t i = 0; i < count; ++i) {
      vx[i] += 0.5F * ax[i] * dt;
      vy[i] += 0.5F * ay[i] * dt;
    }

// Position update (x += v*dt)
#pragma omp parallel for simd aligned(px, py, vx, vy : ALIGNMENT)
    for (size_t i = 0; i < count; ++i) {
      px[i] += vx[i] * dt;
      py[i] += vy[i] * dt;
    }

// Clear accelerations for next step
#pragma omp parallel for simd aligned(ax, ay : ALIGNMENT)
    for (size_t i = 0; i < count; ++i) {
      ax[i] = 0.0F;
      ay[i] = 0.0F;
    }
  }

  // Complete the leapfrog step after forces are computed
  void finalizeLeapfrog(float dt) {
    float *vx = velX.data();
    float *vy = velY.data();
    float *ax = accX.data();
    float *ay = accY.data();

// Second half of velocity update (v += 0.5*a*dt)
#pragma omp parallel for simd aligned(vx, vy, ax, ay : ALIGNMENT)
    for (size_t i = 0; i < count; ++i) {
      vx[i] += 0.5F * ax[i] * dt;
      vy[i] += 0.5F * ay[i] * dt;
    }
  }

  // Reserve space for N particles
  void reserve(size_t n) {
    posX.reserve(n);
    posY.reserve(n);
    velX.reserve(n);
    velY.reserve(n);
    accX.reserve(n);
    accY.reserve(n);
    mass.reserve(n);
  }

  void clear() { count = 0; }
};

// Traditional Particle struct for compatibility
struct Particle {
  glm::vec2 position;
  glm::vec2 velocity;
  glm::vec2 acceleration;
  float mass;
  size_t index; // Index in the SoA arrays

  Particle(const glm::vec2 &pos, float m = 1.0f)
      : position(pos), velocity(0.0f), acceleration(0.0f), mass(m), index(0) {}
};

// Define the implementation for getParticle
inline Particle ParticleSystem::getParticle(size_t index) const {
  Particle p({posX[index], posY[index]}, mass[index]);
  p.velocity = {velX[index], velY[index]};
  p.acceleration = {accX[index], accY[index]};
  p.index = index;
  return p;
}

// Memory pool for quadtree nodes
class MemoryPool {
private:
  std::vector<char *> blocks;
  size_t blockSize = 65536; // 64KB blocks
  size_t nodeSize;
  size_t nodesPerBlock;
  std::vector<void *> freeList;

public:
  MemoryPool(size_t elemSize) : nodeSize(std::max(elemSize, sizeof(void *))) {
    nodesPerBlock = blockSize / nodeSize;
  }

  ~MemoryPool() {
    for (auto *block : blocks) {
      free(block);
    }
  }

  void *allocate() {
    if (freeList.empty()) {
      // Allocate a new block
      char *block = nullptr;
      if (posix_memalign((void **)&block, ALIGNMENT, blockSize) != 0) {
        throw std::bad_alloc();
      }
      blocks.push_back(block);

      // Add all nodes in the block to the free list
      for (size_t i = 0; i < nodesPerBlock; ++i) {
        freeList.push_back(block + (i * nodeSize));
      }
    }

    void *ptr = freeList.back();
    freeList.pop_back();
    return ptr;
  }

  void deallocate(void *ptr) { freeList.push_back(ptr); }

  void reset() {
    freeList.clear();
    for (auto block : blocks) {
      for (size_t i = 0; i < nodesPerBlock; ++i) {
        freeList.push_back(block + (i * nodeSize));
      }
    }
  }
};

// AABB (Axis-Aligned Bounding Box) for Quadtree
struct QuadtreeAABB {
  glm::vec2 center;
  float halfDimension;

  QuadtreeAABB() : center(0.0f), halfDimension(0.0f) {}

  QuadtreeAABB(const glm::vec2 &_center, float _halfDimension)
      : center(_center), halfDimension(_halfDimension) {}

  bool contains(const glm::vec2 &point) const {
    return (point.x >= center.x - halfDimension && point.x <= center.x + halfDimension &&
            point.y >= center.y - halfDimension && point.y <= center.y + halfDimension);
  }

  bool intersects(const QuadtreeAABB &other) const {
    return (std::abs(center.x - other.center.x) <= (halfDimension + other.halfDimension) &&
            std::abs(center.y - other.center.y) <= (halfDimension + other.halfDimension));
  }

  int getQuadrant(const glm::vec2 &point) const {
    if (point.x < center.x) {
      if (point.y < center.y) {
        return 0; // SW
      }
      return 2; // NW
    }
    if (point.y < center.y) {
      return 1; // SE
    }
    return 3; // NE
  }

  std::array<QuadtreeAABB, 4> subdivide() const {
    float quarterDimension = halfDimension * 0.5f;
    std::array<QuadtreeAABB, 4> children;

    // SW, SE, NW, NE
    children[0] =
        QuadtreeAABB({center.x - quarterDimension, center.y - quarterDimension}, quarterDimension);
    children[1] =
        QuadtreeAABB({center.x + quarterDimension, center.y - quarterDimension}, quarterDimension);
    children[2] =
        QuadtreeAABB({center.x - quarterDimension, center.y + quarterDimension}, quarterDimension);
    children[3] =
        QuadtreeAABB({center.x + quarterDimension, center.y + quarterDimension}, quarterDimension);

    return children;
  }
};

// Node for Quadtree
struct QuadtreeNode {
  QuadtreeAABB bounds;
  int childrenStartIndex = -1; // Index of first child (-1 if leaf)

  // For Barnes-Hut optimization
  float totalMass = 0.0f;
  glm::vec2 centerOfMass = {0.0f, 0.0f};
  int particleCount = 0;

  // For leaf nodes: store indices to particles in SoA layout
  std::vector<size_t> particleIndices;

  QuadtreeNode(const QuadtreeAABB &_bounds) : bounds(_bounds) {}

  bool isLeaf() const { return childrenStartIndex == -1; }
};

// The main Quadtree class with optimizations
class OptimizedQuadtree {
private:
  std::unique_ptr<MemoryPool> nodePool;
  std::vector<QuadtreeNode *> nodes;
  QuadtreeNode *rootNode = nullptr;

  // Configuration
  const int MAX_PARTICLES_PER_LEAF = 8; // Increased for better SIMD utilization
  const int MAX_DEPTH = 20;             // Max depth to prevent infinite subdivision
  const float MIN_NODE_SIZE = 1.0F;     // Minimum size of a node's halfDimension
  const float THETA = 0.5F;             // Barnes-Hut approximation parameter
  const float SOFTENING = 0.025F;       // Softening parameter to avoid singularities
  const float SOFTENING_SQUARED = SOFTENING * SOFTENING; // Precalculated for performance
  const float MAX_FORCE = 1000.0F;                       // Maximum force before soft clamping
  const float GROWTH_FACTOR = 1.05F;                     // Growth factor for bounding box

  // Reference to the particle data
  ParticleSystem *particles = nullptr;

public:
  OptimizedQuadtree() : nodePool(std::make_unique<MemoryPool>(sizeof(QuadtreeNode))) {}

  ~OptimizedQuadtree() { clear(); }

  void initialize(const QuadtreeAABB &initialBounds, ParticleSystem *particleSystem) {
    clear();
    particles = particleSystem;
    rootNode = new (nodePool->allocate()) QuadtreeNode(initialBounds);
    nodes.push_back(rootNode);
  }

  void clear() {
    for (auto *node : nodes) {
      node->~QuadtreeNode();
    }
    nodes.clear();
    rootNode = nullptr;
    if (nodePool) {
      nodePool->reset();
    }
  }

  // Build the quadtree from scratch with the current particles
  void buildTree() {
    if (particles == nullptr || particles->count == 0) {
      return;
    }

    // Calculate bounds
    QuadtreeAABB bounds = calculateBounds();
    initialize(bounds, particles);

    // Insert all particles
    std::vector<size_t> indices(particles->count);
    std::iota(indices.begin(), indices.end(), 0);

    // Recursively insert in parallel
    insertParticlesRecursive(rootNode, indices, 0);

    // Computing centerOfMass for nodes needs to happen after all insertions
    computeMassDistribution(rootNode);
  }

  // Barnes-Hut force calculation for all particles
  void computeForces(float gravitationalConstant) {
    if (rootNode == nullptr || particles == nullptr) {
      return;
    }

#pragma omp parallel for schedule(dynamic, 64)
    for (size_t i = 0; i < particles->count; ++i) {
      glm::vec2 force = computeForceOnParticle(i, gravitationalConstant);
      particles->accX[i] = force.x / particles->mass[i];
      particles->accY[i] = force.y / particles->mass[i];
    }
  }

  // Calculate force on a single particle
  glm::vec2 computeForceOnParticle(size_t particleIndex, float gravitationalConstant) const {
    glm::vec2 force(0.0f);
    glm::vec2 particlePos(particles->posX[particleIndex], particles->posY[particleIndex]);
    float particleMass = particles->mass[particleIndex];

    computeForceRecursive(rootNode, particlePos, particleMass, gravitationalConstant, force);
    return force;
  }

  // Visualization and debug functions
  void getAllNodeBounds(std::vector<QuadtreeAABB> &allBounds) const {
    allBounds.clear();
    for (auto *const node : nodes) {
      allBounds.push_back(node->bounds);
    }
  }

  void getAllNodeCentersOfMass(std::vector<glm::vec2> &centerPoints,
                               std::vector<float> &masses) const {
    centerPoints.clear();
    masses.clear();
    for (auto *const node : nodes) {
      if (!node->isLeaf() && node->particleCount > 0) {
        centerPoints.push_back(node->centerOfMass);
        masses.push_back(node->totalMass);
      }
    }
  }

  int getNodeCount() const { return static_cast<int>(nodes.size()); }

  int getMaxDepth() const {
    // In a flat array, depth calculation is more complex
    // Here's a simplified approach
    int maxDepth = 0;
    for (auto *node : nodes) {
      int depth = 0;
      QuadtreeNode *current = node;
      while (current != rootNode) {
        depth++;
        // Find parent
        current = findParent(current);
        if (!current)
          break;
      }
      maxDepth = std::max(maxDepth, depth);
    }
    return maxDepth;
  }

private:
  // Calculate bounds to contain all particles
  QuadtreeAABB calculateBounds() {
    if (particles->count == 0) {
      return QuadtreeAABB({0, 0}, 10.0F);
    }

    // Use first particle to initialize min/max
    simd_float4 minVals =
        simd_make_float4(particles->posX[0], particles->posY[0], FLT_MAX, FLT_MAX);
    simd_float4 maxVals =
        simd_make_float4(particles->posX[0], particles->posY[0], -FLT_MAX, -FLT_MAX);

    // Process particles in chunks of 4 for SIMD optimization
    const size_t vectorSize = 4;
    const size_t vectorizedSize = particles->count & ~(vectorSize - 1);

    // Main vectorized loop
    for (size_t i = 0; i < vectorizedSize; i += vectorSize) {
      simd_float4 posX = simd_make_float4(particles->posX[i], particles->posX[i + 1],
                                          particles->posX[i + 2], particles->posX[i + 3]);
      simd_float4 posY = simd_make_float4(particles->posY[i], particles->posY[i + 1],
                                          particles->posY[i + 2], particles->posY[i + 3]);

      minVals = simd_min(minVals, simd_make_float4(simd_reduce_min(posX), simd_reduce_min(posY),
                                                   minVals[2], minVals[3]));

      maxVals = simd_max(maxVals, simd_make_float4(simd_reduce_max(posX), simd_reduce_max(posY),
                                                   maxVals[2], maxVals[3]));
    }

    // Handle remaining particles
    for (size_t i = vectorizedSize; i < particles->count; ++i) {
      minVals = simd_min(
          minVals, simd_make_float4(particles->posX[i], particles->posY[i], FLT_MAX, FLT_MAX));
      maxVals = simd_max(
          maxVals, simd_make_float4(particles->posX[i], particles->posY[i], -FLT_MAX, -FLT_MAX));
    }

    float minX = minVals[0];
    float minY = minVals[1];
    float maxX = maxVals[0];
    float maxY = maxVals[1];

    glm::vec2 center((minX + maxX) * 0.5F, (minY + maxY) * 0.5F);
    float halfDim = std::max(maxX - center.x, maxY - center.y) * GROWTH_FACTOR;

    return QuadtreeAABB(center, halfDim);
  }

  // Insert particles recursively with in-place partitioning
  void insertParticlesRecursive(QuadtreeNode *node, std::vector<size_t> &indices, int depth) {
    if (indices.empty())
      return;

    if (indices.size() <= MAX_PARTICLES_PER_LEAF || depth >= MAX_DEPTH ||
        node->bounds.halfDimension <= MIN_NODE_SIZE) {

      // Store all particles in this leaf
      node->particleIndices = indices;
      return;
    }

    // Need to subdivide
    subdivideNode(node);

    // Quadrant buckets
    std::array<std::vector<size_t>, 4> quadrantParticles;

    // Partition particles into quadrants
    for (size_t idx : indices) {
      glm::vec2 pos(particles->posX[idx], particles->posY[idx]);
      int quadrant = node->bounds.getQuadrant(pos);
      quadrantParticles[quadrant].push_back(idx);
    }

    // Clear the original indices to free memory
    std::vector<size_t>().swap(indices);

    // Process each quadrant recursively
    for (int i = 0; i < 4; ++i) {
      if (!quadrantParticles[i].empty()) {
        insertParticlesRecursive(nodes[node->childrenStartIndex + i], quadrantParticles[i],
                                 depth + 1);
      }
    }
  }

  // Compute mass distribution bottom-up
  void computeMassDistribution(QuadtreeNode *node) {
    if (node->isLeaf()) {
      // Leaf node: compute from particles
      node->totalMass = 0.0F;
      node->centerOfMass = glm::vec2(0.0F);
      node->particleCount = static_cast<int>(node->particleIndices.size());

      for (size_t idx : node->particleIndices) {
        float m = particles->mass[idx];
        node->totalMass += m;
        node->centerOfMass += glm::vec2(particles->posX[idx], particles->posY[idx]) * m;
      }

      if (node->totalMass > 0.0F) {
        node->centerOfMass /= node->totalMass;
      }
    } else {
      // Internal node: compute from children
      node->totalMass = 0.0F;
      node->centerOfMass = glm::vec2(0.0F);
      node->particleCount = 0;

      for (int i = 0; i < 4; ++i) {
        QuadtreeNode *child = nodes[node->childrenStartIndex + i];
        computeMassDistribution(child);

        node->particleCount += child->particleCount;
        node->totalMass += child->totalMass;
        node->centerOfMass += child->centerOfMass * child->totalMass;
      }

      if (node->totalMass > 0.0F) {
        node->centerOfMass /= node->totalMass;
      }
    }
  }

  void subdivideNode(QuadtreeNode *node) {
    if (!node->isLeaf())
      return;

    std::array<QuadtreeAABB, 4> childrenAABBs = node->bounds.subdivide();
    node->childrenStartIndex = static_cast<int>(nodes.size());

    for (int i = 0; i < 4; ++i) {
      QuadtreeNode *childNode = new (nodePool->allocate()) QuadtreeNode(childrenAABBs[i]);
      nodes.push_back(childNode);
    }
  }

  void computeForceRecursive(QuadtreeNode *node, const glm::vec2 &particlePos, float particleMass,
                             float G, glm::vec2 &force) const {
    if (node == nullptr || node->particleCount == 0) {
      return;
    }

    // Calculate distance vector between particle and node's center of mass
    glm::vec2 direction = node->centerOfMass - particlePos;
    float distanceSquared = glm::dot(direction, direction) + SOFTENING_SQUARED;

    // Apply Barnes-Hut approximation criterion
    if (node->isLeaf() ||
        (node->bounds.halfDimension * 2.0F) / std::sqrt(distanceSquared) < THETA) {
      // Use approximation: treat node as a single particle at its center of mass
      float distance = std::sqrt(distanceSquared);

      // Calculate force using precomputed values where possible
      float invDist = 1.0F / distance;
      glm::vec2 normalizedDirection = direction * invDist;

      // Calculate gravitational force with softening: F = G * m1 * m2 / (r^2 + ε^2)
      float forceMagnitude = G * node->totalMass * particleMass * invDist * invDist;

      // Apply soft clamping to avoid extreme forces
      if (forceMagnitude > MAX_FORCE) {
        forceMagnitude = MAX_FORCE + std::log(forceMagnitude / MAX_FORCE);
      }

      force += normalizedDirection * forceMagnitude;
    } else {
      // Node is too close for approximation, recursively compute forces from children
      for (int i = 0; i < 4; ++i) {
        computeForceRecursive(nodes[node->childrenStartIndex + i], particlePos, particleMass, G,
                              force);
      }
    }
  }

  QuadtreeNode *findParent(QuadtreeNode *child) const {
    for (auto *potentialParent : nodes) {
      if (!potentialParent->isLeaf()) {
        int start = potentialParent->childrenStartIndex;
        if (start >= 0 && start + 3 < static_cast<int>(nodes.size())) {
          for (int i = 0; i < 4; ++i) {
            if (nodes[start + i] == child) {
              return potentialParent;
            }
          }
        }
      }
    }
    return nullptr;
  }
};

// Main simulation class to tie everything together
class GravitySimulation {
private:
  ParticleSystem particles;
  OptimizedQuadtree quadtree;
  float gravitationalConstant = 6.67430e-2F; // Adjusted for simulation scale
  float timeStep = 0.016F;                   // 16ms per frame (60fps)

public:
  GravitySimulation() {
    particles.reserve(10000); // Prepare for a reasonable number of particles

    // Initialize the quadtree with a reasonable bounding box
    QuadtreeAABB initialBounds({640.0F, 360.0F},
                               720.0F); // Center at screen center, large enough for 720p
    quadtree.initialize(initialBounds, &particles);
  }

  // Add a particle to the simulation
  size_t addParticle(const glm::vec2 &position, float mass = 1.0F) {
    return particles.addParticle(position, mass);
  }

  // Get the current particle count
  size_t getParticleCount() const { return particles.count; }

  // Access the particle system (read-only)
  const ParticleSystem &getParticles() const { return particles; }

  // Set gravitational constant
  void setGravitationalConstant(float G) { gravitationalConstant = G; }

  // Set simulation time step
  void setTimeStep(float dt) { timeStep = dt; }

  // Execute one simulation step with leapfrog integration
  void step() {
    if (particles.count == 0) {
      return;
    }

    // Leapfrog first half-step (update velocities)
    particles.updateLeapfrog(timeStep);

    // Rebuild the quadtree with current particle positions
    quadtree.buildTree();

    // Compute forces using Barnes-Hut algorithm
    quadtree.computeForces(gravitationalConstant);

    // Leapfrog second half-step (complete velocity update)
    particles.finalizeLeapfrog(timeStep);
  }

  // For visualization
  void getTreeVisualizationData(std::vector<QuadtreeAABB> &nodeBounds,
                                std::vector<glm::vec2> &centerOfMassPoints,
                                std::vector<float> &masses) {
    quadtree.getAllNodeBounds(nodeBounds);
    quadtree.getAllNodeCentersOfMass(centerOfMassPoints, masses);
  }
};

#endif // OPTIMIZED_QUADTREE_H