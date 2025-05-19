#pragma once

#ifndef QUADTREE_H
#define QUADTREE_H

#include <algorithm>
#include <array>
#include <cmath>
#include <glm/glm.hpp>
#include <vector>

// Forward declarations
class Particle;
struct QuadtreeAABB;
struct QuadtreeNode;

// Particle class definition
class Particle {
public:
  glm::vec2 position;
  glm::vec2 velocity;
  glm::vec2 acceleration;
  float mass;

  Particle(const glm::vec2 &pos, float m = 1.0F)
      : position(pos), velocity(0.0F), acceleration(0.0F), mass(m) {}

  void update(float dt) {
    velocity += acceleration * dt;
    position += velocity * dt;
    acceleration = glm::vec2(0.0F); // Reset acceleration
  }
};

// AABB (Axis-Aligned Bounding Box) for Quadtree
struct QuadtreeAABB {
  glm::vec2 center;
  float halfDimension;

  QuadtreeAABB() : center(0.0F), halfDimension(0.0) {}

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
    float quarterDimension = halfDimension * 0.5F;
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
  float totalMass = 0.0F;
  glm::vec2 centerOfMass = {0.0F, 0.0F};
  int particleCount = 0;

  // For leaf nodes: store multiple particles
  std::vector<Particle *> storedParticles;

  QuadtreeNode(const QuadtreeAABB &_bounds) : bounds(_bounds) {}

  bool isLeaf() const { return childrenStartIndex == -1; }
};

// The main Quadtree class
class Quadtree {
public:
  std::vector<QuadtreeNode> nodes;
  int rootNodeIndex = -1;

  // Configuration
  const int MAX_PARTICLES_PER_LEAF = 4; // Allow multiple particles per leaf for better performance
  const int MAX_DEPTH = 20;             // Max depth to prevent infinite subdivision
  const float MIN_NODE_SIZE = 1.0F;     // Minimum size of a node's halfDimension
  const float THETA = 0.5F; // Barnes-Hut approximation parameter (smaller = more accurate)

  Quadtree() = default;

  void initialize(const QuadtreeAABB &initialBounds) {
    nodes.clear();
    nodes.reserve(2048);               // Pre-allocate some space
    nodes.emplace_back(initialBounds); // Root node
    rootNodeIndex = 0;
  }

  void clearAndReset(const QuadtreeAABB &initialBounds) { initialize(initialBounds); }

  // Insert a particle into the quadtree
  bool insert(Particle *p) {
    if (rootNodeIndex == -1 || nodes.empty()) {
      // Auto-initialize with a reasonable default if not already initialized
      QuadtreeAABB defaultBounds({0, 0}, 1000); // Set a large default bound
      initialize(defaultBounds);
    }

    // Check if particle is within bounds
    if (!nodes[rootNodeIndex].bounds.contains(p->position)) {
      // Expand the quadtree to accommodate the particle
      expandRootToFit(p->position);
    }

    insertRecursive(rootNodeIndex, p, 0);
    return true;
  }

  // Rebuild the entire quadtree with the current set of particles
  // Call this after particles have moved significantly
  void rebuild(const std::vector<Particle *> &particles) {
    if (particles.empty()) {
      // Reset with a small default size
      clearAndReset(QuadtreeAABB({0, 0}, 10));
      return;
    }

    // Calculate bounds to contain all particles
    QuadtreeAABB newBounds = calculateBoundsForParticles(particles);

    // Add some padding to avoid frequent rebuilds
    newBounds.halfDimension *= 1.2F;

    // Reset tree with new bounds
    clearAndReset(newBounds);

    // Insert all particles
    for (Particle *p : particles) {
      insert(p);
    }
  }

  // Query for particles within a given range (AABB)
  void queryRange(const QuadtreeAABB &range, std::vector<Particle *> &foundParticles) const {
    if (rootNodeIndex == -1 || nodes.empty()) {
      return;
    }
    queryRangeRecursive(rootNodeIndex, range, foundParticles);
  }

  // For Barnes-Hut: Calculate force on a particle from the tree
  glm::vec2 calculateForceOnParticle(const Particle &targetParticle,
                                     float gravitationalConstant) const {
    if (rootNodeIndex == -1 || nodes.empty()) {
      return glm::vec2(0.0F);
    }

    glm::vec2 totalForce(0.0F);
    calculateForceRecursive(rootNodeIndex, targetParticle, gravitationalConstant, totalForce);
    return totalForce;
  }

  // Calculate forces for all particles using Barnes-Hut approximation
  void updateAllForces(std::vector<Particle *> &particles, float gravitationalConstant) const {
    for (Particle *p : particles) {
      p->acceleration = calculateForceOnParticle(*p, gravitationalConstant) / p->mass;
    }
  }

  // Debug visualization utilities
  void getAllNodeBounds(std::vector<QuadtreeAABB> &allBounds) const {
    allBounds.clear();
    for (const auto &node : nodes) {
      allBounds.push_back(node.bounds);
    }
  }

  void getAllNodeCentersOfMass(std::vector<glm::vec2> &centerPoints,
                               std::vector<float> &masses) const {
    centerPoints.clear();
    masses.clear();
    for (const auto &node : nodes) {
      if (!node.isLeaf() && node.particleCount > 0) {
        centerPoints.push_back(node.centerOfMass);
        masses.push_back(node.totalMass);
      }
    }
  }

  int getNodeCount() const { return static_cast<int>(nodes.size()); }

  int getMaxDepth() const {
    int maxDepth = 0;
    for (size_t i = 0; i < nodes.size(); ++i) {
      int depth = calculateNodeDepth(static_cast<int>(i));
      maxDepth = std::max(maxDepth, depth);
    }
    return maxDepth;
  }

private:
  static QuadtreeAABB calculateBoundsForParticles(const std::vector<Particle *> &particles) {
    if (particles.empty()) {
      return QuadtreeAABB({0, 0}, 10);
    }

    float minX = particles[0]->position.x;
    float maxX = particles[0]->position.x;
    float minY = particles[0]->position.y;
    float maxY = particles[0]->position.y;

    for (size_t i = 1; i < particles.size(); i++) {
      minX = std::min(minX, particles[i]->position.x);
      maxX = std::max(maxX, particles[i]->position.x);
      minY = std::min(minY, particles[i]->position.y);
      maxY = std::max(maxY, particles[i]->position.y);
    }

    glm::vec2 center((minX + maxX) * 0.5F, (minY + maxY) * 0.5F);
    float halfDim = std::max(maxX - center.x, maxY - center.y) * 1.05F; // Add small padding

    return QuadtreeAABB(center, halfDim);
  }

  void expandRootToFit(const glm::vec2 &position) {
    // Create a new, larger root node that contains both the old root and the new position
    QuadtreeNode &oldRoot = nodes[rootNodeIndex];

    // Determine which quadrant the old root will be in relative to the new root
    glm::vec2 newCenter = oldRoot.bounds.center;
    float newHalfDimension = oldRoot.bounds.halfDimension * 2.0F;

    // Adjust the center to ensure the position will be contained
    if (position.x < oldRoot.bounds.center.x - oldRoot.bounds.halfDimension) {
      newCenter.x += oldRoot.bounds.halfDimension;
    } else if (position.x > oldRoot.bounds.center.x + oldRoot.bounds.halfDimension) {
      newCenter.x -= oldRoot.bounds.halfDimension;
    }

    if (position.y < oldRoot.bounds.center.y - oldRoot.bounds.halfDimension) {
      newCenter.y += oldRoot.bounds.halfDimension;
    } else if (position.y > oldRoot.bounds.center.y + oldRoot.bounds.halfDimension) {
      newCenter.y -= oldRoot.bounds.halfDimension;
    }

    // Create new root
    QuadtreeAABB newBounds(newCenter, newHalfDimension);

    // Save old root data
    int oldRootIndex = rootNodeIndex;
    QuadtreeNode oldRootCopy = oldRoot;

    // Reset the root
    clearAndReset(newBounds);

    // If old root had particles, insert them into the new tree
    if (oldRootCopy.storedParticles.size() > 0) {
      for (Particle *p : oldRootCopy.storedParticles) {
        insert(p);
      }
    }

    // Recursively copy old tree structure if it had children
    if (!oldRootCopy.isLeaf()) {
      copyChildrenFromOldTree(oldRootCopy, oldRootIndex);
    }
  }

  void copyChildrenFromOldTree(const QuadtreeNode &oldNode, int oldNodeIndex) {
    if (oldNode.isLeaf()) {
      // Insert all particles from old leaf node
      for (Particle *p : oldNode.storedParticles) {
        insert(p);
      }
      return;
    }

    // Process each child of the old node
    for (int i = 0; i < 4; ++i) {
      int oldChildIndex = oldNode.childrenStartIndex + i;
      copyChildrenFromOldTree(nodes[oldChildIndex], oldChildIndex);
    }
  }

  void insertRecursive(int nodeIndex, Particle *p, int depth) {
    QuadtreeNode &node = nodes[nodeIndex];

    // Update mass and center of mass for internal nodes as we insert
    node.totalMass += p->mass;
    node.centerOfMass =
        (node.centerOfMass * node.totalMass + p->position * p->mass) / (node.totalMass + p->mass);
    node.particleCount++;

    if (node.isLeaf()) {
      // If we haven't reached capacity, just add the particle
      if ((int)node.storedParticles.size() < MAX_PARTICLES_PER_LEAF || depth >= MAX_DEPTH ||
          node.bounds.halfDimension * 2.0F < MIN_NODE_SIZE * 2.0F) {

        node.storedParticles.push_back(p);
        return;
      }

      // Need to subdivide
      subdivideNode(nodeIndex);

      // Re-insert all particles into the children
      for (Particle *existingParticle : node.storedParticles) {
        int quadrant = node.bounds.getQuadrant(existingParticle->position);
        insertRecursive(node.childrenStartIndex + quadrant, existingParticle, depth + 1);
      }

      // Clear the particles from this node as it's no longer a leaf
      node.storedParticles.clear();

      // Insert the new particle into appropriate child
      int quadrant = node.bounds.getQuadrant(p->position);
      insertRecursive(node.childrenStartIndex + quadrant, p, depth + 1);
    } else {
      // Internal node, insert into appropriate child
      int quadrant = node.bounds.getQuadrant(p->position);
      insertRecursive(node.childrenStartIndex + quadrant, p, depth + 1);
    }
  }

  void subdivideNode(int nodeIndex) {
    QuadtreeNode &node = nodes[nodeIndex];
    if (!node.isLeaf() || node.bounds.halfDimension * 2.0F < MIN_NODE_SIZE * 2.0F) {
      // Already subdivided or too small to subdivide
      return;
    }

    node.childrenStartIndex = static_cast<int>(nodes.size()); // Children will be added at the end
    std::array<QuadtreeAABB, 4> childrenAABBs = node.bounds.subdivide();

    for (int i = 0; i < 4; ++i) {
      nodes.emplace_back(childrenAABBs[i]);
    }
  }

  void queryRangeRecursive(int nodeIndex, const QuadtreeAABB &queryRange,
                           std::vector<Particle *> &foundParticles) const {
    const QuadtreeNode &node = nodes[nodeIndex];

    // If the node's bounds don't intersect the query range, prune this branch
    if (!node.bounds.intersects(queryRange)) {
      return;
    }

    if (node.isLeaf()) {
      for (Particle *p : node.storedParticles) {
        if (queryRange.contains(p->position)) {
          foundParticles.push_back(p);
        }
      }
    } else { // Internal node
      for (int i = 0; i < 4; ++i) {
        queryRangeRecursive(node.childrenStartIndex + i, queryRange, foundParticles);
      }
    }
  }

  void calculateForceRecursive(int nodeIndex, const Particle &targetParticle, float G,
                               glm::vec2 &force) const {
    const QuadtreeNode &node = nodes[nodeIndex];

    // Skip empty nodes
    if (node.particleCount == 0) {
      return;
    }

    // Calculate distance between target and node's center of mass
    glm::vec2 direction = node.centerOfMass - targetParticle.position;
    float distanceSquared = glm::dot(direction, direction);

    // Avoid self-interaction or division by zero
    if (distanceSquared <= 0.0001F) {
      return;
    }

    if (node.isLeaf() || (node.bounds.halfDimension * 2.0F) / sqrt(distanceSquared) < THETA) {

      // Use approximation: treat node as a single particle at its center of mass
      float distance = sqrt(distanceSquared);

      // Normalize direction
      direction /= distance;

      // Calculate gravitational force: F = G * m1 * m2 / r^2
      float forceMagnitude = G * node.totalMass * targetParticle.mass / distanceSquared;

      // Apply soft clamping to avoid extreme forces at very small distances
      const float maxForce = 1000.0F;
      if (forceMagnitude > maxForce) {
        forceMagnitude = maxForce + log(forceMagnitude / maxForce);
      }

      force += direction * forceMagnitude;
    } else {
      // Node is too close for approximation, recursively compute forces from children
      for (int i = 0; i < 4; ++i) {
        calculateForceRecursive(node.childrenStartIndex + i, targetParticle, G, force);
      }
    }
  }

  int calculateNodeDepth(int nodeIndex) const {
    int depth = 0;
    int currentIndex = nodeIndex;

    // Walk up the tree to find the depth
    while (currentIndex != rootNodeIndex) {
      depth++;
      // Find parent (slower in a flat array representation)
      bool foundParent = false;
      for (size_t i = 0; i < nodes.size(); ++i) {
        if (!nodes[i].isLeaf() && currentIndex >= nodes[i].childrenStartIndex &&
            currentIndex < nodes[i].childrenStartIndex + 4) {
          currentIndex = static_cast<int>(i);
          foundParent = true;
          break;
        }
      }

      if (!foundParent) {
        break; // Error case
      }
    }

    return depth;
  }
};

// Helper class to track particle statistics
class QuadtreeStats {
public:
  static void collectStats(const Quadtree &quadtree, int &nodeCount, int &maxDepth,
                           int &particleCount) {
    nodeCount = quadtree.getNodeCount();
    maxDepth = quadtree.getMaxDepth();

    particleCount = 0;
    for (const auto &node : quadtree.nodes) {
      if (node.isLeaf()) {
        particleCount += static_cast<int>(node.storedParticles.size());
      }
    }
  }
};

#endif // QUADTREE_H