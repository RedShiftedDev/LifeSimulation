
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
