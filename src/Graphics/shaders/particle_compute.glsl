#version 430 core
layout(local_size_x = 256) in;

// Particle structure definition
struct Particle {
    vec2 position;
    vec2 velocity;
    int type;
    float radius;
    float active;
    // Add padding to align to 32-byte boundary for better memory access
    float padding;
};

// Spatial grid structure for efficient neighbor search
struct GridCell {
    int count;
    int particleIndices[16]; // Fixed size for simplicity, adjust based on expected density
};

// Main particle buffer
layout(std430, binding = 0) buffer ParticleBuffer {
    Particle particles[];
};

// Interaction matrix
layout(std430, binding = 1) buffer InteractionMatrix {
    float interactions[];
};

// Spatial grid buffer - computed each frame
layout(std430, binding = 2) buffer GridBuffer {
    // Using a flat array to represent 2D grid
    // Access using cellIndex = (y * gridWidth + x)
    GridCell grid[];
};

// Uniforms
uniform float deltaTime;
uniform float rMax;
uniform float beta;
uniform int particleCount;
uniform int numTypes;
uniform int gridWidth;  // Number of cells in X direction
uniform int gridHeight; // Number of cells in Y direction
uniform vec2 worldMin;  // World boundaries min
uniform vec2 worldMax;  // World boundaries max

// Shared memory for local workgroup
shared Particle localParticles[256]; // One per thread in workgroup

// Helper functions
float calculateForce(float r_norm, float a) {
    if (r_norm < beta) {
        return r_norm / beta - 1.0f;
    } else if (beta < r_norm && r_norm < 1.0f) {
        return a * (1.0f - abs(2.0f * r_norm - 1.0f - beta) / (1.0f - beta));
    } else {
        return 0.0f;
    }
}

// Get grid cell indices for a position
ivec2 getGridCell(vec2 position) {
    vec2 normalized = (position - worldMin) / (worldMax - worldMin);
    return ivec2(int(normalized.x * gridWidth), int(normalized.y * gridHeight));
}

// Get linear index for a 2D grid cell
int getGridIndex(ivec2 cell) {
    // Clamp to grid boundaries
    cell = clamp(cell, ivec2(0), ivec2(gridWidth-1, gridHeight-1));
    return cell.y * gridWidth + cell.x;
}

// Phase 1: Build spatial grid
void buildGrid() {
    uint id = gl_GlobalInvocationID.x;
    if (id >= particleCount) return;
    
    // Only process active particles
    if (particles[id].active < 0.5) return;
    
    // Calculate grid cell for this particle
    ivec2 cell = getGridCell(particles[id].position);
    int cellIndex = getGridIndex(cell);
    
    // Add particle to grid cell (atomic to avoid race conditions)
    int slot = atomicAdd(grid[cellIndex].count, 1);
    
    // Ensure we don't exceed the cell capacity
    if (slot < 16) { // Must match array size in GridCell
        grid[cellIndex].particleIndices[slot] = int(id);
    }
}

// Phase 2: Calculate forces using spatial grid
void calculateForces() {
    uint id = gl_GlobalInvocationID.x;
    if (id >= particleCount || particles[id].active < 0.5) return;
    
    // Load particle data into local memory for faster access
    localParticles[gl_LocalInvocationID.x] = particles[id];
    
    // Ensure all threads have loaded their data
    barrier();
    
    vec2 pos_i = localParticles[gl_LocalInvocationID.x].position;
    int type_i = localParticles[gl_LocalInvocationID.x].type;
    vec2 totalForce = vec2(0.0);
    
    // Get grid cell for this particle
    ivec2 cell = getGridCell(pos_i);
    
    // Loop through neighboring cells (3x3 grid centered on particle's cell)
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            int cellIndex = getGridIndex(cell + ivec2(dx, dy));
            int cellCount = min(grid[cellIndex].count, 16); // Limit to array size
            
            // Process particles in this cell
            for (int i = 0; i < cellCount; i++) {
                int j = grid[cellIndex].particleIndices[i];
                
                // Skip self or inactive particles
                if (j == int(id) || particles[j].active < 0.5) continue;
                
                vec2 pos_j = particles[j].position;
                vec2 dist = pos_j - pos_i;
                
                // Fast distance check using dot product
                float distSqr = dot(dist, dist);
                float rMaxSqr = rMax * rMax;
                
                if (distSqr > 0.000001 && distSqr < rMaxSqr) {
                    float r = sqrt(distSqr);
                    int type_j = particles[j].type;
                    
                    // Get interaction strength (calculate index based on actual numTypes)
                    float interaction = interactions[type_i * numTypes + type_j];
                    
                    // Calculate force
                    float r_norm = r / rMax;
                    float forceMag = calculateForce(r_norm, interaction);
                    
                    // Avoid expensive normalization by using the already calculated distance
                    totalForce += (dist / r) * forceMag;
                }
            }
        }
    }
    
    // Apply force and update velocity (only velocity, position update is done in CPU code)
    particles[id].velocity += totalForce * rMax * deltaTime;
}

void main() {
    // First clear the grid
    if (gl_GlobalInvocationID.x < uint(gridWidth * gridHeight)) {
        grid[gl_GlobalInvocationID.x].count = 0;
    }
    
    // Ensure grid is cleared before proceeding
    barrier();
    
    // Phase 1: Build spatial grid
    buildGrid();
    
    // Ensure grid is fully built before calculating forces
    barrier();
    
    // Phase 2: Calculate forces
    calculateForces();
}