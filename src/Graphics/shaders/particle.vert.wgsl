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
