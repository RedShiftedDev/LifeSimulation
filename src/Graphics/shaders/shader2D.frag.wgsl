struct MaterialUniforms {
    color: vec3<f32>,
    alpha: f32,
}

@group(0) @binding(1) var<uniform> material: MaterialUniforms;

@fragment
fn fs_main() -> @location(0) vec4<f32> {
    return vec4<f32>(material.color, material.alpha);
}
