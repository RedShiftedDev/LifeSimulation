#ifndef EMBEDDED_SHADERS_H
#define EMBEDDED_SHADERS_H

#include <string>
#include <unordered_map>
#include <vector>

class EmbeddedShaders {
public:
  // Get shader source by name
  static std::string getShader(const std::string &name) {
    auto it = shaders.find(name);
    if (it != shaders.end()) {
      return it->second;
    }
    return ""; // Return empty string if shader not found
  }

  // Check if shader exists
  static bool hasShader(const std::string &name) { return shaders.contains(name); }

  // Get all available shader names
  static std::vector<std::string> getShaderNames() {
    std::vector<std::string> names;
    for (const auto &pair : shaders) {
      names.push_back(pair.first);
    }
    return names;
  }

private:
  static const std::unordered_map<std::string, std::string> shaders;
};

// Embedded shader sources
const std::unordered_map<std::string, std::string> EmbeddedShaders::shaders = {

    {"particle.frag.wgsl", R"(
// Input from vertex shader
struct FragmentInput {
    @location(0) tex_coord: vec2<f32>,
    @location(1) color: vec4<f32>,
    @location(2) is_active: f32,
};

@fragment
fn fs_main(in: FragmentInput) -> @location(0) vec4<f32> {
    if (in.is_active < 0.5) {
        discard;
    }

    // Assuming tex_coord are already in [0, 1] range for the quad
    let center = vec2<f32>(0.5, 0.5);
    // let dist_from_center = length(in.tex_coord - center); // This is distance from center of quad, normalized to [0, 0.5]
    // For a circle effect, we usually want distance from center normalized to [0, 1] across the quad radius
    let r_vec = in.tex_coord - center; // vector from center to current tex_coord
    let dist = length(r_vec * 2.0); // scales so that edge of quad is at distance 1.0 from center

    if (dist > 1.0) { // Discard pixels outside the circular particle
        discard;
    }

    // let glow = 1.0 - dist * 0.8; // Unused in the final FragColor in GLSL

    let particle_base_color_rgb = in.color.rgb;
    let particle_alpha = in.color.a;

    let inner_color = particle_base_color_rgb * 1.3;
    let outer_color = particle_base_color_rgb * 0.7;

    // Mix based on distance for a soft edge or radial gradient
    let final_rgb = mix(inner_color, outer_color, dist);

    // Smooth alpha falloff towards the edge
    let alpha_falloff = 1.0 - smoothstep(0.8, 1.0, dist);

    return vec4<f32>(final_rgb, alpha_falloff * particle_alpha);
}
)"},

    {"particle.vert.wgsl", R"(
// Uniforms
struct Uniforms {
    projection: mat4x4<f32>,
};
@group(0) @binding(0) var<uniform> ubo: Uniforms;

// Per-vertex attributes (from the quad)
struct VertexInput {
    @location(0) position: vec2<f32>, // Quad vertex position (originally aPos, but likely just 2D for a quad)
    @location(1) tex_coord: vec2<f32>,  // Quad texture coordinates
};

// Per-instance attributes
struct InstanceInput {
    @location(2) world_position_radius_is_active: vec4<f32>, // (world_pos.x, world_pos.y, radius, is_active_flag)
    @location(3) color_padding: vec4<f32>,              // (color.r, color.g, color.b, padding)
};

// Output to fragment shader
struct VertexOutput {
    @builtin(position) clip_position: vec4<f32>,
    @location(0) tex_coord: vec2<f32>,
    @location(1) color: vec4<f32>, // Using vec4 for color to include alpha
    @location(2) is_active: f32,      // Pass is_active flag
};

@vertex
fn vs_main(
    in_vertex: VertexInput,
    in_instance: InstanceInput
) -> VertexOutput {
    var out: VertexOutput;

    let instance_world_pos = in_instance.world_position_radius_is_active.xy;
    let instance_radius = in_instance.world_position_radius_is_active.z;
    out.is_active = in_instance.world_position_radius_is_active.w;
    let instance_color = in_instance.color_padding.rgb;

    // Scale quad vertex by radius and add instance world position
    // Assuming in_vertex.position is in [-0.5, 0.5] range for the quad
    let final_pos_2d = in_vertex.position * instance_radius + instance_world_pos;

    out.clip_position = ubo.projection * vec4<f32>(final_pos_2d, 0.0, 1.0);
    out.tex_coord = in_vertex.tex_coord;
    out.color = vec4<f32>(instance_color, 1.0); // Pass full RGBA

    return out;
}
)"},

    {"shader2D.frag.wgsl", R"(
struct MaterialUniforms {
    color: vec3<f32>,
    alpha: f32,
}

@group(0) @binding(1) var<uniform> material: MaterialUniforms;

@fragment
fn fs_main() -> @location(0) vec4<f32> {
    return vec4<f32>(material.color, material.alpha);
}
)"},

    {"shader2D.vert.wgsl", R"(
struct TransformUniforms {
    model: mat4x4<f32>,
    projection: mat4x4<f32>,
}

@group(0) @binding(0) var<uniform> transform: TransformUniforms;

struct VertexInput {
    @location(0) position: vec3<f32>,
}

struct VertexOutput {
    @builtin(position) clip_position: vec4<f32>,
}

@vertex
fn vs_main(input: VertexInput) -> VertexOutput {
    var out: VertexOutput;

    let world_position = transform.model * vec4<f32>(input.position, 1.0);
    out.clip_position = transform.projection * world_position;

    return out;
}
)"}

};

#endif // EMBEDDED_SHADERS_H
