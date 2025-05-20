#version 410 core

in vec2 TexCoord;
in vec4 Color;
in float Active;
out vec4 FragColor;

void main() {
    if (Active < 0.5) 
        discard;
    
    vec2 center = vec2(0.5, 0.5);
    float dist = length(TexCoord - center) * 2.0;
    
    float glow = 1.0 - dist * 0.8;
    
    vec3 innerColor = Color.rgb * 1.3;
    vec3 outerColor = Color.rgb * 0.7;
    vec3 finalColor = mix(innerColor, outerColor, dist);
    
    float alpha = 1.0 - smoothstep(0.8, 1.0, dist);

    if (dist > 1.0) 
        discard;
    
    FragColor = vec4(finalColor, alpha * Color.a);
}